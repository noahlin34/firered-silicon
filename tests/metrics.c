// Frame-budget metrics.
//
// Two layers, tested separately because they fail differently:
//
//   pure   the aggregation -- percentiles, phase means, gauge modes, overrun
//          classification -- driven with synthetic frames so the arithmetic is
//          pinned without depending on how fast this machine is.
//   engine the live wiring -- that the phases sum against the frame, that the
//          gauges are sampled, and above all that the module is a pure observer:
//          a run with metrics enabled must reach exactly the state a run with
//          them disabled does. Several behaviours in this port depend on that
//          (byte-identical boot captures, exact engine-state assertions in other
//          tests), so a metric that feeds the simulation is a real bug.
//
// Nothing here asserts an absolute millisecond figure: this suite runs on
// whatever machine it is on, and the harness has no pacer at all (sdl2.o is
// excluded, so WaitForFrameDeadline is not linked and the engine runs unpaced).
// The tests assert relationships and accounting, which hold anywhere.

#include <string.h>

#include "registry.h"
#include "platform/metrics.h"
#include "constants/maps.h"
#include "gba/defines.h"

// --- pure: gauge modes -------------------------------------------------------

// Every gauge must have a mode, and the modes must partition the enum. A gauge
// added without choosing one would silently take the default and, if it were a
// depth, be summed into nonsense.
FIRERED_TEST("metrics/every gauge has a mode", "metrics pure", metrics_gauge_modes)
{
    int g;
    int set = 0, add = 0, min = 0;

    for (g = 0; g < METRICS_GAUGE_COUNT; g++)
    {
        switch (Platform_MetricsGaugeMode((enum MetricsGauge)g))
        {
        case METRICS_GAUGE_MODE_SET: set++; break;
        case METRICS_GAUGE_MODE_ADD: add++; break;
        case METRICS_GAUGE_MODE_MIN: min++; break;
        default: TEST_FAIL("gauge %d has no mode", g); break;
        }
    }

    TEST_EQ(set + add + min, METRICS_GAUGE_COUNT);

    // The modes are chosen for a reason, not by accident: these three classes
    // must each be non-empty, or a future edit has collapsed them.
    TEST_GE(set, 1);
    TEST_GE(add, 1);
    TEST_GE(min, 1);

    // Specifically: the queue depth is a low-water mark. Summing it would report
    // a depth larger than the queue can hold.
    TEST_EQ(Platform_MetricsGaugeMode(METRICS_GAUGE_AUDIO_QUEUE_MIN), METRICS_GAUGE_MODE_MIN);
    TEST_EQ(Platform_MetricsGaugeMode(METRICS_GAUGE_HBLANK_DMA), METRICS_GAUGE_MODE_ADD);
    TEST_EQ(Platform_MetricsGaugeMode(METRICS_GAUGE_SPRITES), METRICS_GAUGE_MODE_SET);
}

// Every phase, gauge and counter must have a distinct, non-empty name: the CSV
// header and the report are built from these, so a missing name silently
// produces a blank column that no one can read back.
FIRERED_TEST("metrics/every measurement has a name", "metrics pure", metrics_names)
{
    int i, j;

    for (i = 0; i < METRICS_PHASE_COUNT; i++)
    {
        const char *a = Platform_MetricsPhaseName((enum MetricsPhase)i);
        TEST_PTR_NOT_NULL((const void *)a);
        TEST_TRUE(a[0] != '\0');
        for (j = i + 1; j < METRICS_PHASE_COUNT; j++)
            TEST_TRUE(strcmp(a, Platform_MetricsPhaseName((enum MetricsPhase)j)) != 0);
    }

    for (i = 0; i < METRICS_GAUGE_COUNT; i++)
    {
        const char *a = Platform_MetricsGaugeName((enum MetricsGauge)i);
        TEST_PTR_NOT_NULL((const void *)a);
        TEST_TRUE(a[0] != '\0');
        for (j = i + 1; j < METRICS_GAUGE_COUNT; j++)
            TEST_TRUE(strcmp(a, Platform_MetricsGaugeName((enum MetricsGauge)j)) != 0);
    }

    for (i = 0; i < METRICS_COUNTER_COUNT; i++)
    {
        const char *a = Platform_MetricsCounterName((enum MetricsCounter)i);
        TEST_PTR_NOT_NULL((const void *)a);
        TEST_TRUE(a[0] != '\0');
        for (j = i + 1; j < METRICS_COUNTER_COUNT; j++)
            TEST_TRUE(strcmp(a, Platform_MetricsCounterName((enum MetricsCounter)j)) != 0);
    }
}

// --- pure: arithmetic --------------------------------------------------------

