#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/metrics.h"
#include "task.h"
#include "sprite.h"

/* ---------------------------------------------------------------------------
 * Frame-budget instrumentation.
 *
 * Layout of the state mirrors the three levels a measurement exists at:
 *
 *   accumulators  written by call sites at any point in a frame (phase times via
 *                 Begin/End, gauges via SetGauge/AddGauge, counters via
 *                 AddCounter). Counters are cumulative for the whole run.
 *   sFrame        this frame's snapshot, assembled by Platform_MetricsFrameEnd.
 *   window / run  aggregates, rolled at the window boundary and at exit.
 *
 * Percentiles come from a fixed-bucket histogram rather than a sample array.
 * A sample array would need a bound on frames per window (a --metrics-window 60
 * run is ~3,600 frames) and would silently truncate past it; a histogram is
 * O(1) memory, has no such bound, and 100 us buckets are 0.6% of the frame
 * budget -- finer than anything this instrumentation can act on.
 * --------------------------------------------------------------------------- */

#define HIST_BUCKET_US 100
#define HIST_BUCKETS   1024
/* 102.4 ms ceiling; a longer frame clamps into the last bucket and is still an
 * overrun, which is the only thing that matters about it. */

static bool sEnabled;
static uint64_t (*sClock)(void);
static uint64_t sTicksPerSecond;
static double sWindowSeconds = 1.0;

/* --- per-frame accumulators ------------------------------------------------ */
static uint64_t sFrameStartTicks;
static uint64_t sPhaseStartTicks[METRICS_PHASE_COUNT];
static bool sPhaseOpen[METRICS_PHASE_COUNT];
static uint64_t sFramePhaseUs[METRICS_PHASE_COUNT];
static bool sFrameOpen;
static uint32_t sFrameGauges[METRICS_GAUGE_COUNT];

/* --- counters -------------------------------------------------------------- */
/* Cumulative for the whole run; the base values let the window and run summaries
 * report a per-frame rate. */
static uint32_t sCumulative[METRICS_COUNTER_COUNT];
static uint32_t sWindowCounterBase[METRICS_COUNTER_COUNT];

/* --- window ----------------------------------------------------------------- */
static uint64_t sWindowStartTicks;
static uint32_t sWindowFrames;
static uint64_t sWindowMicros;
static uint64_t sWindowPhaseUs[METRICS_PHASE_COUNT];
static uint64_t sWindowMinUs;
static uint64_t sWindowMaxUs;
static uint32_t sWindowOverruns;
static uint32_t sWindowHist[HIST_BUCKETS];
static uint32_t sWindowGauges[METRICS_GAUGE_COUNT];

/* --- run -------------------------------------------------------------------- */
static uint64_t sRunFrames;
static uint64_t sRunMicros;
static uint64_t sRunMinUs;
static uint64_t sRunMaxUs;
static uint32_t sRunOverruns;
static uint64_t sRunIdleUs;   // run's total idle, for the exit report's idle share
static uint32_t sRunHist[HIST_BUCKETS];

static struct MetricsSummary sSummary;
static FILE *sCsv;
static bool sReported;

/* ---------------------------------------------------------------------------
 * Names and gauge modes
 * --------------------------------------------------------------------------- */

static const char *const sPhaseNames[METRICS_PHASE_COUNT] =
{
    [METRICS_PHASE_LOGIC]   = "logic",
    [METRICS_PHASE_INPUT]   = "input",
    [METRICS_PHASE_VBLANK]  = "vblank",
    [METRICS_PHASE_AUDIO]   = "audio",
    [METRICS_PHASE_PANEL]   = "panel",
    [METRICS_PHASE_IDLE]    = "idle",
    [METRICS_PHASE_PPU]     = "ppu",
    [METRICS_PHASE_PRESENT] = "present",
};

static const char *const sGaugeNames[METRICS_GAUGE_COUNT] =
{
    [METRICS_GAUGE_TASKS]                = "tasks",
    [METRICS_GAUGE_SPRITES]              = "sprites",
    [METRICS_GAUGE_SPRITE_SCANLINES]        = "spriteScanlines",
    [METRICS_GAUGE_AFFINE_SPRITE_SCANLINES] = "affineSpriteScanlines",
    [METRICS_GAUGE_HBLANK_DMA]           = "hblankDma",
    [METRICS_GAUGE_AUDIO_QUEUE_MIN]      = "audioQueueMin",
    [METRICS_GAUGE_HEAP_USED]            = "heapUsed",
    [METRICS_GAUGE_HEAP_LARGEST_FREE]    = "heapLargestFree",
    [METRICS_GAUGE_HEAP_BLOCKS]          = "heapBlocks",
    [METRICS_GAUGE_HEAP_HIGH_WATER]      = "heapHighWater",
};

