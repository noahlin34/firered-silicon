// The field-poison whiteout chain (data/scripts/white_out.inc).
//
// Fainting from poison on a step is the one whiteout the player can reach
// without a battle, and it used to soft-lock the game. EventScript_FieldPoison
// calls special 199 (TryFieldPoisonWhiteOut), and when a mon really fainted the
// script continues into EventScript_FieldWhiteOutFade:
//
//     special Script_FadeOutMapMusic   @ 332
//     waitstate                        @ ScriptContext_Stop()
//     fadescreen FADE_TO_BLACK
//     special SetCB2WhiteOut           @ 200
//     waitstate
//     end
//
// ScrCmd_waitstate is ScriptContext_Stop() with nothing to restart it, so the
// ONLY thing that hands control back is the task special 332 creates
// (Task_EnableScriptAfterMusicFade calls ScriptContext_Enable once the BGM fade
// ends). With 332 unregistered it printed "[Script] special 332 is not ported",
// the task was never created, and the field stayed locked for good: the screen
// faded to black and the game never came back.
//
// The tests assert where the player ends up rather than that the specials
// exist, because "the field is still locked N frames later" is exactly the bug.
// The engine's own end-of-warp signal (unlocked controls on CB1_Overworld) is
// what they wait on, so they do not depend on frame counts.
//
// Reaching the whiteout needs two preconditions the fixtures do not arrange: a
// heal location to respawn at, and a poisoned mon at 1 HP so a step kills it.
// Both are written the way a real playthrough leaves them (SetLastHealLocationWarp
// is what the Pokémon Center nurse and the player's house both call).

#include "internal.h"

#include "constants/battle.h"
#include "constants/heal_locations.h"
#include "constants/maps.h"
#include "constants/pokemon.h"
#include "constants/vars.h"

#include "money.h"
#include "overworld.h"
#include "pokemon.h"
#include "script.h"


static void PoisonLeadMonAtOneHp(void)
{
    u32 status = STATUS1_PSN_ANY;
    u16 hp = 1;

    SetMonData(&gPlayerParty[0], MON_DATA_STATUS, &status);
    SetMonData(&gPlayerParty[0], MON_DATA_HP, &hp);
}

// One full poison-faint whiteout, driven from the player's own step.
//
// VAR_POISON_STEP_COUNTER is seeded to 4 so the very next step fires the tick
// (UpdatePoisonStepCounter does ++, %= 5, and acts on 0), which keeps the player
// inside Pallet Town instead of walking him off the south border. Test_Hold only
// SETS a hold -- frames have to be run for it to take effect -- so the step is
// taken by running frames while the hold is armed.
//
// The whiteout is a dialogue chain, not a silent one: `msgbox Text_WhitedOut`,
// then a second box, then the respawn heal cutscene asks the nurse/mother to
// hand the party back. Every one of those boxes blocks on JOY_NEW(A_BUTTON), so
// the run presses A while walking the chain rather than only waiting it out.
static void RunPoisonWhiteout(void)
{
    int waited = 0;

    // Pallet Town, on open ground away from the north edge's Oak trigger and
    // from any warp tile.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN), MAP_NUM(MAP_PALLET_TOWN), 12, 6);
    Test_RunFramesToWarp();

    Test_SetVar(VAR_POISON_STEP_COUNTER, 4);

    // Walk until the tick fires the whiteout, or give up well past it.
    while (waited < 600)
    {
        Test_Hold(DPAD_DOWN, 16);
        Test_RunFrames(16);
        waited += 16;

        // The whiteout has begun: the field is locked with a script running.
        if (Test_FieldLocked())
            break;
    }

    TEST_TRUE(waited < 600);   // one step with the counter pre-seeded must trigger it
}