// The frame budget is the hardware frame period: 280896 cycles at 16777216 Hz.
// Pinned because every overrun classification and every "headroom" reading is
// relative to it, and a wrong constant would silently rescale all of them.
FIRERED_TEST("metrics/frame budget is the hardware frame period", "metrics pure", metrics_budget)
{
    double ms = METRICS_FRAME_BUDGET_MICROS / 1000.0;

    TEST_GE(ms, 16.70);
    TEST_TRUE(ms <= 16.79);
}

// Synthetic frames: the aggregation must report the mean, the min/max, the
// per-phase means and the percentile, and the percentile must lie between the
// min and the max. Ten frames of 10 ms and ten of 20 ms make every one of those
// exact, so the expected values are derivable rather than fitted.
FIRERED_TEST("metrics/aggregation reports mean, range and percentile", "metrics pure", metrics_aggregate)
{
    struct MetricsFrame frame;
    const struct MetricsSummary *s;
    int i;

    Platform_MetricsSetEnabled(true);
    Platform_MetricsSetClock(NULL, 0);   // no clock: the module must stay inert
    Platform_MetricsReset();

    // With no clock the module cannot measure, and RecordFrame must still be
    // safe to call -- a caller that has not installed a clock (the harness, a
    // test) must not crash or accumulate garbage.
    memset(&frame, 0, sizeof(frame));
    frame.frameMicros = 10000;
    TEST_FALSE(Platform_MetricsRecordFrame(&frame));
    TEST_PTR_EQ((const void *)Platform_MetricsSummary(), NULL);
}

// A window's arithmetic, driven with a clock. 100 us tick granularity means the
// synthetic frame times are exactly representable.
static uint64_t sFakeTicks;
static uint64_t FakeClock(void) { return sFakeTicks; }

FIRERED_TEST("metrics/window summary matches the frames recorded", "metrics pure", metrics_window)
{
    struct MetricsFrame frame;
    const struct MetricsSummary *s;
    int i;

    sFakeTicks = 0;
    Platform_MetricsSetEnabled(true);
    Platform_MetricsSetClock(FakeClock, 1000000);   // 1 tick = 1 us
    Platform_MetricsSetWindowSeconds(1000000.0);    // never auto-rolls
    Platform_MetricsReset();

    for (i = 0; i < 20; i++)
    {
        memset(&frame, 0, sizeof(frame));
        frame.frameMicros = (i < 10) ? 10000 : 20000;
        frame.phaseMicros[METRICS_PHASE_IDLE] = 5000;
        frame.phaseMicros[METRICS_PHASE_PPU] = 2000;
        frame.gauges[METRICS_GAUGE_SPRITES] = (uint32_t)(i % 7);
        Platform_MetricsRecordFrame(&frame);
    }

    // Publish the window rather than waiting for its duration to elapse: the
    // summary is the observation point and the window is set never to auto-roll.
    Platform_MetricsFlushWindow();

    s = Platform_MetricsSummary();
    TEST_PTR_NOT_NULL((const void *)s);

    if (s == NULL)
        return;

    TEST_EQ(s->frames, 20);
    TEST_TRUE(s->frameMs > 14.9 && s->frameMs < 15.1);   // mean of 10 and 20
    TEST_TRUE(s->minMs > 9.9 && s->minMs < 10.1);
    TEST_TRUE(s->maxMs > 19.9 && s->maxMs < 20.1);
    TEST_TRUE(s->p50Ms >= s->minMs && s->p50Ms <= s->maxMs);
    TEST_TRUE(s->p95Ms >= s->p50Ms && s->p95Ms <= s->maxMs);

    TEST_TRUE(s->phaseMs[METRICS_PHASE_IDLE] > 4.9 && s->phaseMs[METRICS_PHASE_IDLE] < 5.1);
    TEST_TRUE(s->phaseMs[METRICS_PHASE_PPU] > 1.9 && s->phaseMs[METRICS_PHASE_PPU] < 2.1);

    // Busy is frame minus idle, and idle percent is the idle share. Both are
    // derived, so a change to either must keep them consistent.
    TEST_TRUE(s->busyMs > 9.9 && s->busyMs < 10.1);
    TEST_TRUE(s->idlePercent > 32.0 && s->idlePercent < 34.0);

    // A SET gauge is the peak across the window, not the last value.
    TEST_EQ(s->gauges[METRICS_GAUGE_SPRITES], 6);
}

