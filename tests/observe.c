// Observation: read engine state, decode its text, render on demand.
//
// Everything here reads symbols the engine already exports. Tests use the real
// headers (via registry.h -> global.h) rather than restating prototypes: a
// hand-written extern with a wrong type compiles and links silently, which is
// the failure class AGENTS.md fixes #57/#58 document.

#include <stdio.h>
#include <string.h>

#include "internal.h"

#include "constants/flags.h"
#include "constants/items.h"
#include "constants/vars.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "item.h"
#include "main.h"
#include "money.h"
#include "overworld.h"
#include "platform/ppu.h"
#include "pokemon.h"
#include "script.h"
#include "string_util.h"

// --- vars / flags / inventory ----------------------------------------------
u16 Test_Var(u16 varId) { return VarGet(varId); }

void Test_SetVar(u16 varId, u16 value) { VarSet(varId, value); }

bool8 Test_Flag(u16 flagId) { return FlagGet(flagId); }

void Test_SetFlag(u16 flagId, bool8 value)
{
    if (value)
        FlagSet(flagId);
    else
        FlagClear(flagId);
}

bool8 Test_HasItem(u16 itemId, u16 count) { return CheckBagHasItem(itemId, count); }

u32 Test_Money(void) { return GetMoney(&gSaveBlock1Ptr->money); }

u16 Test_LastTalked(void) { return gSpecialVar_LastTalked; }

bool8 Test_FieldLocked(void) { return ArePlayerFieldControlsLocked(); }

// --- world position ---------------------------------------------------------
u16 Test_MapGroup(void) { return (u16)gSaveBlock1Ptr->location.mapGroup; }
u16 Test_MapNum(void) { return (u16)gSaveBlock1Ptr->location.mapNum; }

// Player position comes in two coordinate spaces and they are easy to confuse:
//
//   * SaveBlock1.pos is the on-screen camera coordinate. It is written by the
//     map load from the warp destination and updated as the player walks, so it
//     is the intuitive "where is the player in this map" value, starting at the
//     warp's own (x, y).
//   * ObjectEvent.currentCoords is map-grid space, which includes the 7-tile
//     border used for connections -- the same physical tile reads 7 higher in
//     both axes (MAP_OFFSET, include/fieldmap.h).
//
// Tests assert in the space a map's own data uses, i.e. pos.
s16 Test_PlayerX(void) { return gSaveBlock1Ptr->pos.x; }
s16 Test_PlayerY(void) { return gSaveBlock1Ptr->pos.y; }

// The raw object-event view, for tests that care about the avatar itself
// (sprite, facing direction, movement state) rather than map position.
const struct ObjectEvent *Test_PlayerObject(void)
{
    return &gObjectEvents[gPlayerAvatar.objectEventId];
}

u8 Test_PlayerFacing(void) { return Test_PlayerObject()->facingDirection; }
bool8 Test_PlayerIsMoving(void) { return Test_PlayerObject()->heldMovementActive; }

bool8 Test_InOverworld(void) { return gMain.callback1 == CB1_Overworld; }

// --- party ------------------------------------------------------------------
int Test_PartyCount(void) { return CalculatePlayerPartyCount(); }

u16 Test_PartySpecies(int index)
{
    if (index < 0 || index >= PARTY_SIZE)
        return SPECIES_NONE;
    if (!GetMonData(&gPlayerParty[index], MON_DATA_HP))
        return SPECIES_NONE;   // empty slot
    return GetMonData(&gPlayerParty[index], MON_DATA_SPECIES);
}

static u8 PartyLevel(int index)
{
    return (u8)GetMonData(&gPlayerParty[index], MON_DATA_LEVEL);
}

u8 Test_PartyLevel(int index) { return PartyLevel(index); }

u16 Test_PartyHP(int index)
{
    if (index < 0 || index >= PARTY_SIZE)
        return 0;
    return GetMonData(&gPlayerParty[index], MON_DATA_HP);
}

u16 Test_PartyMaxHP(int index)
{
    if (index < 0 || index >= PARTY_SIZE)
        return 0;
    return GetMonData(&gPlayerParty[index], MON_DATA_MAX_HP);
}

// Damage a mon so a heal has something to repair. Tests use this to write the
// precondition the way a real playthrough leaves it -- a battle won at the cost
// of HP -- and then assert only the interaction under test.
void Test_SetPartyHP(int index, u16 hp)
{
    if (index < 0 || index >= PARTY_SIZE)
        return;
    SetMonData(&gPlayerParty[index], MON_DATA_HP, &hp);
}