static const char *const sCounterNames[METRICS_COUNTER_COUNT] =
{
    [METRICS_COUNTER_ALLOCS]           = "allocs",
    [METRICS_COUNTER_FREES]            = "frees",
    [METRICS_COUNTER_AUDIO_UNDERRUNS]  = "audioUnderruns",
    [METRICS_COUNTER_PACER_RESYNCS]    = "pacerResyncs",
    [METRICS_COUNTER_AUDIO_IN_FRAMES]  = "audioInFrames",
    [METRICS_COUNTER_AUDIO_OUT_FRAMES] = "audioOutFrames",
};

/* A gauge's mode is the thing that makes it mean anything: summing "sprites in
 * use" across a frame would report double, and taking the last of "sprites
 * drawn" would drop the frames drawn before the call. The default arm is
 * deliberately the common case and tests/metrics.c asserts the mode table
 * partitions the gauge enum, so adding a gauge without choosing a mode fails a
 * test rather than silently summing. */
enum MetricsGaugeMode Platform_MetricsGaugeMode(enum MetricsGauge gauge)
{
    switch (gauge)
    {
    case METRICS_GAUGE_HBLANK_DMA:
    case METRICS_GAUGE_SPRITE_SCANLINES:
    case METRICS_GAUGE_AFFINE_SPRITE_SCANLINES:
        return METRICS_GAUGE_MODE_ADD;
    case METRICS_GAUGE_AUDIO_QUEUE_MIN:
        return METRICS_GAUGE_MODE_MIN;
    case METRICS_GAUGE_TASKS:
    case METRICS_GAUGE_SPRITES:
    case METRICS_GAUGE_HEAP_USED:
    case METRICS_GAUGE_HEAP_LARGEST_FREE:
    case METRICS_GAUGE_HEAP_BLOCKS:
    case METRICS_GAUGE_HEAP_HIGH_WATER:
        return METRICS_GAUGE_MODE_SET;
    default:
        return METRICS_GAUGE_MODE_SET;
    }
}

/* The engine's own gauges (task/sprite tables) are sampled once per frame from
 * the frame's end, so the tables' private storage stays in their own files. */
void Platform_MetricsSampleEngine(void)
{
    int i;
    uint32_t tasks = 0;
    uint32_t sprites = 0;

    if (!sEnabled)
        return;

    for (i = 0; i < NUM_TASKS; i++)
    {
        if (gTasks[i].isActive)
            tasks++;
    }

    for (i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse)
            sprites++;
    }

    Platform_MetricsSetGauge(METRICS_GAUGE_TASKS, tasks);
    Platform_MetricsSetGauge(METRICS_GAUGE_SPRITES, sprites);
}

const char *Platform_MetricsPhaseName(enum MetricsPhase phase)
{
    if ((int)phase < 0 || phase >= METRICS_PHASE_COUNT)
        return "?";
    return sPhaseNames[phase];
}

const char *Platform_MetricsGaugeName(enum MetricsGauge gauge)
{
    if ((int)gauge < 0 || gauge >= METRICS_GAUGE_COUNT)
        return "?";
    return sGaugeNames[gauge];
}

const char *Platform_MetricsCounterName(enum MetricsCounter counter)
{
    if ((int)counter < 0 || counter >= METRICS_COUNTER_COUNT)
        return "?";
    return sCounterNames[counter];
}

/* The last completed window. NULL until a window closes, so a caller (the HUD,
 * a test) can tell "no measurement yet" from "a measurement of zero". */
static bool sHaveSummary;

const struct MetricsSummary *Platform_MetricsSummary(void)
{
    return sHaveSummary ? &sSummary : NULL;
}

