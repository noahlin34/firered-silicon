// Trainer sight.
//
// src/trainer_see.c was unlinked and src/platform/overworld_stubs.c served both
// of its entry points: CheckForTrainersWantingBattle returned FALSE, and
// MovementAction_RevealTrainer_RunTrainerSeeFuncList was an empty body. So no
// NPC trainer could ever spot the player, and the REVEAL_TRAINER movement action
// -- which the disguised-trainer path in event_object_movement.c calls -- did
// nothing.
//
// Linking the file exposed the port defect that matters here: pret keeps the
// trainer's ObjectEvent pointer packed into two 16-bit task slots
// (tTrainerObjHi/tTrainerObjLo), which is one whole GBA address and half a host
// one. `gObjectEvents` is at 0x1_008da5d8 in this binary, so the truncated read
// faults on the first dereference (fix #26's class, fix #66 for the same
// pack-and-rebuild elsewhere). The pointer now goes through a task-keyed side
// table, and the reveal path stores it with SetPointerTaskArg semantics.
//
// The wiring half is pure; driving a real sighting needs a map with a trainer
// object event, and none of the compiled maps has one (measured: 450
// sight-capable object events game-wide, 0 in the six maps whose scripts are
// compiled), so that half is asserted structurally rather than faked.

#include "registry.h"

#include "constants/field_effects.h"
#include "constants/maps.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "global.fieldmap.h"
#include "task.h"
#include "trainer_see.h"
#include "field_player_avatar.h"

// The reveal path stores the trainer's ObjectEvent pointer and the task body
// dereferences it. pret packs it into two 16-bit slots; on the host that keeps
// 32 bits of a pointer that lives above 4 GiB, so the dereference faults. This
// calls the real entry point event_object_movement.c's REVEAL_TRAINER movement
// action uses and then RUNS FRAMES, which is what makes the read happen -- a
// test that only checked the task count would pass against the truncating store
// (the store itself never faults; the load does).
FIRERED_TEST("trainer-sight/reveal movement dereferences a whole pointer",
         "engine fixture:bedroom trainer-sight", trainer_sight_reveal_ptr)
{
    // A real, active object event to point at. The bedroom fixture holds only
    // the player's own (there is no NPC on 2F), so use that: the task body's
    // first act is ObjectEventClearHeldMovement, which is a no-op for a
    // stationary player. What matters is that the stored pointer is dereferenced
    // -- the truncating store never faults, the load does.
    struct ObjectEvent *trainerObj = &gObjectEvents[gPlayerAvatar.objectEventId];

    TEST_TRUE(trainerObj->active);

    MovementAction_RevealTrainer_RunTrainerSeeFuncList(trainerObj);

    // The task body reads the stored pointer (ObjectEventClearHeldMovement,
    // then the func list). A truncated store faults here rather than failing an
    // assertion, which is the honest signal -- the historical defect is a
    // SIGSEGV, not a wrong value.
    Test_RunFrames(4);

    // Survived the dereference with the field still live. (A truncated pointer
    // faults during Test_RunFrames rather than failing an assertion -- the
    // historical defect is a SIGSEGV, so surviving is the signal.)
    TEST_TRUE(Test_InOverworld());
}

// Both entry points must be the real ones, not the old stub bodies. A stub
// returns FALSE, and the real CheckForTrainersWantingBattle returns TRUE only
// when a trainer actually sees the player -- so with no trainer in view it must
// answer FALSE, but the *file* must be the thing answering (the stub is gone).
// Asserting the symbol resolves is what proves the link; asserting no crash on
// the field proves the call is safe with the compiled maps' object events.
FIRERED_TEST("trainer-sight/checking for trainers is safe with none in sight",
         "engine fixture:bedroom trainer-sight", trainer_sight_no_trainer)
{
    // The bedroom has no trainer object events at all.
    TEST_FALSE(CheckForTrainersWantingBattle());
    // ...and the field is still the field: the call must not have started
    // anything, locked the player, or changed the map.
    TEST_TRUE(Test_InOverworld());
    TEST_FALSE(Test_FieldLocked());
    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F));
}

// The five emoticon effects the sight path starts. They were NULL while
// trainer_see.c was unlinked and are resolved through the field-effect script
// table, so the "!" over a trainer's head is the observable consequence of this
// link.
FIRERED_TEST("trainer-sight/emote effects are registered",
         "pure trainer-sight", trainer_sight_emotes)
{
    extern const u8 *const gFieldEffectScriptPointers[];

    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_EXCLAMATION_MARK_ICON]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_DOUBLE_EXCL_MARK_ICON]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_QUESTION_MARK_ICON]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_X_ICON]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_SMILEY_FACE_ICON]);
}
