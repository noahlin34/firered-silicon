#ifndef GUARD_M4A_H
#define GUARD_M4A_H

#include "gba/m4a_internal.h"

void m4aSoundVSync(void);
void m4aSoundVSyncOn(void);

void m4aSoundInit(void);
void m4aSoundMain(void);
void m4aSongNumStart(u16 n);
void m4aSongNumStartOrChange(u16 n);
void m4aSongNumStop(u16 n);
void m4aMPlayAllStop(void);
void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo);
void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed);
void m4aMPlayFadeOutTemporarily(struct MusicPlayerInfo *mplayInfo, u16 speed);
void m4aMPlayFadeIn(struct MusicPlayerInfo *mplayInfo, u16 speed);
void m4aMPlayImmInit(struct MusicPlayerInfo *mplayInfo);

extern struct MusicPlayerInfo gMPlayInfo_BGM;
extern struct MusicPlayerInfo gMPlayInfo_SE1;
extern struct MusicPlayerInfo gMPlayInfo_SE2;
extern struct MusicPlayerInfo gMPlayInfo_SE3;
extern struct SoundInfo gSoundInfo;

extern const struct SongHeader mus_victory_gym_leader;

/* Song bytecode pointer operands. A generated song stores a u32 index into
 * gNativeSongPtrs (a 4-byte operand cannot hold a host address -- fix #23's
 * treatment); the runtime-built cry songs store one into
 * gNativeSongRuntimePtrs instead. ply_goto resolves both. */
extern const void *const gNativeSongPtrs[];
extern u8 *gNativeSongRuntimePtrs[];

/* The game's cry tables (data/sound/cry_table.h equivalent). One ToneData per
 * species, built by tools/gen_sound_data.py from sound/cry_tables.inc. */
extern const struct ToneData gCryTable[];
extern const struct ToneData gCryTable_Reverse[];

#endif //GUARD_M4A_H
