#ifndef GUARD_FLDEFF_H
#define GUARD_FLDEFF_H

// The pending field-effect callback lives in the task's data[8], which is 32
// bytes in total. The GBA stored the 32-bit address across two s16 slots; on a
// 64-bit host that keeps only half of it, so the callback (a .text pointer well
// above 4 GiB) would be jumped into truncated (fix #66's class). The whole
// pointer goes through SetPointerTaskArg/GetPointerTaskArg (fix #26) at index
// 8, which writes sizeof(void *) bytes from data[8] -- inside the array, and
// clear of every field these tasks use (the show-mon task keeps no other data).
#define FLDEFF_CALL_FUNC_IN_DATA() \
((void (*)(void))GetPointerTaskArg(taskId, 8))();

#define FLDEFF_SET_FUNC_TO_DATA(func) \
SetPointerTaskArg(taskId, 8, (void *)(uintptr_t)(func));

extern struct MapPosition gPlayerFacingPosition;

bool8 CheckObjectGraphicsInFrontOfPlayer(u8 graphicsId);
u8 CreateFieldEffectShowMon(void);

// flash
u8 MapTransitionIsExit(u8 lightLevel, u8 mapType);
u8 MapTransitionIsEnter(u8 mapType1, u8 mapType2);
bool8 SetUpFieldMove_Flash(void);
void CB2_DoChangeMap(void);

// cut
bool8 SetUpFieldMove_Cut(void);

// dig
bool8 SetUpFieldMove_Dig(void);
bool8 FldEff_UseDig(void);

// rocksmash
bool8 SetUpFieldMove_RockSmash(void);
bool8 FldEff_UseRockSmash(void);

// berrytree
void nullsub_56(void);

// poison
void FldEffPoison_Start(void);
bool32 FldEffPoison_IsActive(void);

// strength
bool8 SetUpFieldMove_Strength(void);
bool8 FldEff_UseStrength(void);

// teleport
bool8 SetUpFieldMove_Teleport(void);
bool8 FldEff_UseTeleport(void);

// softboiled
bool8 SetUpFieldMove_SoftBoiled(void);
void ChooseMonForSoftboiled(u8 taskId);
void Task_TryUseSoftboiledOnPartyMon(u8 taskId);

// sweetscent
bool8 SetUpFieldMove_SweetScent(void);
bool8 FldEff_SweetScent(void);

#endif // GUARD_FLDEFF_H
