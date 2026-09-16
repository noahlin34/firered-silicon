// Input scheduling.
//
// The engine reads REG_KEYINPUT (active low) once per iteration in ReadKeys().
// The register is written from Platform_UpdateInput(), which the engine calls
// inside WaitForVBlank -- after the previous ReadKeys and before the next, which
// is exactly the window a probe has to hit (see the AGENTS.md note on writing
// REG_KEYINPUT: doing it later is one frame late and gets clobbered).
//
// Presses are scheduled as press/release cadences, never as a held key. Engine
// code waits on JOY_NEW edges (TextPrinterWaitWithDownArrow, the battle
// controller's print states), and a held key produces no edge -- a held button
// stalls a dialogue box forever.

#include <string.h>

#include "internal.h"

#include "gba/io_reg.h"
#include "script.h"

#define KEYS_MASK 0x03FF

#define PRESS_FRAMES 4
#define RELEASE_FRAMES 8

// A scheduled press/release cycle. `remaining` counts down on each frame; while
// it is inside the press window the button is held down.
struct ScheduledPress
{
    u16 button;
    int remaining;
};

#define MAX_SCHEDULED 64

static struct ScheduledPress sScheduled[MAX_SCHEDULED];
static int sScheduledCount;
static u16 sHeld;            // continuous hold (Test_Hold)
static int sHeldRemaining;

void Input_Reset(void)
{
    sScheduledCount = 0;
    sHeld = 0;
    sHeldRemaining = 0;
}

static void Schedule(u16 button)
{
    if (sScheduledCount < MAX_SCHEDULED)
    {
        sScheduled[sScheduledCount].button = button;
        sScheduled[sScheduledCount].remaining = PRESS_FRAMES + RELEASE_FRAMES;
        sScheduledCount++;
    }
}

void Test_Press(u16 button)
{
    Schedule(button);
}

void Test_PressRepeated(u16 button, int times)
{
    int i;
    for (i = 0; i < times; i++)
        Schedule(button);
}

void Test_Hold(u16 button, int frames)
{
    sHeld = button;
    sHeldRemaining = frames;
}

void Test_ReleaseAll(void)
{
    sScheduledCount = 0;
    sHeld = 0;
    sHeldRemaining = 0;
}

// Presses `button` repeatedly until the engine reports the field idle again.
//
// The first press must always happen: a test calls this to START an interaction
// (walk up and talk, open a menu), and at that moment the engine is already idle,
// so an "is it idle yet?" check before the first press would return immediately
// and nothing would ever be pressed. So the loop presses first and only then
// waits for the idle state to be reached -- which is also what makes it terminate
// after the interaction closes.
void Test_PressUntilIdle(u16 button, int maxFrames)
{
    int waited = 0;

    Schedule(button);

    while (waited < maxFrames)
    {
        Test_YieldToEngine(1);
        waited++;

        // Idle again, with the press cadence drained: the interaction finished.
        if (!ArePlayerFieldControlsLocked() && !ScriptContext_IsEnabled()
            && sScheduledCount == 0)
            return;

        // Keep pressing, but only when nothing is mid-cadence, so consecutive
        // presses are separated by a release the engine can observe (a held key
        // produces no JOY_NEW edge and would stall a dialogue box).
        if (sScheduledCount == 0)
            Schedule(button);
    }
}

// --- per-frame hooks --------------------------------------------------------
void Input_Tick(void)
{
    int i = 0;

    while (i < sScheduledCount)
    {
        sScheduled[i].remaining--;
        if (sScheduled[i].remaining <= 0)
        {
            sScheduled[i] = sScheduled[sScheduledCount - 1];
            sScheduledCount--;
            continue;
        }
        i++;
    }

    if (sHeldRemaining > 0)
    {
        sHeldRemaining--;
        if (sHeldRemaining == 0)
            sHeld = 0;
    }
}

void Input_ApplyToKeys(void)
{
    u16 down = sHeld;
    int i;

    for (i = 0; i < sScheduledCount; i++)
    {
        if (sScheduled[i].remaining > RELEASE_FRAMES)
            down |= sScheduled[i].button;
    }

    REG_KEYINPUT = (u16)(~down & KEYS_MASK);
}
