// Field-effect scripts.
//
// Pure. `data/field_effect_scripts.s` cannot be assembled for the host, so
// tools/gen_field_effect_data.py compiles it to native C byte-for-byte, with every
// 4-byte address operand stored as a u32 INDEX into gNativeFieldEffectPtrs
// (AGENTS.md fix #49). Which effects are emitted is derived by scanning the linked
// sources, so an effect whose native is not linked stays NULL and simply does not
// start -- which is the correct behaviour, not a bug.
//
// The tall-grass rustle is the effect the player actually sees on every step into
// grass, so its whole chain is asserted: the script must decode to
// `loadfadedpal_callnative <palette>, <native>, end` with operands that resolve to
// the real palette and the real native.

#include "registry.h"

#include "constants/field_effects.h"
#include "field_effect.h"
#include "fldeff.h"
#include "task.h"

extern const u8 *const gFieldEffectScriptPointers[];
extern const void *const gNativeFieldEffectPtrs[];
extern const struct SpritePalette gSpritePalette_GeneralFieldEffect1;
extern u32 FldEff_TallGrass(void);
#define FLDEFF_OP_LOADFADEDPAL_CALLNATIVE 7
#define FLDEFF_OP_CALLNATIVE              3
#define FLDEFF_OP_END                     4

FIRERED_TEST("field-effects/tall grass script resolves palette and native",
         "field-effects pure", fldeff_tall_grass)
{
    const u8 *script = gFieldEffectScriptPointers[FLDEFF_TALL_GRASS];

    TEST_PTR_NOT_NULL(script);

    // loadfadedpal_callnative = opcode(1) + palette(4) + native(4) + end(1).
    TEST_EQ(script[0], FLDEFF_OP_LOADFADEDPAL_CALLNATIVE);
    TEST_NE(script[9], 0);   // terminates rather than running on
    TEST_PTR_EQ((const struct SpritePalette *)gNativeFieldEffectPtrs[T2_READ_32(&script[1])],
                &gSpritePalette_GeneralFieldEffect1);
    TEST_PTR_EQ((u32 (*)(void))gNativeFieldEffectPtrs[T2_READ_32(&script[5])],
                FldEff_TallGrass);
}

// The rest of the grass family shares that template/native path. If a re-run of
// the generator stopped resolving one of these (a deleted stub, an unlinked
// native), stepping into that tile would silently stop rustling.
FIRERED_TEST("field-effects/grass family is ported", "field-effects pure", fldeff_grass_family)
{
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_SHORT_GRASS]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_LONG_GRASS]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_JUMP_TALL_GRASS]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_DUST]);
}

// The field moves. Each of these was NULL while its src/fldeff_*.c was
// unlinked, which is exactly what "the move does nothing" looked like: the
// party menu reports the move usable, the script runs `dofieldeffect`, and
// FieldEffectStart finds no script so it returns without adding the id to the
// active list -- the `waitstate` that follows then has nothing to wait for.
// Linking the seven sources resolved all eight (cut has two ids: tree and
// grass), plus the two halves of the cut's grass-metatile sweep.
//
// Assert both halves of each entry: a real script AND a native that resolves.
// A non-NULL script whose `callnative` operand is NULL would be jumped through.
FIRERED_TEST("field-effects/field moves are ported", "field-effects pure",
             fldeff_field_moves)
{
    static const u8 ids[] = {
        FLDEFF_USE_CUT_ON_TREE,
        FLDEFF_USE_CUT_ON_GRASS,
        FLDEFF_CUT_GRASS,
        FLDEFF_USE_ROCK_SMASH,
        FLDEFF_USE_DIG,
        FLDEFF_USE_STRENGTH,
        FLDEFF_USE_TELEPORT,
        FLDEFF_SWEET_SCENT,
    };
    int i;

    for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++)
    {
        const u8 *script = gFieldEffectScriptPointers[ids[i]];
        int nativeOffset;

        TEST_PTR_NOT_NULL(script);
        if (script == NULL)
            continue;   // the operand reads below would fault on a NULL script

        // Two shapes are used here, and the native sits at a different offset in
        // each (see OPCODES in tools/gen_field_effect_data.py):
        //   callnative               = opcode + 4-byte native + end  -> native at 1
        //   loadfadedpal_callnative  = opcode + 4-byte pal + native + end -> at 5
        switch (script[0])
        {
        case FLDEFF_OP_CALLNATIVE:
            nativeOffset = 1;
            break;
        case FLDEFF_OP_LOADFADEDPAL_CALLNATIVE:
            nativeOffset = 5;
            break;
        default:
            TEST_FAIL("unexpected field-effect opcode %d", script[0]);
            continue;
        }

        // The script must terminate rather than run on into its neighbour's
        // bytes: `end` follows the native operand.
        TEST_EQ(script[nativeOffset + 4], FLDEFF_OP_END);
        TEST_PTR_NOT_NULL(gNativeFieldEffectPtrs[T2_READ_32(&script[nativeOffset])]);
    }
}

// The show-mon task the field moves go through is shared, and its callback
// pointer is stored with SetPointerTaskArg rather than pret's two-slot split
// (fix #26/#66: a 32-bit slot cannot hold a host function address). Assert the
// round trip, because a truncating store would only fail when the cleanup runs
// -- i.e. inside FLDEFF_CALL_FUNC_IN_DATA, one frame after the move.
static void TestTaskFunc(u8 taskId);

static void TestTaskFunc(u8 taskId)
{
    (void)taskId;
}

FIRERED_TEST("field-effects/callback survives the task slot round trip",
             "field-effects pure", fldeff_callback_roundtrip)
{
    u8 taskId = CreateTask(TestTaskFunc, 0);

    FLDEFF_SET_FUNC_TO_DATA(TestTaskFunc);
    TEST_PTR_EQ(GetPointerTaskArg(taskId, 8), TestTaskFunc);
    // The store must stay inside the 32-byte data[] array: it writes
    // sizeof(void*) = 8 bytes from data[8], i.e. data[8..11].
    TEST_TRUE(8 + sizeof(void *) / sizeof(s16) <= 16);
    DestroyTask(taskId);
}
