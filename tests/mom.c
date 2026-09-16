// Mom (PalletTown_PlayersHouse_1F, localId 1 at (8,4)) and her two branches.
//
// Mom's script picks its branch on FLAG_BEAT_RIVAL_IN_OAKS_LAB: before the first
// rival battle she has the "PROF. OAK was looking for you" line, and afterwards
// she offers a rest and heals the party. The two fixtures sit on either side of
// that flag (bedroom = unset, lab = set), so each branch gets its own fixture
// rather than one test writing the flag and testing half the script.
//
// Both are real interactions: warp beside her, turn to face her, press A. The
// heal is asserted on party HP, which is what a player observes.

#include "internal.h"
#include "constants/flags.h"
#include "constants/maps.h"
#include "constants/pokemon.h"

// Male player on a fresh save, rival battle not yet fought.
//
// The gendered variants matter: this is a `checkplayergender` branch inside the
// script, so a regression that lost the dispatch would print the girls' line.
FIRERED_TEST("mom/standard dialogue picks the player's gender line",
             "engine fixture:bedroom npc:mom", mom_standard_dialogue)
{
    Test_RequireFixture(FIXTURE_BEDROOM);

    TEST_EQ(Test_Flag(FLAG_BEAT_RIVAL_IN_OAKS_LAB), FALSE);

    // Mom stands at (8,4); (8,5) is the same metatile one tile south of her.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F),
                MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F), 8, 5);
    Test_RunFramesToWarp();

    Test_Press(DPAD_UP);        // turn to face her
    Test_RunFrames(3);
    TEST_EQ(Test_PlayerFacing(), DIR_NORTH);

    Test_Press(A_BUTTON);
    Test_RunFrames(30);         // let the message box write into gStringVar4

    TEST_EQ(Test_LastTalked(), 1);
    // Decoded form: '\l' renders as the <scroll> marker and '\p' as a blank
    // line, since both are layout escapes rather than printable characters.
    TEST_STR_EQ(Test_StringVar4(),
                "MOM: …Right.\n"
                "All boys leave home someday.<scroll>It said so on TV.\n"
                "\n"
                "Oh, yes. PROF. OAK, next door, was\n"
                "looking for you.");

    // The interaction holds the field until the box is dismissed.
    TEST_EQ(Test_FieldLocked(), TRUE);

    Test_PressUntilIdle(A_BUTTON, 900);
    TEST_EQ(Test_FieldLocked(), FALSE);
}

// After the rival battle the same press offers a rest and heals the party.
//
// Asserted on HP, not on the script's shape: the player's observable is that his
// damaged mon is back to full. HealPlayerParty is special index 0, the one
// special this port implements natively, so this exercises it end to end.
FIRERED_TEST("mom/heals the party after the rival battle",
             "engine fixture:lab npc:mom", mom_heal)
{
    Test_RequireFixture(FIXTURE_OAKS_LAB);

    TEST_EQ(Test_Flag(FLAG_BEAT_RIVAL_IN_OAKS_LAB), TRUE);
    TEST_EQ(Test_PartyCount(), 1);

    // Precondition: a battle won at the cost of HP.
    Test_SetPartyHP(0, 1);
    TEST_EQ(Test_PartyHP(0), 1);
    TEST_GE(Test_PartyMaxHP(0), 2);   // or the heal below proves nothing

    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F),
                MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F), 8, 5);
    Test_RunFramesToWarp();

    Test_Press(DPAD_UP);
    Test_RunFrames(3);
    Test_Press(A_BUTTON);
    Test_RunFrames(30);

    TEST_EQ(Test_LastTalked(), 1);
    TEST_STR_EQ(Test_StringVar4(),
                "MOM: RED!\n"
                "You should take a quick rest.");

    Test_PressUntilIdle(A_BUTTON, 900);

    // The party is healed, and the field is handed back.
    TEST_EQ(Test_PartyHP(0), Test_PartyMaxHP(0));
    TEST_EQ(Test_FieldLocked(), FALSE);
}