// Advance the whiteout to its end: the fade, the CB2 hand-off, and the respawn
// cutscene's boxes. Waits for the engine's own end signal (the respawn map with
// the field handed back) rather than a frame count, so an edit to any of the
// chain's delays does not break the test.
static int AdvanceWhiteout(int maxFrames)
{
    int i;

    // PressUntilIdle is the project's "advance dialogue" primitive: it keeps
    // pressing until the field is idle again, which is exactly the end of this
    // chain. Bounded by maxFrames so a regression cannot hang the suite.
    Test_PressUntilIdle(A_BUTTON, maxFrames);

    if (!Test_FieldLocked()
        && Test_MapGroup() == MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F))
        return 0;
    return -1;
}

FIRERED_TEST("whiteout/field poison hands the field back instead of locking it",
             "engine fixture:lab whiteout", whiteout_field_poison_recovers)
{
    // fixture:lab, not fixture:bedroom: the bedroom save has no party at all,
    // so nothing can be poisoned and the step trigger never fires.
    Test_RequireFixture(FIXTURE_OAKS_LAB);

    TEST_GE(Test_PartyCount(), 1);
    TEST_GE(Test_PartyMaxHP(0), 2);   // or poison cannot kill it

    // A respawn point, as if the player had healed at home or a Center.
    SetLastHealLocationWarp(HEAL_LOCATION_PALLET_TOWN);

    PoisonLeadMonAtOneHp();
    TEST_EQ(Test_PartyHP(0), 1);

    RunPoisonWhiteout();

    // The mon died from the poison: DoPoisonFieldEffect took its last HP, which
    // is the branch that leads into the whiteout at all.
    TEST_EQ(Test_PartyHP(0), 0);

    // The whole point of the test. Pre-fix the chain stops at the first
    // `waitstate` -- the fade-out special is unregistered, so the task that
    // re-enables the script context is never created and the field stays locked
    // on a black screen for good. Asserting on the *engine's* end state rather
    // than on frame counts: the player is respawned and has control again.
    TEST_GE(AdvanceWhiteout(6000), 0);
    TEST_EQ(Test_FieldLocked(), FALSE);

    // And the respawn really ran rather than the script bailing out early:
    // DoWhiteOut warps to the last heal location and heals the party.
    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F));
    TEST_EQ(Test_MapNum(), MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F));
    TEST_EQ(Test_PartyHP(0), Test_PartyMaxHP(0));
}

// The money-loss half of the chain is a separate special (373) on the
// "you have money" branch, so it needs its own coverage: with it unregistered
// the same waitstate stops the context and the branch soft-locks identically.
FIRERED_TEST("whiteout/losing money on a whiteout does not lock the field",
             "engine fixture:lab whiteout", whiteout_money_loss_recovers)
{
    u32 moneyBefore;
    u32 moneyAfter;

    Test_RequireFixture(FIXTURE_OAKS_LAB);

    TEST_GE(Test_PartyCount(), 1);
    TEST_GE(Test_PartyMaxHP(0), 2);

    SetLastHealLocationWarp(HEAL_LOCATION_PALLET_TOWN);
    SetMoney(&gSaveBlock1Ptr->money, 3000);
    moneyBefore = Test_Money();
    TEST_EQ(moneyBefore, 3000);   // the "HasMoney" branch needs money to take

    PoisonLeadMonAtOneHp();

    RunPoisonWhiteout();
    TEST_GE(AdvanceWhiteout(6000), 0);
    TEST_EQ(Test_FieldLocked(), FALSE);
    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F));

    // ComputeWhiteOutMoneyLoss: highest party level x 4 x the 0-badge
    // multiplier (2), capped at the player's money. Derived from the level the
    // engine reports rather than hardcoding Lv5, so the assertion survives a
    // fixture change; the `>= 2` guard above keeps it from being a no-op.
    moneyAfter = Test_Money();
    TEST_TRUE(moneyAfter < moneyBefore);
    TEST_EQ(moneyAfter, moneyBefore - (u32)Test_PartyLevel(0) * 4 * 2);
}
