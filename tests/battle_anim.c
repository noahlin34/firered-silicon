// 64-bit pointer storage in battle animation tasks and sprites.
//
// Battle animation code stores pointers in places sized for a GBA address. Two
// of those are live in this port and both truncate a host pointer, because the
// host's statics and heap are mapped above 4 GiB (gHeap at 0x1_00xxxxxx, the
// affine command tables at 0x1_004fxxxx):
//
//   * PrepareAffineAnimInTaskData -> StorePointerInVars -> two 16-bit task
//     slots, read back by RunAffineAnimFromTaskData.
//   * AnimShakeMonOrBattleTerrain (Rock Throw and friends) stores the address of
//     the register it wiggles in sprite->data[6..7] and rebuilds it as
//     `data[6] | (data[7] << 16)` before dereferencing it.
//
// Each test runs the real function and then asserts the *effect* the player
// sees: the affine animation advances through its script, and the shake moves
// the register it was pointed at. Asserting the pointer round-trips would be
// closer to the implementation, but these are the consumers that faulted.
//
// These helpers read battle state (gBattleSpritesDataPtr for the sprite's OAM
// matrix, the battler sprite ids for the coordinate-offset pass), so the engine
// is booted and that state allocated. They need no specific map, but the
// harness's cheapest boot is the bedroom fixture.

#include "registry.h"

#include "battle.h"
#include "battle_anim.h"
#include "battle_gfx_sfx_util.h"
#include "sprite.h"
#include "task.h"

// An affine script the task can walk. It is deliberately long: the runner
// advances by `data[7] << 3` *union* steps, so after the first frame it lands 8
// entries further into the table (that is pret's arithmetic, kept verbatim).
// Entry 8 therefore carries the second value asserted below.
// Held in .rodata (address above 4 GiB), which is what makes this a real test of
// the pointer width rather than of the arithmetic.
static const union AffineAnimCmd sTestAffineAnimCmds[] =
{
    AFFINEANIMCMD_FRAME(0x100, 0x100, 0, 1),   // entry 0: first call reads this
    AFFINEANIMCMD_FRAME(0x111, 0x111, 0, 1),
    AFFINEANIMCMD_FRAME(0x122, 0x122, 0, 1),
    AFFINEANIMCMD_FRAME(0x133, 0x133, 0, 1),
    AFFINEANIMCMD_FRAME(0x144, 0x144, 0, 1),
    AFFINEANIMCMD_FRAME(0x155, 0x155, 0, 1),
    AFFINEANIMCMD_FRAME(0x166, 0x166, 0, 1),
    AFFINEANIMCMD_FRAME(0x177, 0x177, 0, 1),
    AFFINEANIMCMD_FRAME(0x200, 0x200, 0, 1),   // entry 8: second call reads this
    AFFINEANIMCMD_END,
};

// The affine animation task must read its command table through the whole host
// address. Pre-fix the table pointer was truncated to its low 32 bits, which is
// unmapped, so RunAffineAnimFromTaskData faulted on the first `->type` read.
FIRERED_TEST("battle_anim/affine anim task reads its command table",
             "engine fixture:bedroom battle_anim", battle_anim_affine_task_pointer)
{
    u8 taskId, spriteId;
    struct Task *task;

    if (gBattleSpritesDataPtr == NULL)
        AllocateBattleSpritesData();

    taskId = CreateTask(TaskDummy, 0);
    task = &gTasks[taskId];
    spriteId = CreateSprite(&gDummySpriteTemplate, 0, 0, 0);
    TEST_NE(spriteId, MAX_SPRITES);

    PrepareAffineAnimInTaskData(task, spriteId, sTestAffineAnimCmds);

    // The scale starts at 0x100 and the first call adds entry 0. A truncated
    // table pointer faults here rather than returning a wrong number.
    RunAffineAnimFromTaskData(task);
    TEST_EQ(task->data[10], 0x100 + 0x100);
    TEST_EQ(task->data[11], 0x100 + 0x100);

    // The next call reads further into the same table, still through the same
    // pointer -- so the value is entry 8's, accumulated onto the running scale.
    RunAffineAnimFromTaskData(task);
    TEST_EQ(task->data[10], 0x100 + 0x100 + 0x200);
    TEST_EQ(task->data[11], 0x100 + 0x100 + 0x200);

    // And the script's END entry stops it (it is reached after another step). A
    // truncated pointer would not have survived the traversal above.
    DestroyTask(taskId);
}

