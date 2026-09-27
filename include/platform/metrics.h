#ifndef GUARD_PLATFORM_METRICS_H
#define GUARD_PLATFORM_METRICS_H

// Frame-budget instrumentation for the native port.
//
// The engine is frame-locked (one engine frame is 280896 CPU cycles at
// 16777216 Hz, i.e. ~16.7427 ms -- see "Native Frame Timing" in AGENTS.md), so
// the useful question is never "how fast" but "how much of the frame budget is
// spent, in which phase, and what does a bad frame look like".
//
// The module is a pure observer, and that is an invariant three later
// behaviours depend on: the boot test's captures are byte-reproducible across
// runs, the test suite boots a fixture and then asserts exact engine state, and
// the RNG is seeded from a host timer. So every hook is a read of the clock or a
// write to this module's own state -- nothing here may touch engine state, and
// tests/metrics.c asserts that a run with metrics enabled and a run with them
// disabled reach the same place.
//
// Cost when disabled: one predictable branch per hook. When enabled, one clock
// read per phase boundary. The clock is supplied by the platform (SDL's
// performance counter in the game) so this file stays SDL-free and links into
// the test binary, which has no SDL at all.

#include <stdint.h>
#include <stdbool.h>

// One engine frame's budget, in microseconds. Derived from the hardware frame
// period the port paces to; used only to classify a frame as an overrun. The
// engine's own clock is the authority -- this is a reference, not a target.
#define METRICS_FRAME_BUDGET_MICROS (280896.0 * 1000000.0 / 16777216.0)

// The phases of one frame. They are disjoint EXCEPT METRICS_PHASE_AUDIO, which
// is measured around m4aSoundMain inside VBlankIntr and is therefore already
// contained in METRICS_PHASE_VBLANK. Audio stays a separate number because it is
// the phase a sound regression appears in, and it is reported as a
// sub-measurement so the list cannot be read as a partition. Summing the phases
// must therefore skip AUDIO; Platform_MetricsBusyMicros() is the only supported
// total and does exactly that.
enum MetricsPhase
{
    METRICS_PHASE_LOGIC,    // engine frame work: callbacks, scripts, tasks, map music
    METRICS_PHASE_INPUT,    // SDL event pump + REG_KEYINPUT write
    METRICS_PHASE_VBLANK,   // vblank callback, GPU register copy, DMA3, link, Random
    METRICS_PHASE_AUDIO,    // ...nested in VBLANK: m4aSoundMain + the audio handoff
    METRICS_PHASE_PANEL,    // developer panel update (0 unless --dev-panel)
    METRICS_PHASE_IDLE,     // WaitForFrameDeadline -- the frame's headroom
    METRICS_PHASE_PPU,      // PPU_RenderFrame
    METRICS_PHASE_PRESENT,  // texture upload + SDL_RenderPresent
    METRICS_PHASE_COUNT
};

// Gauges are sampled per frame and aggregated over a window. Each has an
// aggregation mode: most are written once per frame (MODE_SET), the scanline
// gauges and the HBlank transfer count accumulate across the frame's sub-steps
// (MODE_ADD), and the audio queue depth is a low-water mark (MODE_MIN) because a
// queue that ever runs dry is the problem.
//
// The two scanline gauges count sprite x scanline samples, not distinct sprites:
// that is exactly what the compositor iterates, so it is proportional to sprite
// rendering cost. Counting distinct sprites per frame would need a per-frame
// visited set inside the hot loop.
enum MetricsGauge
{
    METRICS_GAUGE_TASKS,                 // active engine tasks
    METRICS_GAUGE_SPRITES,               // sprites in use
    METRICS_GAUGE_SPRITE_SCANLINES,       // sprite x scanline samples drawn
    METRICS_GAUGE_AFFINE_SPRITE_SCANLINES,// ...of those, affine (rotated/scaled)
    METRICS_GAUGE_HBLANK_DMA,            // HBlank DMA transfers replayed this frame
    METRICS_GAUGE_AUDIO_QUEUE_MIN,       // lowest audio queue depth seen this frame
    METRICS_GAUGE_HEAP_USED,             // bytes allocated from gHeap
    METRICS_GAUGE_HEAP_LARGEST_FREE,     // largest single free block (fragmentation)
    METRICS_GAUGE_HEAP_BLOCKS,           // blocks in the heap free list
    METRICS_GAUGE_HEAP_HIGH_WATER,       // peak bytes allocated since boot
    METRICS_GAUGE_COUNT
};

enum MetricsCounter
{
    METRICS_COUNTER_ALLOCS,          // cumulative Alloc/AllocZeroed calls
    METRICS_COUNTER_FREES,           // cumulative Free calls
    METRICS_COUNTER_AUDIO_UNDERRUNS, // SDL callbacks that had to emit silence
    METRICS_COUNTER_PACER_RESYNCS,   // frames where the pacer dropped its deadline
    METRICS_COUNTER_AUDIO_IN_FRAMES, // engine PCM samples handed to the resampler
    METRICS_COUNTER_AUDIO_OUT_FRAMES,// device-rate frames produced by it
    METRICS_COUNTER_COUNT
};

// One frame's measurements. This is the module's own record type and is the
// aggregation entry point as well: Platform_MetricsFrameEnd() fills one of these
// from the clock and the call sites and hands it to Platform_MetricsRecordFrame(),
// so the aggregation can be exercised with synthetic numbers (tests/metrics.c)
// rather than only through a live frame.
struct MetricsFrame
{
    uint64_t frameMicros;
    uint64_t phaseMicros[METRICS_PHASE_COUNT];
    uint32_t gauges[METRICS_GAUGE_COUNT];
    uint32_t counters[METRICS_COUNTER_COUNT]; // cumulative, as of this frame
};