// A frame's busy time: everything the frame did except the pacer's sleep. Idle is
// the only phase that is not work, and the nested AUDIO phase is measured inside
// VBLANK, so subtracting idle counts every other phase exactly once. Summing the
// phases by hand instead would *under*count whatever is not attributed to a
// phase and would need AUDIO excluded as a special case; this cannot.
//
// This is what an overrun is measured against. The paced frame time cannot be,
// because the pacer deliberately sleeps to at least the period.
uint64_t Platform_MetricsBusyMicros(const struct MetricsFrame *frame)
{
    if (frame == NULL)
        return 0;
    if (frame->frameMicros <= frame->phaseMicros[METRICS_PHASE_IDLE])
        return 0;
    return frame->frameMicros - frame->phaseMicros[METRICS_PHASE_IDLE];
}

/* ---------------------------------------------------------------------------
 * Clock
 * --------------------------------------------------------------------------- */

static bool ClockUsable(void)
{
    return sEnabled && sClock != NULL && sTicksPerSecond != 0;
}

/* Differences (not absolute times) are converted, so the arithmetic stays in
 * integer ticks until the final division and a long uptime cannot lose
 * precision. */
static uint64_t TicksToMicros(uint64_t ticks)
{
    return (uint64_t)((double)ticks * 1000000.0 / (double)sTicksPerSecond);
}

/* ---------------------------------------------------------------------------
 * Aggregation helpers
 * --------------------------------------------------------------------------- */

static void HistogramAdd(uint32_t *hist, uint64_t micros)
{
    uint64_t bucket = micros / HIST_BUCKET_US;

    if (bucket >= HIST_BUCKETS)
        bucket = HIST_BUCKETS - 1;
    hist[bucket]++;
}

/* Bucket midpoint, in milliseconds. A histogram percentile is accurate to half
 * a bucket, which is 0.05 ms against a 16.7 ms budget. */
static double HistogramPercentile(const uint32_t *hist, uint64_t total, double fraction)
{
    uint64_t target;
    uint64_t seen = 0;
    int i;

    if (total == 0)
        return 0.0;

    target = (uint64_t)((double)total * fraction + 0.5);
    if (target < 1)
        target = 1;

    for (i = 0; i < HIST_BUCKETS; i++)
    {
        seen += hist[i];
        if (seen >= target)
            return (i * HIST_BUCKET_US + HIST_BUCKET_US / 2) / 1000.0;
    }

    return (double)(HIST_BUCKETS * HIST_BUCKET_US) / 1000.0;
}

/* UINT32_MAX marks a MIN gauge with no sample yet: a low-water mark starts at
 * the top of its range, and reporting "no sample" as ~0 would read as a
 * catastrophic queue starvation. */
static uint32_t GaugeValueOrZero(uint32_t value, enum MetricsGauge gauge)
{
    if (Platform_MetricsGaugeMode(gauge) == METRICS_GAUGE_MODE_MIN && value == UINT32_MAX)
        return 0;
    return value;
}

static void GaugesReset(uint32_t *gauges)
{
    int g;

    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
        gauges[g] = (Platform_MetricsGaugeMode((enum MetricsGauge)g) == METRICS_GAUGE_MODE_MIN)
                  ? UINT32_MAX : 0;
}

/* Folds this frame's gauges into a window accumulator: peak across the window
 * for SET/ADD, low-water across it for MIN. */
