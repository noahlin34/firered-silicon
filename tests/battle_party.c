// The in-battle POKéMON menu (choose a mon to switch in, and cancel back out).
//
// Cancelling used to crash the game. The player's controller emits the choice
// through `BtlController_EmitChosenMonReturnValue`, whose third operand is the
// party order to adopt; cancelling passes NULL for it ("the order did not
// change"), and that function copied the array unconditionally:
//
//     for (i = 0; i < ARRAY_COUNT(gBattlePartyCurrentOrder); i++)
//         sBattleBuffersTransferData[2 + i] = battlePartyOrder[i];
//
// On the GBA address 0 is readable BIOS, so the read returned garbage that
// nothing consumes on the cancel path (the buffer's bytes 2-4 are only read for
// MULTI battles, and a cancel leaves the party order alone). On the host it is
// an unmapped page, so cancelling the in-battle party menu faulted inside the
// controller that emits the answer -- the battle screen was still up and the
// menu closed correctly, which is why it looked like "closing the menu crashes".
//
// The test drives the real path: a wild battle, DOWN+A on the action menu to
// pick POKéMON, then B on the party menu. What it asserts is that the battle
// survives it -- the party menu hands the frame back to BattleMainCB2 and the
// action menu comes up again for the same turn.
//
// `fixture:lab` because a battle needs a booted save with a party; the battle is
// started from the test rather than by walking into grass so the test is not
// also asserting the encounter roll.

#include "registry.h"

#include "constants/maps.h"
#include "constants/party_menu.h"
#include "constants/items.h"
#include "constants/pokemon.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "gba/io_reg.h"
#include "main.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokemon.h"
#include "script_pokemon_util.h"

// Advances the engine until `condition` holds, or gives up after maxFrames.
// Returns the number of frames it took, or -1 if the condition never held.
#define RUN_UNTIL(condition, maxFrames)                        \
    ({                                                         \
        int _f = -1;                                           \
        int _limit = (maxFrames);                              \
        while (_limit-- > 0)                                   \
        {                                                      \
            if (condition)                                     \
            {                                                  \
                _f = (maxFrames) - _limit;                     \
                break;                                         \
            }                                                  \
            Test_RunFrames(1);                                 \
        }                                                      \
        _f;                                                    \
    })

// The action menu is up and the battle wants the player's choice. The engine
// signals that by scrolling BG0 to the action-menu page; there is no flag for it.
static bool8 BattleWantsAction(void)
{
    return gMain.callback2 == BattleMainCB2 && gBattle_BG0_Y == 160 && !gPaletteFade.active;
}

// The party menu owns the frame (it replaced the battle's callback2).
static bool8 PartyMenuIsUp(void)
{
    return gMain.callback2 != BattleMainCB2
        && gPartyMenu.menuType == PARTY_MENU_TYPE_IN_BATTLE
        && !gPaletteFade.active;
}

FIRERED_TEST("battle/cancelling the party menu returns to the battle",
             "engine fixture:lab battle", battle_party_menu_cancel)
{
    int i;

    Test_RequireFixture(FIXTURE_OAKS_LAB);
    TEST_GE(Test_PartyCount(), 1);

    // A scripted wild battle, the same setup ScrCmd_dowildbattle performs.
    CreateScriptedWildMon(SPECIES_PIDGEY, 5, 0);
    StartScriptedWildBattle();

    // The intro prints several boxes, each waiting on a JOY_NEW press edge, so
    // advance with a press/release cadence rather than a held key.
    for (i = 0; i < 400 && !BattleWantsAction(); i++)
    {
        Test_Press(A_BUTTON);
        Test_RunFrames(12);
    }
    TEST_TRUE(BattleWantsAction());
    TEST_EQ(gMain.callback1 == CB1_Overworld, FALSE);   // the battle owns callback1
    TEST_TRUE(gBattleTypeFlags & BATTLE_TYPE_WILD_SCRIPTED);

    // DOWN from FIGHT moves the action cursor to POKéMON (index 2), A picks it.
    Test_Press(DPAD_DOWN);
    Test_RunFrames(4);
    TEST_EQ(gActionSelectionCursor[0], 2);
    Test_Press(A_BUTTON);

    // The controller fades out and opens the party menu to choose a mon.
    TEST_GE(RUN_UNTIL(gPartyMenu.menuType == PARTY_MENU_TYPE_IN_BATTLE, 400), 0);
    TEST_EQ(gPartyMenu.action, PARTY_ACTION_CHOOSE_MON);
    TEST_GE(RUN_UNTIL(PartyMenuIsUp(), 400), 0);

    // Cancel. This is the press that used to fault: the menu closes, the battle
    // screen is rebuilt, and the controller emits the cancelled choice.
    Test_Press(B_BUTTON);
    TEST_GE(RUN_UNTIL(gMain.callback2 == BattleMainCB2, 900), 0);
    Test_RunFrames(60);

    // Back in the battle, on the same turn, waiting for an action again. Both
    // halves matter: reaching BattleMainCB2 proves the reshow completed, and the
    // action menu proves the cancelled choice did not end the battle.
    TEST_TRUE(BattleWantsAction());
    TEST_EQ(gBattlersCount, 2);
    TEST_GE(Test_PartyHP(0), 1);
}