// One reporting window's aggregate. Kept after the window rolls over so the HUD
// can read it at any point in the next window.
struct MetricsSummary
{
    uint32_t frames;                 // frames in the window
    double frameMs;                  // mean
    double minMs;
    double p50Ms;
    double p95Ms;
    double maxMs;
    double phaseMs[METRICS_PHASE_COUNT];  // mean per phase
    double idlePercent;              // mean idle / mean frame time
    double busyMs;                   // mean frame time - mean idle (engine + render)
    uint32_t overruns;               // frames over METRICS_FRAME_BUDGET_MICROS
    uint32_t gauges[METRICS_GAUGE_COUNT]; // aggregate per gauge in the window
    double allocsPerFrame;
    double freesPerFrame;
    double audioUnderrunsPerFrame;
};

// Samples engine-owned gauges: the task and sprite tables here, and the heap in
// src/malloc.c's own sampler (its block layout is private to that file). Called
// once per frame by Platform_MetricsFrameEnd, before the snapshot, so every SET
// gauge holds a current value. The heap walk is O(blocks) -- tens of
// dereferences against a 16.7 ms budget; the sprite scan is O(MAX_SPRITES).
void Platform_MetricsSampleEngine(void);
void Platform_MetricsSampleHeap(void);

// --- lifecycle --------------------------------------------------------------

bool Platform_MetricsIsEnabled(void);
void Platform_MetricsSetEnabled(bool enabled);

// The clock and its frequency. A zero clock or frequency leaves the module
// inert even when enabled, so a caller that cannot supply a clock (the harness)
// gets silence rather than garbage. sdl2.c installs SDL's performance counter.
void Platform_MetricsSetClock(uint64_t (*clock)(void), uint64_t ticksPerSecond);

// Report cadence: one stdout line per window (default 1.0 s). A caller that
// reports itself (the HUD, the CSV) sets this to 0 to silence the stdout line.
void Platform_MetricsSetWindowSeconds(double seconds);

// One CSV row per frame, with a header row naming every column. Returns false if
// the file could not be opened. The handle is closed at exit.
bool Platform_MetricsOpenCsv(const char *path);

// Prints the accumulated summary. Registered with atexit when enabled, so the
// boot test's exit(0) still reports; safe (and a no-op) to call twice.
void Platform_MetricsReport(void);

// Drops every accumulator, including the window summary. For tests.
void Platform_MetricsReset(void);

// --- call sites -------------------------------------------------------------

void Platform_MetricsBegin(enum MetricsPhase phase);
void Platform_MetricsEnd(enum MetricsPhase phase);

// The frame's own boundaries: Begin at the top of the engine's frame loop,
// End after the frame has been presented. End also writes the CSV row and prints
// the window line when the window rolls over.
void Platform_MetricsFrameBegin(void);
void Platform_MetricsFrameEnd(void);

void Platform_MetricsSetGauge(enum MetricsGauge gauge, uint32_t value);
void Platform_MetricsAddGauge(enum MetricsGauge gauge, uint32_t delta);
void Platform_MetricsAddCounter(enum MetricsCounter counter, uint32_t delta);

// Aggregates one frame into the current window and returns true when the window
// rolled over. Platform_MetricsFrameEnd() is the caller in the live loop; tests
// call it directly with synthetic numbers.
//
// Deliberately does NOT print: a test exercising the aggregation would otherwise
// write to stdout, and the live loop is the only thing that should.
bool Platform_MetricsRecordFrame(const struct MetricsFrame *frame);

// A frame's total busy time: the phases that partition the frame, minus the
// pacer's idle, with the nested AUDIO phase excluded so it is counted once
// (inside VBLANK) rather than twice. This, not the paced frame time, is the
// quantity that has to fit the budget.
uint64_t Platform_MetricsBusyMicros(const struct MetricsFrame *frame);

// Closes and publishes the current window immediately, so Platform_MetricsSummary
// reflects the frames recorded so far rather than waiting for the window's
// duration to elapse. The exit report uses it, and a test that wants to read an
// aggregate without advancing the clock can use it too. A no-op when no frames
// have been recorded since the last roll.
void Platform_MetricsFlushWindow(void);

// --- observation ------------------------------------------------------------

// The last completed window's aggregate, or NULL before the first window closes.
const struct MetricsSummary *Platform_MetricsSummary(void);

// Human-readable names, for the reporter and for tests that check the report
// mentions each phase by name.
const char *Platform_MetricsPhaseName(enum MetricsPhase phase);
const char *Platform_MetricsGaugeName(enum MetricsGauge gauge);
const char *Platform_MetricsCounterName(enum MetricsCounter counter);

// How a gauge combines within a frame and over a window. Exposed so the test can
// assert the mode table covers every gauge rather than trusting the switch.
enum MetricsGaugeMode
{
    METRICS_GAUGE_MODE_SET, // last write wins within the frame; max over the window
    METRICS_GAUGE_MODE_ADD, // sums within the frame; max over the window
    METRICS_GAUGE_MODE_MIN, // minimum within the frame and over the window
};

enum MetricsGaugeMode Platform_MetricsGaugeMode(enum MetricsGauge gauge);

#endif // GUARD_PLATFORM_METRICS_H