static void GaugeFold(uint32_t *dst, const uint32_t *frame)
{
    int g;

    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
    {
        if (Platform_MetricsGaugeMode((enum MetricsGauge)g) == METRICS_GAUGE_MODE_MIN)
        {
            if (frame[g] < dst[g])
                dst[g] = frame[g];
        }
        else if (frame[g] > dst[g])
        {
            dst[g] = frame[g];
        }
    }
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------------- */

bool Platform_MetricsIsEnabled(void)
{
    return sEnabled;
}

void Platform_MetricsSetEnabled(bool enabled)
{
    sEnabled = enabled;
}

void Platform_MetricsSetClock(uint64_t (*clock)(void), uint64_t ticksPerSecond)
{
    sClock = clock;
    sTicksPerSecond = ticksPerSecond;
    if (sWindowStartTicks == 0 && clock != NULL)
        sWindowStartTicks = clock();
}

void Platform_MetricsSetWindowSeconds(double seconds)
{
    sWindowSeconds = seconds > 0.0 ? seconds : 0.0;
}

void Platform_MetricsReset(void)
{
    sFrameOpen = false;
    memset(sPhaseOpen, 0, sizeof(sPhaseOpen));
    memset(sFramePhaseUs, 0, sizeof(sFramePhaseUs));
    memset(sCumulative, 0, sizeof(sCumulative));
    memset(sWindowCounterBase, 0, sizeof(sWindowCounterBase));
    memset(sWindowHist, 0, sizeof(sWindowHist));
    memset(sRunHist, 0, sizeof(sRunHist));
    memset(sWindowPhaseUs, 0, sizeof(sWindowPhaseUs));
    GaugesReset(sFrameGauges);
    GaugesReset(sWindowGauges);

    sWindowStartTicks = sClock ? sClock() : 0;
    sWindowFrames = 0;
    sWindowMicros = 0;
    sWindowMinUs = 0;
    sWindowMaxUs = 0;
    sWindowOverruns = 0;

    sRunFrames = 0;
    sRunMicros = 0;
    sRunMinUs = 0;
    sRunMaxUs = 0;
    sRunOverruns = 0;
    sRunIdleUs = 0;

    memset(&sSummary, 0, sizeof(sSummary));
    sHaveSummary = false;
    sReported = false;
}

/* ---------------------------------------------------------------------------
 * Call sites
 * --------------------------------------------------------------------------- */

void Platform_MetricsBegin(enum MetricsPhase phase)
{
    if (!ClockUsable() || (int)phase < 0 || phase >= METRICS_PHASE_COUNT)
        return;
    if (sPhaseOpen[phase])
        return; /* an unbalanced Begin would restart the phase clock mid-way */

    sPhaseOpen[phase] = true;
    sPhaseStartTicks[phase] = sClock();
}

void Platform_MetricsEnd(enum MetricsPhase phase)
{
    if (!ClockUsable() || (int)phase < 0 || phase >= METRICS_PHASE_COUNT)
        return;
    if (!sPhaseOpen[phase])
        return;

    sPhaseOpen[phase] = false;
    sFramePhaseUs[phase] += TicksToMicros(sClock() - sPhaseStartTicks[phase]);
}

void Platform_MetricsSetGauge(enum MetricsGauge gauge, uint32_t value)
{
    if (!sEnabled || (int)gauge < 0 || gauge >= METRICS_GAUGE_COUNT)
        return;

    switch (Platform_MetricsGaugeMode(gauge))
    {
    case METRICS_GAUGE_MODE_ADD:
        sFrameGauges[gauge] += value;
        break;
    case METRICS_GAUGE_MODE_MIN:
        if (value < sFrameGauges[gauge])
            sFrameGauges[gauge] = value;
        break;
    case METRICS_GAUGE_MODE_SET:
    default:
        sFrameGauges[gauge] = value;
        break;
    }
}

void Platform_MetricsAddGauge(enum MetricsGauge gauge, uint32_t delta)
{
    if (!sEnabled || (int)gauge < 0 || gauge >= METRICS_GAUGE_COUNT)
        return;
    if (Platform_MetricsGaugeMode(gauge) == METRICS_GAUGE_MODE_MIN)
        return; /* a delta is not a depth; callers use SetGauge for those */

    sFrameGauges[gauge] += delta;
}

void Platform_MetricsAddCounter(enum MetricsCounter counter, uint32_t delta)
{
    if (!sEnabled || (int)counter < 0 || counter >= METRICS_COUNTER_COUNT)
        return;

    sCumulative[counter] += delta;
}

void Platform_MetricsFrameBegin(void)
{
    /* The frame boundary belongs to the pacer, so the tick is recorded even when
     * the module is disabled: enabling it mid-run then has a sane start rather
     * than a start tick from the epoch. */
    sFrameStartTicks = sClock ? sClock() : 0;

    if (!ClockUsable())
        return;

    sFrameOpen = true;
    memset(sFramePhaseUs, 0, sizeof(sFramePhaseUs));
    memset(sPhaseOpen, 0, sizeof(sPhaseOpen));
    GaugesReset(sFrameGauges);
}

/* The CSV column order is the header Platform_MetricsOpenCsv wrote, and it is
 * built from the same name tables, so the two cannot drift. */
static void WriteCsvRow(const struct MetricsFrame *frame, uint64_t frameIndex)
{
    int p, g, c;

    if (sCsv == NULL)
        return;

    fprintf(sCsv, "%llu,%llu", (unsigned long long)frameIndex, (unsigned long long)frame->frameMicros);
    for (p = 0; p < METRICS_PHASE_COUNT; p++)
        fprintf(sCsv, ",%llu", (unsigned long long)frame->phaseMicros[p]);
    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
        fprintf(sCsv, ",%u", GaugeValueOrZero(frame->gauges[g], (enum MetricsGauge)g));
    for (c = 0; c < METRICS_COUNTER_COUNT; c++)
        fprintf(sCsv, ",%u", frame->counters[c]);
    fputc('\n', sCsv);
}

void Platform_MetricsFrameEnd(void)
{
    struct MetricsFrame frame;
    uint64_t micros;
    int p, g, c;

    /* Gauges describe the frame that just finished, so they are sampled here
     * rather than by the call sites: the task/sprite tables and the heap are
     * each private to their own file, and one sampler call keeps their walk out
     * of the frame loop's hot path. */
    Platform_MetricsSampleEngine();
    Platform_MetricsSampleHeap();

    if (!ClockUsable() || !sFrameOpen)
        return;

    sFrameOpen = false;

    micros = TicksToMicros(sClock() - sFrameStartTicks);

    memset(&frame, 0, sizeof(frame));
    frame.frameMicros = micros;
    for (p = 0; p < METRICS_PHASE_COUNT; p++)
        frame.phaseMicros[p] = sFramePhaseUs[p];
    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
        frame.gauges[g] = GaugeValueOrZero(sFrameGauges[g], (enum MetricsGauge)g);
    for (c = 0; c < METRICS_COUNTER_COUNT; c++)
        frame.counters[c] = sCumulative[c];

    Platform_MetricsRecordFrame(&frame);
    WriteCsvRow(&frame, sRunFrames - 1);
}

/* ---------------------------------------------------------------------------
 * Aggregation
 * --------------------------------------------------------------------------- */

static void PrintWindowLine(const struct MetricsSummary *s)
{
    int p, g;

    printf("[Metrics] %u frames  %.2f ms avg (min %.2f p50 %.2f p95 %.2f max %.2f)  "
           "idle %.1f%%  busy %.2f ms  over %u  |",
           s->frames, s->frameMs, s->minMs, s->p50Ms, s->p95Ms, s->maxMs,
           s->idlePercent, s->busyMs, s->overruns);

    for (p = 0; p < METRICS_PHASE_COUNT; p++)
    {
        if (p == METRICS_PHASE_IDLE)
            continue; /* already reported as a percentage above */
        if (p == METRICS_PHASE_AUDIO)
            continue; /* nested in vblank; printed as a sub-measurement below */
        printf(" %s %.2f", Platform_MetricsPhaseName((enum MetricsPhase)p), s->phaseMs[p]);
    }

    printf(" (audio %.2f)", s->phaseMs[METRICS_PHASE_AUDIO]);

    printf("  |");
    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
        printf(" %s %u", Platform_MetricsGaugeName((enum MetricsGauge)g), s->gauges[g]);

    printf("  | allocs %.1f frees %.1f underruns %.2f/frame\n",
           s->allocsPerFrame, s->freesPerFrame, s->audioUnderrunsPerFrame);
}

static void CloseWindow(void)
{
    int p, c;

    memset(&sSummary, 0, sizeof(sSummary));
    sSummary.frames = sWindowFrames;
    sSummary.frameMs = sWindowFrames ? (double)sWindowMicros / sWindowFrames / 1000.0 : 0.0;
    sSummary.minMs = sWindowFrames ? (double)sWindowMinUs / 1000.0 : 0.0;
    sSummary.maxMs = sWindowFrames ? (double)sWindowMaxUs / 1000.0 : 0.0;
    sSummary.p50Ms = HistogramPercentile(sWindowHist, sWindowFrames, 0.50);
    sSummary.p95Ms = HistogramPercentile(sWindowHist, sWindowFrames, 0.95);
    sSummary.overruns = sWindowOverruns;

    // A histogram percentile is a bucket midpoint, so it can land half a bucket
    // (50 us) above a maximum that sits on a bucket edge. Clamp it into the
    // observed range: a percentile outside [min, max] is not a percentile of the
    // sample, and a reader comparing p95 against max would rightly distrust it.
    if (sSummary.p50Ms < sSummary.minMs) sSummary.p50Ms = sSummary.minMs;
    if (sSummary.p50Ms > sSummary.maxMs) sSummary.p50Ms = sSummary.maxMs;
    if (sSummary.p95Ms < sSummary.p50Ms) sSummary.p95Ms = sSummary.p50Ms;
    if (sSummary.p95Ms > sSummary.maxMs) sSummary.p95Ms = sSummary.maxMs;

    for (p = 0; p < METRICS_PHASE_COUNT; p++)
        sSummary.phaseMs[p] = sWindowFrames ? (double)sWindowPhaseUs[p] / sWindowFrames / 1000.0 : 0.0;

    if (sSummary.frameMs > 0.0)
        sSummary.idlePercent = 100.0 * sSummary.phaseMs[METRICS_PHASE_IDLE] / sSummary.frameMs;

    // Busy is the frame minus the pacer's sleep -- the same quantity
    // Platform_MetricsBusyMicros computes per frame, so the HUD, the overrun
    // classification and the busy time reported all agree by construction.
    sSummary.busyMs = sSummary.frameMs - sSummary.phaseMs[METRICS_PHASE_IDLE];

    for (c = 0; c < METRICS_COUNTER_COUNT; c++)
    {
        uint32_t delta = sCumulative[c] - sWindowCounterBase[c];
        sWindowCounterBase[c] = sCumulative[c];

        if (c == METRICS_COUNTER_ALLOCS)
            sSummary.allocsPerFrame = sWindowFrames ? (double)delta / sWindowFrames : 0.0;
        else if (c == METRICS_COUNTER_FREES)
            sSummary.freesPerFrame = sWindowFrames ? (double)delta / sWindowFrames : 0.0;
        else if (c == METRICS_COUNTER_AUDIO_UNDERRUNS)
            sSummary.audioUnderrunsPerFrame = sWindowFrames ? (double)delta / sWindowFrames : 0.0;
    }

    // The summary's gauge array was zeroed by the memset above, but a MIN gauge
    // folds as a low-water mark and 0 is below every reading -- so it must be
    // seeded to its "no sample" sentinel first, exactly like a window's own
    // accumulator, or the window would always report an empty queue.
    // Seed then fold: a MIN gauge must start at its "no sample" sentinel, or a
    // zeroed array would pin it below every reading. After the fold, normalise
    // any gauge that never received a sample back to 0 so callers (the HUD, the
    // report, a test) never see the sentinel.
    GaugesReset(sSummary.gauges);
    GaugeFold(sSummary.gauges, sWindowGauges);
    for (p = 0; p < METRICS_GAUGE_COUNT; p++)
        sSummary.gauges[p] = GaugeValueOrZero(sSummary.gauges[p], (enum MetricsGauge)p);
    sHaveSummary = true;
}

static void ResetWindow(void)
{
    sWindowStartTicks = sClock ? sClock() : 0;
    sWindowFrames = 0;
    sWindowMicros = 0;
    sWindowMinUs = 0;
    sWindowMaxUs = 0;
    sWindowOverruns = 0;
    memset(sWindowHist, 0, sizeof(sWindowHist));
    memset(sWindowPhaseUs, 0, sizeof(sWindowPhaseUs));
    GaugesReset(sWindowGauges);
}

/* Publishes the current window on demand. The exit report needs this (a run
 * usually ends part-way through a window) and it makes a window's aggregate
 * readable without waiting on the clock, which is what the aggregation tests
 * do. Guarded against an empty window so a flush cannot publish zero frames. */
void Platform_MetricsFlushWindow(void)
{
    if (sWindowFrames == 0)
        return;

    CloseWindow();
    ResetWindow();
}

bool Platform_MetricsRecordFrame(const struct MetricsFrame *frame)
{
    bool rolled = false;
    int p;
    uint64_t elapsedTicks;

    if (frame == NULL)
        return false;

    for (p = 0; p < METRICS_PHASE_COUNT; p++)
        sWindowPhaseUs[p] += frame->phaseMicros[p];

    sWindowFrames++;
    sWindowMicros += frame->frameMicros;
    if (sWindowFrames == 1 || frame->frameMicros < sWindowMinUs)
        sWindowMinUs = frame->frameMicros;
    if (sWindowFrames == 1 || frame->frameMicros > sWindowMaxUs)
        sWindowMaxUs = frame->frameMicros;
    HistogramAdd(sWindowHist, frame->frameMicros);
    GaugeFold(sWindowGauges, frame->gauges);

    /* An overrun is *busy* time past the budget, not paced time past it. The
     * pacer targets at least the frame period and SDL_Delay sleeps in whole
     * milliseconds, so a healthy run's paced frame is routinely a fraction of a
     * millisecond longer than the nominal period -- counting that reports ~half
     * the frames as overruns on a machine that is 70% idle. What matters is
     * whether the work alone exceeded the budget, which is when the pacer's
     * resync branch fires and a frame is genuinely abandoned. */
    if (Platform_MetricsBusyMicros(frame) > METRICS_FRAME_BUDGET_MICROS)
        sWindowOverruns++;

    sRunFrames++;
    sRunMicros += frame->frameMicros;
    sRunIdleUs += frame->phaseMicros[METRICS_PHASE_IDLE];
    if (sRunFrames == 1 || frame->frameMicros < sRunMinUs)
        sRunMinUs = frame->frameMicros;
    if (sRunFrames == 1 || frame->frameMicros > sRunMaxUs)
        sRunMaxUs = frame->frameMicros;
    HistogramAdd(sRunHist, frame->frameMicros);

    if (Platform_MetricsBusyMicros(frame) > METRICS_FRAME_BUDGET_MICROS)
        sRunOverruns++;

    if (sWindowSeconds > 0.0 && sClock != NULL && sTicksPerSecond != 0)
    {
        elapsedTicks = sClock() - sWindowStartTicks;
        if ((double)elapsedTicks / (double)sTicksPerSecond >= sWindowSeconds)
        {
            CloseWindow();
            PrintWindowLine(&sSummary);
            rolled = true;
            ResetWindow();
        }
    }

    return rolled;
}

/* ---------------------------------------------------------------------------
 * CSV and reporting
 * --------------------------------------------------------------------------- */

bool Platform_MetricsOpenCsv(const char *path)
{
    int p, g, c;

    if (path == NULL)
        return false;

    if (sCsv != NULL)
    {
        fclose(sCsv);
        sCsv = NULL;
    }

    sCsv = fopen(path, "w");
    if (sCsv == NULL)
        return false;

    fprintf(sCsv, "frame,frame_us");
    for (p = 0; p < METRICS_PHASE_COUNT; p++)
        fprintf(sCsv, ",%s_us", Platform_MetricsPhaseName((enum MetricsPhase)p));
    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
        fprintf(sCsv, ",%s", Platform_MetricsGaugeName((enum MetricsGauge)g));
    for (c = 0; c < METRICS_COUNTER_COUNT; c++)
        fprintf(sCsv, ",%s_total", Platform_MetricsCounterName((enum MetricsCounter)c));
    fputc('\n', sCsv);

    return true;
}

void Platform_MetricsReport(void)
{
    int c;

    if (sReported)
        return;
    sReported = true;

    // A run usually ends part-way through a window, so publish what has been
    // measured rather than discarding it -- otherwise a short --boot-test would
    // report no summary at all.
    Platform_MetricsFlushWindow();
    if (sCsv != NULL)
    {
        fclose(sCsv);
        sCsv = NULL;
    }

    if (sRunFrames == 0)
        return;

    printf("[Metrics] run: %llu frames, %.2f s elapsed, %.2f ms/frame "
           "(min %.2f p50 %.2f p95 %.2f max %.2f), idle %.1f%%, overruns %u\n",
           (unsigned long long)sRunFrames, (double)sRunMicros / 1e6,
           (double)sRunMicros / (double)sRunFrames / 1000.0,
           (double)sRunMinUs / 1000.0,
           HistogramPercentile(sRunHist, sRunFrames, 0.50),
           HistogramPercentile(sRunHist, sRunFrames, 0.95),
           (double)sRunMaxUs / 1000.0,
           sRunMicros ? 100.0 * (double)sRunIdleUs / (double)sRunMicros : 0.0,
           sRunOverruns);

    printf("[Metrics] rates:");
    for (c = 0; c < METRICS_COUNTER_COUNT; c++)
        printf(" %s %.2f/frame", Platform_MetricsCounterName((enum MetricsCounter)c),
               (double)sCumulative[c] / (double)sRunFrames);
    printf("\n");
}
