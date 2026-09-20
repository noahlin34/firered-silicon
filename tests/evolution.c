// The evolution scene (src/evolution_scene.c).
//
// Both of the engine's evolution entry points end in the same call,
// BeginEvolutionScene: the party menu's PartyMenuTryEvolution (Rare Candy, an
// evolution stone) and the battle's TryEvolvePokemon (a level-up). Neither
// caller could do anything while src/evolution_scene.c was unlinked, because the
// two functions were empty stubs in src/platform/battle_engine_stubs.c:
//
//     void BeginEvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
//     void EvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
//
// The stub installed no main callback and changed nothing, so a mon that
// levelled into its evolution level simply stayed unevolved -- and because the
// battle path clears its gLeveledUpInBattle bit *before* asking for the
// evolution, the chance was consumed too. Nothing crashed, logged, or failed a
// test, which is why it survived.
//
// These tests drive the real call those callers make and assert what a player
// observes: the species and nickname change, the pokedex records the new
// species, B stops the evolution, and control comes back to the caller.
//
// The scene is driven with a press/release cadence on A, which is what a player
// does: its message boxes wait on a JOY_NEW edge, and a held key produces none.
// Without the presses the scene parks on its first box (measured: still in
// EVOSTATE_TRY_LEARN_MOVE after 4,000 frames).

#include "registry.h"

#include "constants/items.h"
#include "constants/pokemon.h"
#include "item.h"
#include "evolution_scene.h"
#include "main.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"

// The scene ends by handing control to gCB2_AfterEvolution, which the caller
// installs: the party menu puts its exitCallback there, a battle puts
// BattleMainCB2. Standing in for the caller is how these tests both provide the
// callback the scene needs and observe that it really finished -- the flag is the
// difference between "the scene ran to the end" and "the harness ran out of
// frames".
static bool8 sAfterEvolutionRan;

static void TestAfterEvolutionCallback(void)
{
    sAfterEvolutionRan = TRUE;
    SetMainCallback2(CB2_ReturnToField);
}

// Runs the scene to completion, pressing A every 20 frames to walk it through its
// boxes. Returns the frame it finished on, or -1 if it never did.
static int RunEvolutionToCompletion(int maxFrames)
{
    int i;

    for (i = 0; i < maxFrames; i++)
    {
        if (i % 20 == 0)
            Test_Press(A_BUTTON);
        Test_RunFrames(1);
        if (sAfterEvolutionRan)
            return i;
    }
    return -1;
}

FIRERED_TEST("evolution/levelling into the evolution level evolves the mon",
             "engine fixture:lab evolution", evolution_from_party_menu)
{
    struct Pokemon *mon;
    u8 name[20];

    Test_RequireFixture(FIXTURE_OAKS_LAB);
    TEST_GE(Test_PartyCount(), 1);
    TEST_EQ(Test_PartySpecies(0), SPECIES_BULBASAUR);

    mon = &gPlayerParty[0];

    // The engine's own decision function, called with the same arguments both
    // callers pass: a Bulbasaur at its evolution level is offered an Ivysaur.
    // The battle path consults exactly this (SpeciesToNationalPokedexNum aside)
    // before it asks for the scene.
    SetMonData(mon, MON_DATA_LEVEL, (u8[]){16});
    TEST_EQ(GetEvolutionTargetSpecies(mon, EVO_MODE_NORMAL, ITEM_NONE), SPECIES_IVYSAUR);

    sAfterEvolutionRan = FALSE;
    gCB2_AfterEvolution = TestAfterEvolutionCallback;
    BeginEvolutionScene(mon, SPECIES_IVYSAUR, TRUE, 0);

    // The hand-off firing is the assertion that the scene reached its end state
    // rather than stalling; pre-fix nothing installed a callback at all.
    TEST_GE(RunEvolutionToCompletion(3000), 0);

    // What the player sees. The nickname is renamed with the species because it
    // still matched the old species name.
    TEST_EQ(Test_PartySpecies(0), SPECIES_IVYSAUR);
    GetMonData(mon, MON_DATA_NICKNAME, name);
    TEST_STR_EQ(Test_Decode(name), "IVYSAUR");

    // The scene records the evolved species in the pokedex as seen and caught.
    // Both are the scene's own bookkeeping, skipped entirely when it does not run.
    TEST_TRUE(GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_IVYSAUR), FLAG_GET_CAUGHT));
    TEST_TRUE(GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_IVYSAUR), FLAG_GET_SEEN));

    // The party is usable again: the evolved mon answers for the new species.
    TEST_EQ(GetMonData(mon, MON_DATA_SPECIES), SPECIES_IVYSAUR);
}