// The companion path: choosing a mon actually switches it in. Cancel and choose
// share every step up to `WaitForMonSelection`, so a fix that merely stopped the
// fault could still leave the real answer broken.
//
// A second party mon is required: switching to the mon already in battle is
// refused (gText_PkmnAlreadyInBattle), which is a message, not a completed turn.
FIRERED_TEST("battle/switching a mon from the party menu completes the turn",
             "engine fixture:lab battle", battle_party_menu_switch)
{
    int hpBefore;
    u16 speciesAtBattleStart;
    int i;

    Test_RequireFixture(FIXTURE_OAKS_LAB);

    // The rival's counter-pick left one mon; add a second so the menu offers a
    // real switch rather than "already in battle".
    TEST_EQ(ScriptGiveMon(SPECIES_PIDGEY, 5, ITEM_NONE, 0, 0, 0), MON_GIVEN_TO_PARTY);
    TEST_GE(Test_PartyCount(), 2);

    CreateScriptedWildMon(SPECIES_RATTATA, 5, 0);
    StartScriptedWildBattle();

    for (i = 0; i < 400 && !BattleWantsAction(); i++)
    {
        Test_Press(A_BUTTON);
        Test_RunFrames(12);
    }
    TEST_TRUE(BattleWantsAction());

    Test_Press(DPAD_DOWN);          // POKéMON
    Test_RunFrames(4);
    Test_Press(A_BUTTON);
    TEST_GE(RUN_UNTIL(PartyMenuIsUp(), 500), 0);

    // Move the cursor off the mon that is in battle, then A opens its action
    // window ("SHIFT" for an in-battle party menu); A again picks the top entry,
    // which switches it in and closes the menu back into the battle.
    Test_Press(DPAD_DOWN);
    Test_RunFrames(4);
    Test_Press(A_BUTTON);
    Test_RunFrames(30);
    hpBefore = Test_PartyHP(0);
    speciesAtBattleStart = GetMonData(&gPlayerParty[gBattlerPartyIndexes[0]], MON_DATA_SPECIES);
    Test_Press(A_BUTTON);
    TEST_GE(RUN_UNTIL(gMain.callback2 == BattleMainCB2, 900), 0);

    // The turn runs -- the wild mon gets its move in -- and the battle is still
    // playable.
    Test_RunFrames(1200);
    TEST_EQ(gBattlersCount, 2);
    TEST_EQ(gMain.callback1 == CB1_Overworld, FALSE);
    TEST_GE(hpBefore, 1);
    TEST_GE(Test_PartyCount(), 2);

    // And the switch actually happened: the mon on the field is the one the menu
    // offered, not the one that started the battle. Asserted through the battler's
    // party index, which is what the engine's own healthbox and switch-in read --
    // "the battle survived" alone would pass even if the answer were discarded.
    TEST_NE(speciesAtBattleStart, SPECIES_PIDGEY);
    TEST_EQ(GetMonData(&gPlayerParty[gBattlerPartyIndexes[0]], MON_DATA_SPECIES), SPECIES_PIDGEY);
}
