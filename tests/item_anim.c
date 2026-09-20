// The item-use animation scene (src/pokemon_special_anim.c).
//
// pret keeps the scene state in a task's first two data slots with
// SetWordTaskArg, which stores exactly 32 bits -- the width of a GBA address.
// The host heap is mapped above 4 GiB (gHeap lives at 0x1_00xxxxxx), so the
// stored pointer lost its top half and the very first dereference in
// Task_UseItem_Normal faulted. Using any Potion or Rare Candy on a party mon
// crashed the game.
//
// The test starts the real animation and then reads the scene back through the
// accessors the animation itself uses, which is where the truncated pointer was
// dereferenced. It needs a booted save for the party and for the heap.

#include "registry.h"

#include "constants/items.h"
#include "constants/moves.h"
#include "pokemon.h"
#include "pokemon_special_anim.h"
#include "sprite.h"
#include "pokemon_special_anim_internal.h"
#include "task.h"

static void TestAnimFinishedCallback(void) {}

// True once the animation's own task is installed and running. The scene sets
// the main callback to CB2_PSA and leaves the task in gTasks, so the task's
// presence is the observable that the hand-off happened.
static bool8 AnimTaskIsRunning(void)
{
    int i;

    for (i = 0; i < NUM_TASKS; i++)
    {
        if (gTasks[i].isActive && gTasks[i].func != TaskDummy)
            return TRUE;
    }
    return FALSE;
}

FIRERED_TEST("item_anim/use-item scene keeps its state pointer whole",
             "engine fixture:lab item_anim", item_anim_pointer)
{
    u16 species;

    Test_RequireFixture(FIXTURE_OAKS_LAB);
    TEST_GE(Test_PartyCount(), 1);

    species = Test_PartySpecies(0);

    // The call the party menu makes when a Potion is used on a mon.
    StartUseItemAnim_Normal(0, ITEM_POTION, TestAnimFinishedCallback);

    // The scene owns the frame and its own task is live. Pre-fix the pointer to
    // its state lost the top half of the host heap address, so the first
    // dereference inside that task faulted -- the process died, so reaching the
    // assertions below at all is the fix.
    TEST_TRUE(AnimTaskIsRunning());

    // The state is reachable and describes the mon the menu selected. Both of
    // these dereference the stored pointer.
    TEST_EQ(PSA_GetItemId(), ITEM_POTION);
    TEST_EQ(PSA_GetMonSpecies(), species);
    TEST_EQ(Test_PartySpecies(0), species);
}

// The same storage is used by the TM/HM path, which also carries the forgotten
// move's name through the pointer.
FIRERED_TEST("item_anim/tm scene keeps its state pointer whole",
             "engine fixture:lab item_anim", item_anim_tm_pointer)
{
    Test_RequireFixture(FIXTURE_OAKS_LAB);

    StartUseItemAnim_ForgetMoveAndLearnTMorHM(0, ITEM_TM01, MOVE_POUND, TestAnimFinishedCallback);

    TEST_TRUE(AnimTaskIsRunning());
    TEST_EQ(PSA_GetItemId(), ITEM_TM01);
    TEST_EQ(PSA_GetMonSpecies(), Test_PartySpecies(0));
}

// The evolution-stone path uses the same storage; a mon that cannot evolve
// reaches it.
FIRERED_TEST("item_anim/cant-evolve scene keeps its state pointer whole",
             "engine fixture:lab item_anim", item_anim_cant_evolve_pointer)
{
    Test_RequireFixture(FIXTURE_OAKS_LAB);

    StartUseItemAnim_CantEvolve(0, ITEM_FIRE_STONE, TestAnimFinishedCallback);

    TEST_TRUE(AnimTaskIsRunning());
    TEST_EQ(PSA_GetItemId(), ITEM_FIRE_STONE);
    TEST_EQ(PSA_GetMonSpecies(), Test_PartySpecies(0));
}

// Linking the scene is what makes the animation VISIBLE, and that is a claim
// about pixels -- the pointer tests above cannot see it. Before this, 27 stubs
// in src/platform/battle_peripheral_stubs.c (PSA_SetUpItemUseOnMonAnim,
// PSA_SetUpZoomAnim, PSA_CreateMonSpriteAtCloseness, CreateItemSpriteAtMaxCloseness,
// the level-up window painters and the rest) meant the scene advanced its state
// machine while drawing nothing at all.
//
// Asserted differentially: capture the field, start the animation, capture
// again, and require the region the mon zooms into to have changed. A golden
// frame would break on any legitimate palette or layout edit; a difference
// against the same save's own field cannot.
FIRERED_TEST("item_anim/the use-item scene actually draws",
             "engine fixture:lab item_anim", item_anim_draws)
{
    // A COPY, not the live pointer: Test_RenderFrame rewrites the harness's one
    // framebuffer in place, so holding Test_Framebuffer() aliases the frames the
    // loop below renders and every diff would be zero.
    static u16 before[240 * 160];
    int i;
    int liveSprites = 0;

    Test_RequireFixture(FIXTURE_OAKS_LAB);

    // The field as it is, with no animation running.
    Test_RenderFrame();
    memcpy(before, Test_Framebuffer(), sizeof(before));

    // The mon's own zoomed-in region. Sampled while the animation runs below.
    StartUseItemAnim_Normal(0, ITEM_POTION, TestAnimFinishedCallback);
    TEST_TRUE(AnimTaskIsRunning());

    // Run the scene far enough to reach its zoom and its sprite burst, and hold
    // the widest frame it produces.
    int bestDiff = 0;
    int bestColors = 0;
    for (i = 0; i < 8; i++)
    {
        int diff, colors, j, n = 0;

        Test_RunFrames(15);
        Test_RenderFrame();

        for (j = 0; j < MAX_SPRITES; j++)
            if (gSprites[j].inUse)
                n++;
        if (n > liveSprites)
            liveSprites = n;

        // Where the mon sprite zooms in. Compared against the pre-animation
        // field, so this is "the scene painted something here", not "the field
        // has scenery".
        diff = Test_RegionDiffers(40, 16, 120, 96, before);
        colors = Test_DistinctColors(40, 16, 120, 96);
        if (diff > bestDiff)
            bestDiff = diff;
        if (colors > bestColors)
            bestColors = colors;
    }

    // The scene draws the mon (and its background) into that region: a stub
    // implementation leaves the field exactly as it was.
    TEST_GE(bestDiff, 1000);
    TEST_GE(bestColors, 8);

    // And it drives real sprites, which is what the sprite helpers the scene now
    // provides are for. The item-use scene's outward spiral burst alone creates
    // dozens in one frame.
    TEST_GE(liveSprites, 10);
}
