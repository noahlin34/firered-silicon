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

extern const u8 *const gFieldEffectScriptPointers[];
extern const void *const gNativeFieldEffectPtrs[];
extern const struct SpritePalette gSpritePalette_GeneralFieldEffect1;
extern u32 FldEff_TallGrass(void);

// Opcodes in the field-effect dialect (data/field_effect_scripts.s).
#define FLDEFF_OP_LOADFADEDPAL_CALLNATIVE 7
#define FLDEFF_OP_END                     4

OMP_TEST("field-effects/tall grass script resolves palette and native",
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
OMP_TEST("field-effects/grass family is ported", "field-effects pure", fldeff_grass_family)
{
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_SHORT_GRASS]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_LONG_GRASS]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_JUMP_TALL_GRASS]);
    TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[FLDEFF_DUST]);
}

// An effect whose native is not linked must be NULL, not a bogus script: a
// non-NULL entry with a NULL native would be jumped through and crash.
// `src/fldeff_cut.c` is not linked, so the cut-on-grass effect is the known gap.
OMP_TEST("field-effects/unported effects are inert rather than bogus",
         "field-effects pure", fldeff_unported)
{
    TEST_PTR_EQ(gFieldEffectScriptPointers[FLDEFF_USE_CUT_ON_GRASS], NULL);
}