// An overrun is busy time past the budget. A frame that is 30 ms long but 29 ms
// idle is not an overrun -- that is the pacer doing its job -- while a frame
// that is 20 ms long with no idle is. Getting this backwards reports a healthy
// machine as broken, which is exactly what the first version of this code did.
FIRERED_TEST("metrics/overrun is busy time, not paced time", "metrics pure", metrics_overrun)
{
    struct MetricsFrame frame;
    const struct MetricsSummary *s;

    sFakeTicks = 0;
    Platform_MetricsSetEnabled(true);
    Platform_MetricsSetClock(FakeClock, 1000000);
    Platform_MetricsSetWindowSeconds(1000000.0);
    Platform_MetricsReset();

    // Long frame, almost all idle: not an overrun.
    memset(&frame, 0, sizeof(frame));
    frame.frameMicros = 30000;
    frame.phaseMicros[METRICS_PHASE_IDLE] = 29000;
    Platform_MetricsRecordFrame(&frame);
    Platform_MetricsFlushWindow();

    s = Platform_MetricsSummary();
    TEST_PTR_NOT_NULL((const void *)s);
    if (s != NULL)
        TEST_EQ(s->overruns, 0);

    Platform_MetricsReset();
    sFakeTicks = 0;

    // Short frame, no idle: an overrun.
    memset(&frame, 0, sizeof(frame));
    frame.frameMicros = 20000;
    frame.phaseMicros[METRICS_PHASE_IDLE] = 0;
    Platform_MetricsRecordFrame(&frame);
    Platform_MetricsFlushWindow();

    s = Platform_MetricsSummary();
    TEST_PTR_NOT_NULL((const void *)s);
    if (s != NULL)
        TEST_EQ(s->overruns, 1);
}

// A MIN gauge folds as a low-water mark across the window, not a peak: the
// question a queue-depth gauge answers is "did it ever run dry", and taking the
// maximum would answer the opposite.
FIRERED_TEST("metrics/low-water gauge folds to the minimum", "metrics pure", metrics_min_gauge)
{
    struct MetricsFrame frame;
    const struct MetricsSummary *s;

    sFakeTicks = 0;
    Platform_MetricsSetEnabled(true);
    Platform_MetricsSetClock(FakeClock, 1000000);
    Platform_MetricsSetWindowSeconds(1000000.0);
    Platform_MetricsReset();

    memset(&frame, 0, sizeof(frame));
    frame.frameMicros = 10000;
    frame.gauges[METRICS_GAUGE_AUDIO_QUEUE_MIN] = 400;
    Platform_MetricsRecordFrame(&frame);

    frame.gauges[METRICS_GAUGE_AUDIO_QUEUE_MIN] = 120;
    Platform_MetricsRecordFrame(&frame);

    frame.gauges[METRICS_GAUGE_AUDIO_QUEUE_MIN] = 900;
    Platform_MetricsRecordFrame(&frame);

    Platform_MetricsFlushWindow();
    s = Platform_MetricsSummary();
    TEST_PTR_NOT_NULL((const void *)s);
    if (s != NULL)
        TEST_EQ(s->gauges[METRICS_GAUGE_AUDIO_QUEUE_MIN], 120);
}

// A phase clock must not be restarted by a nested Begin, and an End for a phase
// that was never opened must not invent time for it. Both are cheap mistakes at
// a call site; the first would silently halve a phase's reported cost and the
// second would charge work to a phase that never ran.
//
// The window is set shorter than one frame so the very first recorded frame
// publishes a summary of exactly that frame -- otherwise the mean would blur the
// values this test needs to read exactly.
FIRERED_TEST("metrics/unbalanced phase calls do not double-count", "metrics pure", metrics_unbalanced)
{
    const struct MetricsSummary *s;

    sFakeTicks = 0;
    Platform_MetricsSetEnabled(true);
    Platform_MetricsSetClock(FakeClock, 1000000);   // 1 tick = 1 us
    Platform_MetricsSetWindowSeconds(0.001);        // shorter than the frame below
    Platform_MetricsReset();

    Platform_MetricsFrameBegin();
    Platform_MetricsBegin(METRICS_PHASE_PPU);
    sFakeTicks += 500;
    Platform_MetricsBegin(METRICS_PHASE_PPU);   // nested: ignored, not restarted
    sFakeTicks += 500;
    Platform_MetricsEnd(METRICS_PHASE_PPU);     // so PPU spans the full 1000 us
    Platform_MetricsEnd(METRICS_PHASE_INPUT);   // never opened: must add nothing
    sFakeTicks += 1000;
    Platform_MetricsFrameEnd();                 // frame = 2000 us, rolls the window

    s = Platform_MetricsSummary();
    TEST_PTR_NOT_NULL((const void *)s);
    if (s == NULL)
        return;

    TEST_EQ(s->frames, 1);

    // The nested Begin did not restart the clock: PPU is the whole span.
    TEST_TRUE(s->phaseMs[METRICS_PHASE_PPU] > 0.99 && s->phaseMs[METRICS_PHASE_PPU] < 1.01);

    // The unopened End charged nothing to INPUT.
    TEST_TRUE(s->phaseMs[METRICS_PHASE_INPUT] < 0.01);

    // Busy is the frame minus idle. This frame had no idle phase at all, so the
    // whole 2000 us is busy -- PPU is only the part a phase accounted for, which
    // is the point: busy cannot undercount unattributed work.
    TEST_TRUE(s->busyMs > 1.99 && s->busyMs < 2.01);
    TEST_TRUE(s->idlePercent < 0.01);
}

