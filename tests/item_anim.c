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