// The other half of the scene's contract, and the reason TASK_BIT_CAN_STOP
// exists: both callers pass canStopEvo = TRUE, and holding B while the mon is
// cycling must abandon the evolution. The mon keeps its species and nickname,
// the task machine still runs to its end, and the caller gets control back.
FIRERED_TEST("evolution/holding B stops the evolution",
             "engine fixture:lab evolution", evolution_can_be_stopped)
{
    struct Pokemon *mon;
    u8 name[20];
    int i;

    Test_RequireFixture(FIXTURE_OAKS_LAB);
    mon = &gPlayerParty[0];
    SetMonData(mon, MON_DATA_LEVEL, (u8[]){16});
    GetMonData(mon, MON_DATA_NICKNAME, name);

    sAfterEvolutionRan = FALSE;
    gCB2_AfterEvolution = TestAfterEvolutionCallback;
    BeginEvolutionScene(mon, SPECIES_IVYSAUR, TRUE, 0);

    for (i = 0; i < 3000; i++)
    {
        if (i % 20 == 0)
            Test_Press(A_BUTTON);
        Test_Hold(B_BUTTON, 1);
        Test_RunFrames(1);
        if (sAfterEvolutionRan)
            break;
    }

    // The scene still finished and handed control back...
    TEST_TRUE(sAfterEvolutionRan);

    // ...but the mon did not evolve, and its name was not renamed.
    TEST_EQ(Test_PartySpecies(0), SPECIES_BULBASAUR);
    {
        u8 after[20];
        GetMonData(mon, MON_DATA_NICKNAME, after);
        TEST_STR_EQ(Test_Decode(after), Test_Decode(name));
    }
    TEST_FALSE(GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_IVYSAUR), FLAG_GET_CAUGHT));
}

// The path a player actually takes, with no scene-level setup: a Rare Candy in
// the BAG, used on a mon one level short of its evolution level. This drives
// START -> BAG -> Rare Candy -> USE -> mon and lets the engine's own item path
// (ItemUseCB_RareCandyStep -> PartyMenuTryEvolution) start the scene, so it
// covers the item that could level a mon up and never evolve it.
FIRERED_TEST("evolution/using a Rare Candy at the evolution level evolves the mon",
             "engine fixture:lab evolution slow", evolution_rare_candy)
{
    int i;

    Test_RequireFixture(FIXTURE_OAKS_LAB);
    TEST_EQ(Test_PartySpecies(0), SPECIES_BULBASAUR);

    AddBagItem(ITEM_RARE_CANDY, 1);
    // One level short, so the Rare Candy is what reaches Lv16 and asks for the
    // evolution -- not a direct call to the scene.
    SetMonData(&gPlayerParty[0], MON_DATA_LEVEL, (u8[]){15});
    TEST_EQ(Test_PartyLevel(0), 15);

    // START opens the menu; the BAG entry is one row below POKéMON, which this
    // save has because FLAG_SYS_POKEMON_GET is set on the lab fixture.
    Test_Press(START_BUTTON);
    Test_RunFrames(60);
    Test_Press(DPAD_DOWN);
    Test_RunFrames(20);
    Test_Press(A_BUTTON);
    Test_RunFrames(90);

    // A on the item opens its context menu, A again chooses USE.
    Test_Press(A_BUTTON);
    Test_RunFrames(40);
    Test_Press(A_BUTTON);
    Test_RunFrames(60);

    // The party menu asks which mon; then the level-up, its stat pages, the
    // evolution scene and its boxes all run to the end.
    for (i = 0; i < 500; i++)
    {
        Test_Press(A_BUTTON);
        Test_RunFrames(20);
        if (Test_PartySpecies(0) == SPECIES_IVYSAUR)
            break;
    }

    TEST_EQ(Test_PartySpecies(0), SPECIES_IVYSAUR);
    TEST_EQ(Test_PartyLevel(0), 16);
    // The candy was consumed exactly once, by the same interaction that evolved
    // the mon.
    TEST_FALSE(Test_HasItem(ITEM_RARE_CANDY, 1));
}
