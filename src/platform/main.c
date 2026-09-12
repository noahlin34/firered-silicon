#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <execinfo.h>
#include <SDL.h>
#include "global.h"
#include "platform/platform.h"
#include "global.fieldmap.h"

static void CrashHandler(int sig)
{
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    char **strs = backtrace_symbols(callstack, frames);
    fprintf(stderr, "\n[FATAL] Caught signal %d (%s)!\n", sig, strsignal(sig));
    if (strs)
    {
        for (int i = 0; i < frames; ++i)
            fprintf(stderr, "  %s\n", strs[i]);
        free(strs);
    }
    exit(sig);
}

extern void AgbMain(void);

// Simple smoke test to verify GBA Hardware Abstraction Layer on Apple Silicon
static void TestBiosSyscalls(void)
{
    printf("[SmokeTest] Testing BIOS Syscalls...\n");

    // 1. Test Div
    assert(Div(100, 5) == 20);
    assert(Div(-42, 7) == -6);
    assert(Div(123, 0) == 0); // Safe div-by-zero check

    // 2. Test Sqrt
    assert(Sqrt(144) == 12);
    assert(Sqrt(0) == 0);
    assert(Sqrt(1000000) == 1000);

    // 3. Test ArcTan2
    assert(ArcTan2(1, 0) == 0);

    // 4. Test CpuSet 16-bit copy & fill
    uint16_t src16[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint16_t dst16[8] = {0};
    CpuSet(src16, dst16, 8);
    for (int i = 0; i < 8; i++)
        assert(dst16[i] == src16[i]);

    uint16_t fill16 = 0xABCD;
    CpuSet(&fill16, dst16, CPU_SET_SRC_FIXED | 8);
    for (int i = 0; i < 8; i++)
        assert(dst16[i] == 0xABCD);

    // 5. Test CpuFastSet 32-bit chunk fill
    uint32_t fill32 = 0x12345678;
    uint32_t dst32[16] = {0};
    CpuFastSet(&fill32, dst32, CPU_FAST_SET_SRC_FIXED | 16);
    for (int i = 0; i < 16; i++)
        assert(dst32[i] == 0x12345678);

    // 6. Test LZ77 decompression
    // LZ77 stream for "HELLO HELLO "
    // Header: 0x10, len=12 (0x0C, 0x00, 0x00)
    // Flags: 0x00 (8 literal bytes) -> 'H', 'E', 'L', 'L', 'O', ' ', 'H', 'E'
    // Flags: 0x80 (1 compressed block, 7 literal/unused) -> copy 4 bytes from offset 6 ("LLO ")
    static const uint8_t testLzData[] = {
        0x10, 0x0C, 0x00, 0x00, // Header: 12 bytes
        0x00, 'H', 'E', 'L', 'L', 'O', ' ', 'H', 'E', // Literal block
        0x80, 0x10, 0x05, // Copy length (1+3)=4, offset (5+1)=6
    };
    uint8_t lzOut[16] = {0};
    LZ77UnCompWram(testLzData, lzOut);
    assert(memcmp(lzOut, "HELLO HELLO ", 12) == 0);

    printf("[SmokeTest] All BIOS Syscalls passed!\n");
}

static void TestGbaMemory(void)
{
    printf("[SmokeTest] Testing GBA Memory Buffers & Registers...\n");

    // Test writing to REG_DISPCNT
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON;
    assert(REG_DISPCNT == (DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON));

    // Test writing to BG_PLTT
    uint16_t *palette = (uint16_t *)BG_PLTT;
    palette[0] = RGB_RED;
    palette[1] = RGB_GREEN;
    palette[2] = RGB_BLUE;
    assert(((uint16_t *)BG_PLTT)[0] == RGB_RED);
    assert(((uint16_t *)BG_PLTT)[1] == RGB_GREEN);
    assert(((uint16_t *)BG_PLTT)[2] == RGB_BLUE);

    // Test DMA macro
    uint16_t dmaSrc[4] = {0x1111, 0x2222, 0x3333, 0x4444};
    uint16_t dmaDst[4] = {0};
    DmaCopy16(3, dmaSrc, dmaDst, sizeof(dmaSrc));
    for (int i = 0; i < 4; i++)
        assert(dmaDst[i] == dmaSrc[i]);

    DmaFill16(3, 0x7777, dmaDst, sizeof(dmaDst));
    for (int i = 0; i < 4; i++)
        assert(dmaDst[i] == 0x7777);

    printf("[SmokeTest] GBA Memory and DMA tests passed!\n");
}
extern const struct MapHeader PalletTown_PlayersHouse_1F;
extern const struct MapHeader PalletTown_PlayersHouse_2F;
extern const u8 EventScript_Bookshelf[];
extern const u8 EventScript_Cabinet[];
extern const u8 EventScript_Dresser[];
extern const u8 EventScript_Kitchen[];
extern const u8 EventScript_PlayerFacingTVScreen[];
extern const u8 EventScript_CancelMessageBox[];
extern const u8 PalletTown_PlayersHouse_1F_EventScript_TV[];
extern const u8 PalletTown_PlayersHouse_2F_EventScript_NES[];
extern const u8 PalletTown_PlayersHouse_2F_EventScript_Sign[];
extern const void *const gNativeScriptPtrs[];
extern const struct MapHeader PalletTown_ProfessorOaksLab;
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Aide1[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Aide2[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Aide3[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_BulbasaurBall[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_SquirtleBall[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_CharmanderBall[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Rival[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Pokedex[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Computer[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_LeftSign[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_RightSign[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_ProfOak[];
extern const u8 PalletTown_ProfessorOaksLab_ChooseStarterScene[];
extern const struct MapHeader PalletTown;
extern const u8 PalletTown_EventScript_OakTriggerLeft[];
extern const u8 PalletTown_EventScript_OakTriggerRight[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_RivalBattleTriggerLeft[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_RivalBattleTriggerMid[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_RivalBattleTriggerRight[];
extern const struct MapHeader ViridianCity_Mart;
extern const struct MapHeader ViridianCity;
extern const struct MapHeader Route1;
extern const u8 ViridianCity_Mart_OnLoad[];
extern const u8 ViridianCity_Mart_EventScript_ParcelScene[];
extern const u8 ViridianCity_Mart_EventScript_Clerk[];
extern const u8 ViridianCity_Mart_EventScript_HideQuestionnaire[];
extern const u8 ViridianCity_Mart_Items[];
extern const u8 ViridianCity_EventScript_RoadBlocked[];
extern const u8 ViridianCity_EventScript_CitySign[];
extern const u8 Route1_EventScript_MartClerk[];
extern const u8 Route1_EventScript_RouteSign[];

// Native script arrays are emitted without a public length. These smoke
// checks only need to locate a short opcode sequence inside a small script, so
// a bounded scan is enough and avoids exporting sizes from the generator.
static int FindScriptPattern(const u8 *script, const u8 *pattern, int patternLen)
{
    const int maxScan = 512;
    for (int i = 0; i < maxScan; i++)
    {
        int match = 1;
        for (int j = 0; j < patternLen; j++)
        {
            if (script[i + j] != pattern[j])
            {
                match = 0;
                break;
            }
        }
        if (match)
            return i;
    }
    return -1;
}

static void TestOverworldInteractions(void)
{
    printf("[SmokeTest] Testing Overworld Object & Background Event Scripts...\n");

    // 1. Check 1F Background Events (TV)
    const struct MapEvents *events1F = PalletTown_PlayersHouse_1F.events;
    assert(events1F != NULL);
    assert(events1F->bgEventCount >= 1);
    const struct BgEvent *tvEvent = &events1F->bgEvents[0];
    assert(tvEvent->x == 6 && tvEvent->y == 1);
    assert(tvEvent->bgUnion.script == PalletTown_PlayersHouse_1F_EventScript_TV);
    assert(tvEvent->bgUnion.script[0] == 0x69); // lockall

    // 2. Check 2F Background Events (NES, PC, Sign)
    const struct MapEvents *events2F = PalletTown_PlayersHouse_2F.events;
    assert(events2F != NULL);
    assert(events2F->bgEventCount >= 3);
    const struct BgEvent *nesEvent = &events2F->bgEvents[0];
    assert(nesEvent->x == 6 && nesEvent->y == 5);
    assert(nesEvent->bgUnion.script == PalletTown_PlayersHouse_2F_EventScript_NES);
    assert(nesEvent->bgUnion.script[0] == 0x69); // lockall

    const struct BgEvent *signEvent = &events2F->bgEvents[2];
    assert(signEvent->x == 11 && signEvent->y == 1);
    assert(signEvent->bgUnion.script == PalletTown_PlayersHouse_2F_EventScript_Sign);
    assert(signEvent->bgUnion.script[0] == 0x69); // lockall

    // 3. Check Metatile scripts (Bookshelf, Cabinet, Dresser, Kitchen, TV screen)
    assert(EventScript_Bookshelf[0] == 0x69);
    assert(EventScript_Cabinet[0] == 0x69);
    assert(EventScript_Dresser[0] == 0x69);
    assert(EventScript_Kitchen[0] == 0x69);
    assert(EventScript_PlayerFacingTVScreen[0] == 0x69);

    // 4. Check that message pointer operands resolve to non-NULL via gNativeScriptPtrs
    uint32_t bookshelfMsgIdx = EventScript_Bookshelf[2] | (EventScript_Bookshelf[3] << 8) |
                              (EventScript_Bookshelf[4] << 16) | (EventScript_Bookshelf[5] << 24);
    assert(gNativeScriptPtrs[bookshelfMsgIdx] != NULL);

    uint32_t nesMsgIdx = PalletTown_PlayersHouse_2F_EventScript_NES[2] | (PalletTown_PlayersHouse_2F_EventScript_NES[3] << 8) |
                         (PalletTown_PlayersHouse_2F_EventScript_NES[4] << 16) | (PalletTown_PlayersHouse_2F_EventScript_NES[5] << 24);
    assert(gNativeScriptPtrs[nesMsgIdx] != NULL);

    // 5. Walk-away cancel must run the real script (special DoPicboxCancel,
    // release, end), not the dummy `end` that left the signpost box on screen.
    assert(EventScript_CancelMessageBox[0] == 0x25); // special
    assert(EventScript_CancelMessageBox[3] == 0x6c); // release
    assert(EventScript_CancelMessageBox[4] == 0x02); // end

    // 6. Oak's Lab: every reachable A-press target must run its real script,
    //    and the map-script header tables must drive the starter scene.
    //    The interception cutscene leaves the scene var at 1, so ON_FRAME
    //    fires ChooseStarterScene on the first idle frame in the lab.
    {
        const struct MapEvents *labEvents = PalletTown_ProfessorOaksLab.events;
        assert(labEvents != NULL);
        assert(labEvents->objectEventCount == 10);
        assert(labEvents->bgEventCount == 4);

        const u8 *const expectedObjects[] = {
            PalletTown_ProfessorOaksLab_EventScript_Aide1,   // localId 1
            PalletTown_ProfessorOaksLab_EventScript_Aide3,   // localId 2
            PalletTown_ProfessorOaksLab_EventScript_Aide2,   // localId 3
            PalletTown_ProfessorOaksLab_EventScript_ProfOak, // localId 4
            PalletTown_ProfessorOaksLab_EventScript_BulbasaurBall,  // localId 5
            PalletTown_ProfessorOaksLab_EventScript_SquirtleBall,   // localId 6
            PalletTown_ProfessorOaksLab_EventScript_CharmanderBall, // localId 7
            PalletTown_ProfessorOaksLab_EventScript_Rival,          // localId 8
            PalletTown_ProfessorOaksLab_EventScript_Pokedex,        // localId 9
            PalletTown_ProfessorOaksLab_EventScript_Pokedex,        // localId 10
        };
        for (int i = 0; i < 10; i++)
            assert(labEvents->objectEvents[i].script == expectedObjects[i]);

        // NPCs and the rival lock the field first; the item balls ask first.
        assert(PalletTown_ProfessorOaksLab_EventScript_Aide1[0] == 0x6a); // lock
        assert(PalletTown_ProfessorOaksLab_EventScript_Aide2[0] == 0x6a);
        assert(PalletTown_ProfessorOaksLab_EventScript_Aide3[0] == 0x6a);
        assert(PalletTown_ProfessorOaksLab_EventScript_Rival[0] == 0x6a);
        assert(PalletTown_ProfessorOaksLab_EventScript_ProfOak[0] == 0x6a);
        for (int i = 4; i <= 6; i++)
            assert(labEvents->objectEvents[i].script[0] == 0x6a); // lock

        // Background events: both computer terminals and the two signs.
        const u8 *const expectedBg[] = {
            PalletTown_ProfessorOaksLab_EventScript_Computer,  // (2,1)
            PalletTown_ProfessorOaksLab_EventScript_Computer,  // (3,1)
            PalletTown_ProfessorOaksLab_EventScript_LeftSign,  // (6,1)
            PalletTown_ProfessorOaksLab_EventScript_RightSign, // (7,1)
        };
        for (int i = 0; i < 4; i++)
        {
            assert(labEvents->bgEvents[i].bgUnion.script == expectedBg[i]);
            assert(labEvents->bgEvents[i].bgUnion.script[0] == 0x69); // lockall
        }

        // The first aide's msgbox operand must resolve through the pointer table.
        uint32_t aideMsgIdx = PalletTown_ProfessorOaksLab_EventScript_Aide1[12] |
                              (PalletTown_ProfessorOaksLab_EventScript_Aide1[13] << 8) |
                              (PalletTown_ProfessorOaksLab_EventScript_Aide1[14] << 16) |
                              (PalletTown_ProfessorOaksLab_EventScript_Aide1[15] << 24);
        assert(PalletTown_ProfessorOaksLab_EventScript_Aide1[11] == 0x67); // message
        assert(gNativeScriptPtrs[aideMsgIdx] != NULL);

        // Map-script header tables: ON_TRANSITION / ON_WARP_INTO_MAP_TABLE /
        // ON_FRAME_TABLE, and the frame table's scene-1 entry must resolve to
        // ChooseStarterScene (the scene that walks Oak in and opens the
        // starter choice).
        const u8 *labMapScripts = PalletTown_ProfessorOaksLab.mapScripts;
        assert(labMapScripts != NULL);
        assert(labMapScripts[0] == 0x03);  // MAP_SCRIPT_ON_TRANSITION
        assert(labMapScripts[5] == 0x04);  // MAP_SCRIPT_ON_WARP_INTO_MAP_TABLE
        assert(labMapScripts[10] == 0x02); // MAP_SCRIPT_ON_FRAME_TABLE

        uint32_t onFrameIdx = labMapScripts[11] | (labMapScripts[12] << 8) |
                              (labMapScripts[13] << 16) | (labMapScripts[14] << 24);
        const u8 *onFrameTable = (const u8 *)gNativeScriptPtrs[onFrameIdx];
        assert(onFrameTable != NULL);
        // map_script_2 VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB, 1, ...
        assert(onFrameTable[0] == 0x55 && onFrameTable[1] == 0x40);
        assert(onFrameTable[2] == 0x01 && onFrameTable[3] == 0x00);
        uint32_t starterIdx = onFrameTable[4] | (onFrameTable[5] << 8) |
                              (onFrameTable[6] << 16) | (onFrameTable[7] << 24);
        assert(gNativeScriptPtrs[starterIdx] == PalletTown_ProfessorOaksLab_ChooseStarterScene);

        // ChooseStarterScene starts with lockall and ends by setting the scene
        // var to 2, which is what makes the starter balls answer the player.
        assert(PalletTown_ProfessorOaksLab_ChooseStarterScene[0] == 0x69); // lockall
    }

    // 7. Check Pallet Town Coord Events (Oak exit interception triggers)
    {
        const struct MapEvents *ptEvents = PalletTown.events;
        assert(ptEvents != NULL);
        assert(ptEvents->coordEventCount == 3);
        assert(ptEvents->coordEvents[0].x == 12 && ptEvents->coordEvents[0].y == 1);
        assert(ptEvents->coordEvents[0].script == PalletTown_EventScript_OakTriggerLeft);
        assert(ptEvents->coordEvents[0].script[0] == 0x69); // lockall

        assert(ptEvents->coordEvents[1].x == 13 && ptEvents->coordEvents[1].y == 1);
        assert(ptEvents->coordEvents[1].script == PalletTown_EventScript_OakTriggerRight);
        assert(ptEvents->coordEvents[1].script[0] == 0x69); // lockall

        uint32_t triggerIdx = PalletTown_EventScript_OakTriggerLeft[7] |
                              (PalletTown_EventScript_OakTriggerLeft[8] << 8) |
                              (PalletTown_EventScript_OakTriggerLeft[9] << 16) |
                              (PalletTown_EventScript_OakTriggerLeft[10] << 24);
        const u8 *oakTriggerScript = (const u8 *)gNativeScriptPtrs[triggerIdx];
        assert(oakTriggerScript != NULL);
        assert(oakTriggerScript[0] == 0x16); // setvar (famechecker)
    }

    // 8. Oak's Parcel chain: the Mart's ON_LOAD/ON_FRAME header tables must be
    //    compiled (they are what hands over ITEM_OAKS_PARCEL and advances the
    //    lab scene to 5), and the route/city A-press targets must be real.
    {
        const u8 *martMapScripts = ViridianCity_Mart.mapScripts;
        assert(martMapScripts != NULL);
        assert(martMapScripts[0] == 0x01);  // MAP_SCRIPT_ON_LOAD
        assert(martMapScripts[5] == 0x02);  // MAP_SCRIPT_ON_FRAME_TABLE

        uint32_t onLoadIdx = martMapScripts[1] | (martMapScripts[2] << 8) |
                             (martMapScripts[3] << 16) | (martMapScripts[4] << 24);
        assert(gNativeScriptPtrs[onLoadIdx] == ViridianCity_Mart_OnLoad);

        uint32_t onFrameIdx = martMapScripts[6] | (martMapScripts[7] << 8) |
                              (martMapScripts[8] << 16) | (martMapScripts[9] << 24);
        const u8 *martOnFrame = (const u8 *)gNativeScriptPtrs[onFrameIdx];
        assert(martOnFrame != NULL);
        // map_script_2 VAR_MAP_SCENE_VIRIDIAN_CITY_MART, 0, ParcelScene
        assert(martOnFrame[0] == 0x57 && martOnFrame[1] == 0x40);
        assert(martOnFrame[2] == 0x00 && martOnFrame[3] == 0x00);
        uint32_t parcelIdx = martOnFrame[4] | (martOnFrame[5] << 8) |
                             (martOnFrame[6] << 16) | (martOnFrame[7] << 24);
        assert(gNativeScriptPtrs[parcelIdx] == ViridianCity_Mart_EventScript_ParcelScene);

        // The parcel scene hands over the parcel and advances Oak's lab scene
        // to 5 (setvar VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB, 5) before
        // releaseall/end.
        assert(ViridianCity_Mart_EventScript_ParcelScene[0] == 0x69); // lockall
        {
            const u8 labSceneVar5[] = { 0x16, 0x55, 0x40, 0x05, 0x00 };
            assert(FindScriptPattern(ViridianCity_Mart_EventScript_ParcelScene,
                                     labSceneVar5, sizeof(labSceneVar5)) >= 0);
            const u8 giveParcel[] = { 0x44, 0x5d, 0x01, 0x01, 0x00 };
            assert(FindScriptPattern(ViridianCity_Mart_EventScript_ParcelScene,
                                     giveParcel, sizeof(giveParcel)) >= 0);
        }

        // ON_LOAD hides the questionnaire counter via setmetatile (0xa2).
        assert(ViridianCity_Mart_EventScript_HideQuestionnaire[0] == 0xa2);

        // The clerk's pokemart operand is the item list, which must start with
        // real items and terminate with ITEM_NONE.
        assert(ViridianCity_Mart_EventScript_Clerk[0] == 0x6a); // lock
        int clerkPokemart = -1;
        for (int i = 0; i < 512; i++)
        {
            if (ViridianCity_Mart_EventScript_Clerk[i] == 0x86) // pokemart
            {
                clerkPokemart = i;
                break;
            }
        }
        assert(clerkPokemart >= 0);
        uint32_t itemsIdx = ViridianCity_Mart_EventScript_Clerk[clerkPokemart + 1] |
                            (ViridianCity_Mart_EventScript_Clerk[clerkPokemart + 2] << 8) |
                            (ViridianCity_Mart_EventScript_Clerk[clerkPokemart + 3] << 16) |
                            (ViridianCity_Mart_EventScript_Clerk[clerkPokemart + 4] << 24);
        assert(gNativeScriptPtrs[itemsIdx] == ViridianCity_Mart_Items);
        // ITEM_POKE_BALL, ITEM_POTION, ITEM_ANTIDOTE, ITEM_PARALYZE_HEAL, ITEM_NONE
        assert(ViridianCity_Mart_Items[0] == 0x04 && ViridianCity_Mart_Items[1] == 0x00);
        assert(ViridianCity_Mart_Items[2] == 0x0d && ViridianCity_Mart_Items[3] == 0x00);
        assert(ViridianCity_Mart_Items[8] == 0x00 && ViridianCity_Mart_Items[9] == 0x00);

        // Viridian City's ON_TRANSITION places the tutorial old man; its signs,
        // the road-block trigger and the free-Potion clerk are all real.
        const u8 *cityMapScripts = ViridianCity.mapScripts;
        assert(cityMapScripts != NULL);
        assert(cityMapScripts[0] == 0x03); // MAP_SCRIPT_ON_TRANSITION
        assert(ViridianCity.events->coordEventCount == 4);
        assert(ViridianCity.events->coordEvents[0].script == ViridianCity_EventScript_RoadBlocked);
        assert(ViridianCity_EventScript_RoadBlocked[0] == 0x69); // lockall
        assert(ViridianCity.events->bgEvents[3].bgUnion.script == ViridianCity_EventScript_CitySign);

        assert(Route1.mapScripts != NULL);
        assert(Route1_EventScript_MartClerk[0] == 0x6a); // lock
        assert(Route1_EventScript_RouteSign[0] == 0x69); // lockall
    }

    // 9. Leaving Oak's Lab with a starter (scene 3) must start the rival battle.
    //    The three coord events at (5..7, 8) stop the player and run the
    //    trigger scripts; a NULL here is walked straight over, and the rival
    //    battle never happens.
    {
        const struct MapEvents *labEvents = PalletTown_ProfessorOaksLab.events;
        const u8 *const expectedTriggers[] = {
            PalletTown_ProfessorOaksLab_EventScript_RivalBattleTriggerLeft,  // (5,8)
            PalletTown_ProfessorOaksLab_EventScript_RivalBattleTriggerMid,   // (6,8)
            PalletTown_ProfessorOaksLab_EventScript_RivalBattleTriggerRight, // (7,8)
        };
        assert(labEvents->coordEventCount == 6);
        for (int i = 0; i < 3; i++)
        {
            const struct CoordEvent *coord = &labEvents->coordEvents[3 + i];
            assert(coord->x == 5 + i && coord->y == 8);
            assert(coord->trigger == VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB);
            assert(coord->index == 3);
            assert(coord->script == expectedTriggers[i]);
            assert(coord->script[0] == 0x69); // lockall
        }

        // Each trigger sets VAR_TEMP_2 (1/2/3, the player's column) so the
        // rival's approach path matches, then jumps to the shared battle script.
        for (int i = 0; i < 3; i++)
        {
            assert(expectedTriggers[i][0] == 0x69); // lockall
            assert(expectedTriggers[i][1] == 0x16); // setvar
            assert(expectedTriggers[i][2] == 0x02 && expectedTriggers[i][3] == 0x40); // VAR_TEMP_2
            assert(expectedTriggers[i][4] == i + 1 && expectedTriggers[i][5] == 0x00);
            assert(expectedTriggers[i][6] == 0x05); // goto
        }

        // The trigger's goto resolves through the pointer table to the shared
        // battle script, which dispatches on VAR_STARTER_MON (0x4031) with one
        // goto_if_eq per starter. This is the wiring the scene depends on: the
        // trigger stops the player, and the shared script picks the rival's
        // counter-pick approach.
        uint32_t battleIdx = expectedTriggers[1][7] | (expectedTriggers[1][8] << 8) |
                             (expectedTriggers[1][9] << 16) | (expectedTriggers[1][10] << 24);
        const u8 *battleScript = (const u8 *)gNativeScriptPtrs[battleIdx];
        assert(battleScript != NULL);
        assert(battleScript[0] == 0xc7); // textcolor
        // compare VAR_STARTER_MON against 0/1/2, each followed by goto_if_eq.
        for (int starter = 0; starter < 3; starter++)
        {
            const u8 cmp[] = { 0x21, 0x31, 0x40, (u8)starter, 0x00 };
            assert(FindScriptPattern(battleScript, cmp, sizeof(cmp)) >= 0);
        }
    }

    printf("[SmokeTest] All Overworld Object & Background Event Scripts verified!\n");
}


static void SetupPpuTestScene(void)
{
    printf("[SmokeTest] Setting up simulated GBA Scene (BG0 Tilemap + OAM Sprite)...\n");

    // 1. Setup Palettes
    uint16_t *bgPal = (uint16_t *)BG_PLTT;
    bgPal[0] = RGB(3, 5, 12);  // Backdrop: deep navy
    bgPal[1] = RGB(28, 5, 5);  // FireRed brick red
    bgPal[2] = RGB(31, 31, 31);// White
    bgPal[3] = RGB(4, 4, 6);   // Shadow black

    uint16_t *objPal = (uint16_t *)OBJ_PLTT;
    objPal[1] = RGB(31, 28, 4); // Yellow
    objPal[2] = RGB(31, 14, 2); // Orange / Charizard orange
    objPal[3] = RGB(31, 31, 31);// White highlight

    // 2. Setup Background 4bpp 8x8 Tile (Tile index 1 in CharBase 0)
    // 32 bytes for 8x8 4bpp tile: 2 pixels per byte
    uint8_t *bgTile = (uint8_t *)BG_CHAR_ADDR(0) + 32;
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            if (row == 0 || row == 7 || col == 0)
                bgTile[row * 4 + col] = 0x33; // Border
            else
                bgTile[row * 4 + col] = 0x11; // Interior brick
        }
    }

    // 3. Setup ScreenBase 28 Tilemap (offset 28 * 0x800 = 0xE000, safe from CharBase 0)
    uint16_t *bgMap = (uint16_t *)BG_SCREEN_ADDR(28);
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            // Checkerboard pattern of Tile 0 (backdrop) and Tile 1 (FireRed brick)
            if ((x >= 2 && x < 28) && (y >= 2 && y < 18) && ((x + y) % 2 == 0))
                bgMap[y * 32 + x] = 1; // Tile 1, palette 0
            else
                bgMap[y * 32 + x] = 0; // Empty
        }
    }

    // Configure BG0 with ScreenBase 28 and CharBase 0
    REG_BG0CNT = BGCNT_PRIORITY(1) | BGCNT_CHARBASE(0) | BGCNT_SCREENBASE(28) | BGCNT_16COLOR | BGCNT_TXT256x256;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    // 4. Setup Sprite Tile in OBJ VRAM (Tile 0)
    // Diamond icon in 4bpp
    uint8_t *objTile = (uint8_t *)OBJ_VRAM0;
    memset(objTile, 0, 32);
    // Draw an 8x8 diamond in colors 1 & 2
    objTile[1 * 4 + 1] = 0x01;
    objTile[1 * 4 + 2] = 0x10;
    objTile[2 * 4 + 1] = 0x22;
    objTile[2 * 4 + 2] = 0x22;
    objTile[3 * 4 + 0] = 0x01;
    objTile[3 * 4 + 1] = 0x22;
    objTile[3 * 4 + 2] = 0x22;
    objTile[3 * 4 + 3] = 0x10;
    objTile[4 * 4 + 0] = 0x01;
    objTile[4 * 4 + 1] = 0x22;
    objTile[4 * 4 + 2] = 0x22;
    objTile[4 * 4 + 3] = 0x10;
    objTile[5 * 4 + 1] = 0x22;
    objTile[5 * 4 + 2] = 0x22;
    objTile[6 * 4 + 1] = 0x01;
    objTile[6 * 4 + 2] = 0x10;

    // 5. Setup OAM Sprite 0 (centered at 116, 76)
    struct OamData *oam = (struct OamData *)OAM_;
    memset(oam, 0, sizeof(struct OamData) * 128);
    // Hide all sprites by default
    for (int i = 0; i < 128; i++)
        oam[i].affineMode = ST_OAM_AFFINE_OFF;

    oam[0].y = 76;
    oam[0].affineMode = ST_OAM_AFFINE_OFF;
    oam[0].objMode = ST_OAM_OBJ_NORMAL;
    oam[0].bpp = ST_OAM_4BPP;
    oam[0].shape = ST_OAM_SQUARE;
    oam[0].x = 116;
    oam[0].size = ST_OAM_SIZE_0; // 8x8
    oam[0].tileNum = 0;
    oam[0].priority = 0; // Front layer
    oam[0].paletteNum = 0;

    // Enable BG0 and OBJ in Mode 0
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGBUS, CrashHandler);
    signal(SIGSEGV, CrashHandler);
    printf("=========================================\n");
    printf(" Pokemon FireRed - Apple Silicon Native  \n");
    printf(" Architecture: ARM64 Mach-O             \n");
    printf(" Subsystem: Software GBA PPU Compositor  \n");
    printf("=========================================\n");

    TestBiosSyscalls();
    TestGbaMemory();

    TestOverworldInteractions();
    if (Platform_Init(argc, argv) != 0)
    {
        fprintf(stderr, "Failed to initialize platform window.\n");
        return 1;
    }
    if (argc > 1 && strcmp(argv[1], "--test") == 0)
    {
        SetupPpuTestScene();
        printf("[SmokeTest] Rendering simulated GBA scene through PPU...\n");
        for (int frame = 0; frame < 120; frame++)
        {
            Platform_UpdateInput();
            REG_BG0HOFS = frame / 2;
            Platform_RenderAndPresent();
        }
        Platform_SaveScreenshot("ppu_test_output.bmp");
        printf("[SmokeTest] PPU rendered 120 frames successfully!\n");
        Platform_Cleanup();
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--boot-test") == 0)
        {
            extern int gEngineMaxFrames;
            gEngineMaxFrames = (i + 1 < argc && argv[i + 1][0] != '-') ? atoi(argv[++i]) : 240;
            printf("[Engine] Running boot test for %d frames...\n", gEngineMaxFrames);
        }
        else if (strcmp(argv[i], "--skip-intro") == 0)
        {
            gPlatformSkipIntro = true;
            printf("[Engine] Dev boot: fresh save, spawning in the player's bedroom (intro skipped).\n");
        }
    }

    printf("[Engine] Booting Pokemon FireRed CPU Engine (AgbMain)...\n");
    AgbMain();
    Platform_Cleanup();
    return 0;
}