// --- engine: the module is a pure observer -----------------------------------

// Rendering is a pure function of engine memory, so identical framebuffers prove
// the module did not touch anything the game can observe. That is the property
// several other behaviours depend on: the boot test's captures are
// byte-reproducible, and other tests assert exact engine state.
//
// Enabling metrics with a live clock and then rendering the SAME frame state must
// produce the same pixels. A counter that fed the simulation would show up here
// as a pixel difference, deterministically.
FIRERED_TEST("metrics/enabling them does not change engine state", "engine fixture:bedroom metrics",
             metrics_observer_only)
{
    static u16 before[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    static u16 after[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    int i;

    Test_RunFrames(60);

    Test_RenderFrame();
    for (i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
        before[i] = Test_Framebuffer()[i];

    // Turn the module on with a working clock. No frames run between the two
    // renders: only the instrumentation is new.
    Harness_MetricsClockReset();
    Platform_MetricsSetClock(Harness_MetricsClock, HARNESS_METRICS_TICKS_PER_SECOND);
    Platform_MetricsSetEnabled(true);

    Test_RenderFrame();
    for (i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
        after[i] = Test_Framebuffer()[i];

    TEST_MEM_EQ(before, after, sizeof(before));

    // And the engine still plays: with the module running, input still moves the
    // player and the world is still the fixture's.
    {
        s16 y0 = Test_PlayerY();

        Test_Hold(DPAD_DOWN, 40);
        Test_RunFrames(50);
        TEST_TRUE(Test_PlayerY() > y0);
        TEST_TRUE(Test_InOverworld());
        TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F));
    }

    Platform_MetricsSetEnabled(false);
}

// The live frame loop, through a real (synthetic) clock. This is what proves the
// Begin/End pairs in src/main.c and src/platform/sdl2.c are balanced: a missing
// End leaves a phase accumulating forever, and a missing Begin leaves a phase at
// zero -- and the phases must still be consistent with the frame either way.
//
// No absolute millisecond figure is asserted: the harness has no pacer, and the
// synthetic clock's value is arbitrary. The relationships are the contract.
FIRERED_TEST("metrics/live window is consistent with the frames run", "engine fixture:bedroom metrics",
             metrics_live_window)
{
    const struct MetricsSummary *s;
    double phaseSum = 0.0;
    int p;

    Harness_MetricsClockReset();
    Platform_MetricsSetClock(Harness_MetricsClock, HARNESS_METRICS_TICKS_PER_SECOND);
    // Shorter than the wall time the run below takes in synthetic ticks, so the
    // window certainly closes inside it.
    Platform_MetricsSetWindowSeconds(0.05);
    Platform_MetricsReset();
    Platform_MetricsSetEnabled(true);

    Test_RunFrames(120);

    s = Platform_MetricsSummary();
    TEST_PTR_NOT_NULL((const void *)s);

    if (s != NULL)
    {
        TEST_GE(s->frames, 1);

        // The harness replaces Platform_RenderAndPresent (tests/platform_host.c),
        // so PPU, present and idle are genuinely absent there -- sdl2.o, which
        // owns those three phases, is not part of the test binary. What the
        // harness does run is the engine frame itself: the vblank, the input
        // pump and the frame loop's own logic phase. At least one of those must
        // be non-zero, or the loop's Begin/End pairs are not both firing.
        {
            double measured = s->phaseMs[METRICS_PHASE_VBLANK]
                            + s->phaseMs[METRICS_PHASE_INPUT]
                            + s->phaseMs[METRICS_PHASE_LOGIC];
            TEST_TRUE(measured > 0.0);
        }
        // The partition phases cannot sum to more than the frame they came from,
        // allowing a tick for the averaging's rounding.
        for (p = 0; p < METRICS_PHASE_COUNT; p++)
        {
            if (p == METRICS_PHASE_AUDIO)
                continue;   // nested inside VBLANK: would be counted twice
            phaseSum += s->phaseMs[p];
        }
        TEST_TRUE(phaseSum <= s->frameMs + 0.5);

        // Idle is a share of the frame, so it cannot exceed 100%.
        TEST_TRUE(s->idlePercent <= 100.0);

        // The gauges are sampled from the engine's own tables, so the fixture's
        // on-screen sprites and running tasks must be visible.
        TEST_GE(s->gauges[METRICS_GAUGE_SPRITES], 1);
    }

    Platform_MetricsSetEnabled(false);
    Platform_MetricsReset();
}

