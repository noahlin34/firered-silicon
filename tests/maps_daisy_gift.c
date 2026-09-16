#include "internal.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/vars.h"
#include "script.h"

FIRERED_TEST("daisy/gives the town map after the parcel", "engine fixture:lab npc:daisy",
             daisy_town_map)
{
    // The gift branch is gated on the parcel having been delivered, which the lab
    // fixture does not arrange: write the precondition the way a real playthrough
    // leaves it, then test only the interaction.
    Test_SetVar(VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE, 1);

    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN_RIVALS_HOUSE),
                MAP_NUM(MAP_PALLET_TOWN_RIVALS_HOUSE), 5, 5);
    Test_RunFramesToWarp();

    Test_Press(DPAD_UP);              // turn to face Daisy
    Test_RunFrames(3);
    Test_PressUntilIdle(A_BUTTON, 900);

    TEST_TRUE(Test_HasItem(ITEM_TOWN_MAP, 1));
    TEST_EQ(Test_Var(VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE), 2);
    TEST_EQ(Test_Flag(FLAG_HIDE_TOWN_MAP), TRUE);
    TEST_EQ(Test_LastTalked(), 1);
}