// The same task, re-pointed at a second table, must follow the new table -- a
// side table keyed by task id that was never refreshed would keep animating the
// first one, and a caller can and does re-prepare mid-animation (Splash does).
FIRERED_TEST("battle_anim/affine anim task follows a re-pointed table",
             "engine fixture:bedroom battle_anim", battle_anim_affine_task_repoint)
{
    static const union AffineAnimCmd other[] =
    {
        AFFINEANIMCMD_FRAME(0x40, 0x40, 0, 1),
        AFFINEANIMCMD_FRAME(0x50, 0x50, 0, 1),
        AFFINEANIMCMD_FRAME(0x60, 0x60, 0, 1),
        AFFINEANIMCMD_FRAME(0x70, 0x70, 0, 1),
        AFFINEANIMCMD_FRAME(0x80, 0x80, 0, 1),
        AFFINEANIMCMD_FRAME(0x90, 0x90, 0, 1),
        AFFINEANIMCMD_FRAME(0xa0, 0xa0, 0, 1),
        AFFINEANIMCMD_FRAME(0xb0, 0xb0, 0, 1),
        AFFINEANIMCMD_FRAME(0xc0, 0xc0, 0, 1),
        AFFINEANIMCMD_END,
    };
    u8 taskId, spriteId;
    struct Task *task;

    if (gBattleSpritesDataPtr == NULL)
        AllocateBattleSpritesData();

    taskId = CreateTask(TaskDummy, 0);
    task = &gTasks[taskId];
    spriteId = CreateSprite(&gDummySpriteTemplate, 0, 0, 0);
    TEST_NE(spriteId, MAX_SPRITES);

    PrepareAffineAnimInTaskData(task, spriteId, sTestAffineAnimCmds);
    RunAffineAnimFromTaskData(task);
    TEST_EQ(task->data[10], 0x100 + 0x100);

    // Re-point at the other table and rewind, exactly as AnimTask_Splash does on
    // its loop-back branch. A stale side table would keep the first table's
    // numbers here.
    PrepareAffineAnimInTaskData(task, spriteId, other);
    RunAffineAnimFromTaskData(task);
    TEST_EQ(task->data[10], 0x100 + 0x40);
    TEST_EQ(task->data[11], 0x100 + 0x40);
    RunAffineAnimFromTaskData(task);
    TEST_EQ(task->data[10], 0x100 + 0x40 + 0xc0);   // entry 8 of the new table
    TEST_EQ(task->data[11], 0x100 + 0x40 + 0xc0);

    DestroyTask(taskId);
}

// AnimShakeMonOrBattleTerrain wiggles one of the battle's BG registers: it
// stores the register's address, reads the current value, then adds/subtracts on
// each step and restores the original when the duration runs out. Asserted
// through a real register (gBattle_BG3_Y, arg selector 1), which is what the
// move animation moves. The callback is static, so it is reached the way the
// engine reaches it -- through the sprite template the animation script names.
FIRERED_TEST("battle_anim/shake anim writes through the register pointer",
             "engine fixture:bedroom battle_anim", battle_anim_shake_pointer)
{
    extern const struct SpriteTemplate gShakeMonOrTerrainSpriteTemplate;
    const struct SpriteTemplate *template = &gShakeMonOrTerrainSpriteTemplate;
    u16 before;
    u8 spriteId;
    int moved = 0, steps;

    if (gBattleSpritesDataPtr == NULL)
        AllocateBattleSpritesData();

    TEST_PTR_NOT_NULL((const void *)template->callback);

    spriteId = CreateSprite(&gDummySpriteTemplate, 0, 0, 0);
    TEST_NE(spriteId, MAX_SPRITES);

    gBattle_BG3_Y = 100;
    before = gBattle_BG3_Y;

    // args: delta, interval, duration, selector, 0 (leave coord offsets alone)
    gBattleAnimArgs[0] = 4;
    gBattleAnimArgs[1] = 1;
    gBattleAnimArgs[2] = 3;
    gBattleAnimArgs[3] = 1;      // -> gBattle_BG3_Y
    gBattleAnimArgs[4] = 0;

    template->callback(&gSprites[spriteId]);

    // It stored the register's address and cached the value it found there.
    // Pre-fix the stored address had lost its top half, so this read faulted.
    TEST_EQ(gSprites[spriteId].data[4], before);
    TEST_EQ(gSprites[spriteId].data[5], 1);

    // The first call installs the step callback; drive that (not the template's
    // entry point, which would re-initialise the sprite each time) exactly as
    // the sprite engine does.
    for (steps = 0; steps < 12 && gSprites[spriteId].inUse; steps++)
    {
        gSprites[spriteId].callback(&gSprites[spriteId]);
        if (gBattle_BG3_Y != before)
            moved = 1;
    }

    TEST_EQ(moved, 1);                          // it wrote the named register
    TEST_EQ(gBattle_BG3_Y, before);             // and restored the original
    TEST_EQ(gSprites[spriteId].inUse, FALSE);   // the sprite destroyed itself
}
