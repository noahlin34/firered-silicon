// Scripts the engine reaches through compiled dependencies: whiteout recovery,
// field poison and hidden-item pickup.
//
// Pure. Each of these was a `{0x02}` dummy in src/platform/overworld_stubs.c, so
// the engine set up a script that did nothing: a whiteout cleared the screen and
// never healed the party, and a hidden item could never be collected. They were
// only reachable once their supporting engine code was linked, which is why they
// surfaced late (AGENTS.md fix #57).

#include "registry.h"
#include "script_ops.h"

#include "constants/vars.h"

extern const u8 EventScript_AfterWhiteOutHeal[];
extern const u8 EventScript_AfterWhiteOutMomHeal[];
extern const u8 EventScript_PkmnCenterNurse_TakeAndHealPkmn[];
extern const u8 EventScript_FieldPoison[];
extern const u8 EventScript_HiddenItemScript[];
extern const u8 EventScript_TryPickUpHiddenItem[];
extern const u8 EventScript_PickedUpHiddenItem[];
extern u16 (*const gSpecials[])(void);

// Task_WhiteOut picks one of these by whether the player's last heal spot was
// their own house. Both were dummy scripts, so the whiteout never healed.
OMP_TEST("whiteout/recovery scripts are real", "scripts whiteout pure", whiteout_scripts)
{
    TEST_PTR_NOT_NULL(EventScript_AfterWhiteOutHeal);
    TEST_PTR_NOT_NULL(EventScript_AfterWhiteOutMomHeal);
    TEST_PTR_NOT_NULL(EventScript_PkmnCenterNurse_TakeAndHealPkmn);

    TEST_TRUE(Test_ScriptStartsWith(EventScript_AfterWhiteOutHeal, SCR_CMD_LOCKALL));
    TEST_TRUE(Test_ScriptStartsWith(EventScript_AfterWhiteOutMomHeal, SCR_CMD_LOCKALL));

    // The shared heal routine animates the player rather than opening with a
    // special; what matters is that it is compiled and reaches a special at all
    // (a `{0x02}` dummy has none).
    TEST_NE(Test_ScriptFindOpcode(EventScript_PkmnCenterNurse_TakeAndHealPkmn,
                                  SCR_CMD_SPECIAL), -1);
}

// The shared heal routine must really play its field effect: dofieldeffect then
// waitfieldeffect, in that order.
OMP_TEST("whiteout/heal routine plays its field effect", "scripts whiteout pure",
         whiteout_heal_effect)
{
    const u8 *s = EventScript_PkmnCenterNurse_TakeAndHealPkmn;
    int doAt = Test_ScriptFindOpcode(s, SCR_CMD_DOFIELDEFFECT);
    int waitAt = Test_ScriptFindOpcode(s, SCR_CMD_WAITFIELDEFFECT);

    TEST_GE(doAt, 0);
    TEST_GE(waitAt, 0);
    TEST_GE(waitAt, doAt);   // waits for the effect it started
}

// A poisoned mon fainting on a step runs its own whiteout path. Shape: lockall,
// textcolor, special TryFieldPoisonWhiteOut (199), waitstate, then the VAR_RESULT
// branch into the whiteout.
OMP_TEST("whiteout/field poison branches on the whiteout special",
         "scripts whiteout pure", whiteout_field_poison)
{
    const u8 *s = EventScript_FieldPoison;
    int specialAt;

    TEST_PTR_NOT_NULL(s);
    TEST_TRUE(Test_ScriptStartsWith(s, SCR_CMD_LOCKALL));

    specialAt = Test_ScriptFindOpcode(s, SCR_CMD_SPECIAL);
    TEST_GE(specialAt, 0);
    TEST_EQ(Test_ScriptHalfwordAt(s, specialAt + 1), 199);   // TryFieldPoisonWhiteOut
    TEST_NE(Test_ScriptFindOpcode(s, SCR_CMD_WAITSTATE), -1);

    // ...and the special must be registered, or ScrCmd_special reports
    // "special 199 is not ported", VAR_RESULT is never set, and the script
    // branches on garbage.
    TEST_PTR_NOT_NULL((const void *)gSpecials[199]);
}

// The pickup script grants the item; special 150 sets the flag. Without the flag
// the same spot can be dug up forever.
OMP_TEST("hidden-items/pickup script and its flag special are wired",
         "scripts whiteout pure", hidden_item_script)
{
    TEST_TRUE(Test_ScriptStartsWith(EventScript_HiddenItemScript, SCR_CMD_LOCKALL));
    TEST_TRUE(Test_ScriptStartsWith(EventScript_TryPickUpHiddenItem, SCR_CMD_ADDITEM));
    TEST_PTR_NOT_NULL((const void *)gSpecials[150]);   // SetHiddenItemFlag
}
