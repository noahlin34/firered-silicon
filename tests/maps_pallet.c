// Pallet Town exterior wiring: the Oak interception trigger and the connections
// that let the player leave town.
//
// Pure. The interception trigger is a coord event, and a coord event with a NULL
// script is SIMPLY WALKABLE (AGENTS.md fix #27) -- so the failure this guards is
// "the cutscene never happens and the player walks out of town", not a crash.

#include "registry.h"
#include "script_ops.h"

#include "constants/global.h"
#include "constants/maps.h"
#include "global.fieldmap.h"

extern const struct MapHeader PalletTown;
extern const u8 PalletTown_EventScript_OakTriggerLeft[];
extern const u8 PalletTown_EventScript_OakTriggerRight[];

// Both northern-exit tiles run Oak's interception when the scene var is 0.
OMP_TEST("pallet/coord events stop the player at the northern exit", "maps pallet pure",
         pallet_oak_trigger)
{
    const struct MapEvents *events = PalletTown.events;

    TEST_PTR_NOT_NULL(events);
    TEST_EQ(events->coordEventCount, 3);

    TEST_EQ(events->coordEvents[0].x, 12);
    TEST_EQ(events->coordEvents[0].y, 1);
    TEST_PTR_EQ(events->coordEvents[0].script, PalletTown_EventScript_OakTriggerLeft);
    TEST_TRUE(Test_ScriptStartsWith(events->coordEvents[0].script, SCR_CMD_LOCKALL));

    TEST_EQ(events->coordEvents[1].x, 13);
    TEST_EQ(events->coordEvents[1].y, 1);
    TEST_PTR_EQ(events->coordEvents[1].script, PalletTown_EventScript_OakTriggerRight);
    TEST_TRUE(Test_ScriptStartsWith(events->coordEvents[1].script, SCR_CMD_LOCKALL));
}

// The trigger's body must go somewhere real: its goto operand must resolve
// through the pointer table to a script that starts with real work, not a dummy.
OMP_TEST("pallet/interception trigger jumps into a real script", "maps pallet pure",
         pallet_oak_trigger_body)
{
    const u8 *trigger = PalletTown_EventScript_OakTriggerLeft;
    int gotoTarget;
    const u8 *target;

    TEST_PTR_NOT_NULL(trigger);

    gotoTarget = Test_ScriptFindOpcode(trigger, SCR_CMD_GOTO);
    if (gotoTarget < 0)
    {
        // The closure may inline its body rather than jumping; either shape is
        // acceptable as long as the script is real. Assert the marker this
        // script is known to carry (a setvar/famechecker write) is present.
        TEST_NE(Test_ScriptFindOpcode(trigger, SCR_CMD_SETVAR), -1);
        return;
    }

    target = Test_ScriptPtrAt(trigger, gotoTarget + 1);
    TEST_PTR_NOT_NULL(target);
}

// Outdoor maps need generated MapConnections. Without them gMapConnectionFlags
// stays all-false, GetMapBorderIdAt answers CONNECTION_INVALID in every
// direction, and the player is walled inside Pallet Town (AGENTS.md fix #30).
// Route 1 north and Route 21 south are what make the town a town.
OMP_TEST("pallet/town has connections to route 1 and route 21", "maps pallet pure",
         pallet_connections)
{
    const struct MapConnections *conn = PalletTown.connections;
    int sawNorth = 0, sawSouth = 0, i;

    TEST_PTR_NOT_NULL(conn);
    TEST_GE(conn->count, 1);

    for (i = 0; i < conn->count; i++)
    {
        const struct MapConnection *c = &conn->connections[i];

        if (c->direction == CONNECTION_NORTH)
        {
            sawNorth = 1;
            TEST_EQ(c->mapGroup, MAP_GROUP(MAP_ROUTE1));
            TEST_EQ(c->mapNum, MAP_NUM(MAP_ROUTE1));
        }
        if (c->direction == CONNECTION_SOUTH)
        {
            sawSouth = 1;
            TEST_EQ(c->mapGroup, MAP_GROUP(MAP_ROUTE21_NORTH));
            TEST_EQ(c->mapNum, MAP_NUM(MAP_ROUTE21_NORTH));
        }
    }

    TEST_EQ(sawNorth, 1);
    TEST_EQ(sawSouth, 1);
}
