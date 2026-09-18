/* Portable C port of the m4a driver that the ROM build assembles from
 * src/m4a_1.s.
 *
 * The GBA build splits the sound driver in two: src/m4a.c (public API,
 * sequencer helpers, the CGB channel synthesiser, the mixer's data tables) and
 * src/m4a_1.s (the sequence interpreter, the PCM mixer, the channel-list
 * plumbing and the vblank/PCM-DMA handoff). The assembler half cannot be built
 * for macOS -- it is Thumb/ARM machine code that writes GBA registers directly
 * -- so this file transcribes it.
 *
 * Everything here has a named counterpart in src/m4a_1.s, and the arithmetic
 * matches it rather than being "equivalent". The mixer's 8-bit output, the
 * 23-bit interpolation fraction and the reverb feedback all depend on exact
 * bit behaviour: a 'cleaned up' mixer produces audibly different results
 * (wrong pitch, no DPCM, a permanent DC buzz from a mis-sized clear).
 *
 * Function names keep their assembler spelling because src/m4a.c reaches them
 * through gMPlayJumpTable by index (see gMPlayJumpTableTemplate in
 * src/m4a_tables.c) and those indices are part of the data format.
 *
 * The mixer writes into SoundInfo.pcmBuffer, which on the GBA DMA feeds the
 * two hardware FIFOs. Under PORTABLE the platform layer drains that same buffer
 * instead (Platform_SubmitAudioFrame, src/platform/sdl2.c), so the buffer's
 * layout -- two interleaved halves, `pcmSamplesPerVBlank` bytes per vblank --
 * is preserved exactly.
 */

#include "global.h"
#include "gba/m4a_internal.h"
#include "m4a.h"
#include "platform/platform.h"

/* Defined in src/m4a_tables.c. */
extern const u8 gClockTable[];

/* Defined in src/data/sound/song_data.h. Every song bytecode label a pointer
 * operand can name; the songs' 4-byte pointer operands hold an index into it
 * rather than an address (fix #23's treatment, applied to song bytecode). */
extern const void *const gNativeSongPtrs[];

/* Runtime-built songs (the Pokemon cries) whose loop targets only exist once
 * the song struct does. ply_goto resolves operands at or above
 * NATIVE_SONG_RUNTIME_BASE through this table. */
extern u8 *gNativeSongRuntimePtrs[MAX_POKEMON_CRIES];

/* Operand values at or above this cannot collide with a generated table index:
 * gNativeSongPtrs has ~1600 entries, so the top bit is free. */
#define NATIVE_SONG_RUNTIME_BASE 0x80000000u

/* ------------------------------------------------------------------------
 * Small helpers
 * ------------------------------------------------------------------------ */

/* umul3232H32: the top 32 bits of an unsigned 32x32 product. The assembler
 * reaches this through SWI 0x0B; on the host the compiler emits umulh. */
u32 umul3232H32(u32 multiplier, u32 multiplicand)
{
    return (u32)(((u64)multiplier * (u64)multiplicand) >> 32);
}

/* SoundMainBTM: clears 64 bytes. gMPlayJumpTable[35], and what Clear64byte
 * dispatches to. */
void SoundMainBTM(void *x)
{
    memset(x, 0, 64);
}

/* RealClearChain: unlinks one channel from its track's channel list.
 * gMPlayJumpTable[34].
 *
 * The chain pointers are `void *` in the struct (pret declares them that way
 * because the assembler never cared about the pointee); the casts below give
 * them their real type. */
void RealClearChain(void *x)
{
    struct SoundChannel *chan = x;
    struct MusicPlayerTrack *track = chan->track;
    struct SoundChannel *prev = chan->prevChannelPointer;
    struct SoundChannel *next = chan->nextChannelPointer;

    if (!track)
        return;

    if (prev)
        prev->nextChannelPointer = next;
    else
        track->chan = next;

    if (next)
        next->prevChannelPointer = prev;

    chan->track = NULL;
}

/* The jump-table template from src/m4a_tables.c. The ROM build reaches it
 * through SWI 0x2A, which copies it out of the BIOS; here it is ordinary rodata
 * and the copy is a plain memcpy. */
extern void *const gMPlayJumpTableTemplate[];

/* MPlayJumpTableCopy: fills gMPlayJumpTable from the template. The 36 entries
 * are the command dispatch table (indices 0..29), then SampleFreqSet,
 * TrackStop, FadeOutBody, TrkVolPitSet, RealClearChain and SoundMainBTM. */
void MPlayJumpTableCopy(MPlayFunc *mplayJumpTable)
{
    s32 i;

    for (i = 0; i < 36; i++)
        mplayJumpTable[i] = gMPlayJumpTableTemplate[i];
}

/* ClearChain / Clear64byte dispatch through the jump table, exactly as the
 * assembler does (index 34 is RealClearChain, 35 is SoundMainBTM). Going
 * through the table matters: MPlayExtender installs the real implementations
 * at those indices during m4aSoundInit. */
void ClearChain(void *x)
{
    ((void (*)(void *))gMPlayJumpTable[34])(x);
}

void Clear64byte(void *x)
{
    ((void (*)(void *))gMPlayJumpTable[35])(x);
}

/* ------------------------------------------------------------------------
 * TrackStop
 * ------------------------------------------------------------------------ */

/* Stops every channel on the track and clears its channel list. Deliberately
 * does not clear MPT_FLG_EXIST: the assembler leaves that to the caller. */
void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan;

    (void)mplayInfo;

    if (!(track->flags & MPT_FLG_EXIST))
        return;

    chan = track->chan;
    while (chan)
    {
        struct SoundChannel *next = chan->nextChannelPointer;

        if (chan->statusFlags != 0)
        {
            u8 cgbType = chan->type & TONEDATA_TYPE_CGB;

            if (cgbType)
                SOUND_INFO_PTR->CgbOscOff(cgbType);
            chan->statusFlags = 0;
        }
        chan->track = NULL;
        chan = next;
    }

    track->chan = NULL;
}

/* ------------------------------------------------------------------------
 * Volume, pan and pitch
 * ------------------------------------------------------------------------ */

/* ChnVolSetAsm: per-channel left/right volume from the voice velocity, the
 * track's rhythm pan and the track's volume multipliers. The `>> 14` mirrors
 * the assembler's `mul; asr #14`, not a scale choice of its own. */
static void ChnVolSetAsm(struct SoundChannel *chan, struct MusicPlayerTrack *track)
{
    s32 velocity = chan->velocity;
    s32 pan = (s8)chan->rhythmPan;
    s32 volume;

    volume = (0x80 + pan) * velocity;
    volume = (track->volMR * volume) >> 14;
    chan->rightVolume = volume > 0xFF ? 0xFF : (u8)volume;

    volume = (0x7F - pan) * velocity;
    volume = (track->volML * volume) >> 14;
    chan->leftVolume = volume > 0xFF ? 0xFF : (u8)volume;
}

/* ------------------------------------------------------------------------
 * ply_note: start a note on a channel
 * ------------------------------------------------------------------------ */

/* Starts a note. `note_cmd` is the NOTE byte minus 0xCF (so 1 is a 1-clock
 * note), which indexes gClockTable for the default gate time. The assembler
 * signature is `ply_note(u32 note_cmd, mplayInfo, track)` and it is reached
 * through SoundInfo.plynote, so the argument order is fixed. */
void ply_note(u32 note_cmd, struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    struct SoundChannel *chan;
    struct ToneData *tone;
    u8 key, velocity;
    u32 priority;
    u8 numChans;
    u8 cgbType;
    u8 rhythmPan = 0;
    const u8 *cmd;

    track->gateTime = gClockTable[note_cmd];

    /* Read the note's key/velocity/gate-time operands, which may be omitted
     * (a byte >= 0x80 ends them) to mean "same as last time". */
    cmd = track->cmdPtr;
    if (*cmd < 0x80)
    {
        key = *cmd++;
        if (*cmd < 0x80)
        {
            velocity = *cmd++;
            track->key = key;
            track->velocity = velocity;
            if (*cmd < 0x80)
                track->gateTime += *cmd++;
        }
        else
        {
            track->key = key;
        }
    }
    track->cmdPtr = (u8 *)cmd;

    /* Resolve the tone: a keysplit voice picks a sub-voice by key, and a
     * rhythm voice may carry the pan in its pan_sweep byte. */
    tone = &track->tone;
    if (tone->type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL))
    {
        u8 index;

        if (tone->type & TONEDATA_TYPE_SPL)
            index = tone->keySplitTable[track->key];
        else
            index = track->key;

        tone = &((struct ToneData *)tone->wav)[index];
        if (tone->type & (TONEDATA_TYPE_SPL | TONEDATA_TYPE_RHY))
            return;
        if (track->tone.type & TONEDATA_TYPE_RHY)
        {
            if (tone->pan_sweep & 0x80)
                rhythmPan = (tone->pan_sweep - TONEDATA_P_S_PAN) << 1;
        }
    }

    priority = mplayInfo->priority + track->priority;
    if (priority > 0xFF)
        priority = 0xFF;

    cgbType = tone->type & TONEDATA_TYPE_CGB;

    if (cgbType)
    {
        struct CgbChannel *cgb = soundInfo->cgbChans;

        if (!cgb)
            return;

        cgb += cgbType - 1;

        /* A CGB channel is only stolen if free, or if the new note's priority
         * beats the running one, or if the running one belongs to an earlier
         * track (the `bcs` compares two host pointers, which is what the
         * assembler's `bcs` on the raw addresses does too -- both are ordered
         * the same way within one allocation). */
        if (cgb->statusFlags & SOUND_CHANNEL_SF_ON)
        {
            if (!(cgb->statusFlags & SOUND_CHANNEL_SF_STOP))
            {
                if (cgb->priority < priority)
                    ;
                else if (cgb->priority != priority)
                    return;
                else if (cgb->track < track)
                    ;
                else
                    return;
            }
        }

        chan = (struct SoundChannel *)cgb;
    }
    else
    {
        /* Pick a PCM channel: prefer a free one, then the lowest-priority one.
         * `stealFirst` tracks whether a stopped channel has been seen, which
         * the assembler keeps in r2. */
        struct SoundChannel *best = NULL;
        u32 bestPriority = priority;
        u8 stealFirst = 0;
        s32 i;

        for (i = 0; i < soundInfo->maxChans; i++)
        {
            chan = &soundInfo->chans[i];

            if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON))
                break;

            if (chan->statusFlags & SOUND_CHANNEL_SF_STOP)
            {
                if (stealFirst == 0)
                {
                    stealFirst = 1;
                    best = chan;
                    bestPriority = chan->priority;
                }
            }
            else if (stealFirst == 0)
            {
                if (chan->priority < bestPriority)
                {
                    bestPriority = chan->priority;
                    best = chan;
                }
                else if (chan->priority == bestPriority && chan->track < track)
                {
                    best = chan;
                }
            }
        }

        if (i == soundInfo->maxChans)
        {
            if (!best)
                return;
            chan = best;
        }

        if (!chan)
            return;
    }

    ClearChain(chan);

    chan->prevChannelPointer = NULL;
    chan->nextChannelPointer = track->chan;
    if (track->chan)
        track->chan->prevChannelPointer = chan;
    track->chan = chan;
    chan->track = track;

    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelayC)
        ClearModM(track);

    TrkVolPitSet(mplayInfo, track);

    /* The assembler writes these four channel bytes with ONE word store:
     *
     *     ldr r0, [r5, o_MusicPlayerTrack_gateTime]   @ track bytes [4..7]
     *     str r0, [r4, o_SoundChannel_gateTime]       @ chan  bytes [16..19]
     *
     * MusicPlayerTrack is { flags, wait, patternLevel, repN, gateTime, key,
     * velocity, runningStatus } and SoundChannel is { ..., key, envelopeVolume,
     * envelopeVolumeRight, envelopeVolumeLeft, pseudoEchoVolume,
     * pseudoEchoLength, dummy1, dummy2, gateTime, midiKey, velocity, priority,
     * rhythmPan, ... }, so the four bytes copied are
     *
     *     gateTime -> gateTime, key -> midiKey, velocity -> velocity,
     *     runningStatus -> priority (overwritten two instructions later).
     *
     * `velocity` is what ChnVolSetAsm scales the channel volume by, and nothing
     * else in the driver ever writes it -- spelling out the word store is the
     * only way it gets a value. Setting gateTime alone leaves velocity at 0 and
     * every PCM channel mixes silence. */
    chan->gateTime = track->gateTime;
    chan->midiKey = track->key;
    chan->velocity = track->velocity;
    chan->priority = priority;
    chan->key = track->key;
    chan->rhythmPan = rhythmPan;
    /* The assembler again uses wide stores over adjacent scalar fields:
     *
     *     ldr r0, [r6, o_ToneData_attack]      @ ToneData { attack, decay, sustain, release }
     *     str r0, [r4, o_SoundChannel_attack]  @ SoundChannel { attack, decay, sustain, release }
     *     ldrh r0, [r5, o_MusicPlayerTrack_pseudoEchoVolume]
     *     strh r0, [r4, o_SoundChannel_pseudoEchoVolume]  @ + pseudoEchoLength
     *
     * So all four envelope bytes arrive at once, and the echo volume AND length
     * arrive together. Copying only `attack` leaves decay/sustain/release at 0,
     * and the envelope state machine's decay stage then computes
     * `volume = (volume * decay) >> 8` = 0, finds it at or below the sustain of
     * 0, and switches the channel off -- so every note dies the vblank after
     * its attack finishes, whatever the voice's real envelope says. That is the
     * dropout: notes only sound while attack is still ramping, which at attack
     * 51 of 255 is about five vblanks. The same omission zeroes
     * pseudoEchoVolume/Length, so the release tail has nowhere to decay into
     * and stops dead instead of fading. */
    chan->type = tone->type;
    chan->wav = tone->wav;
    chan->attack = tone->attack;
    chan->decay = tone->decay;
    chan->sustain = tone->sustain;
    chan->release = tone->release;
    chan->pseudoEchoVolume = track->pseudoEchoVolume;
    chan->pseudoEchoLength = track->pseudoEchoLength;

    ChnVolSetAsm(chan, track);

    {
        s32 key2 = chan->key + track->keyM;
        u32 freq;

        if (key2 < 0)
            key2 = 0;

        if (cgbType)
        {
            struct CgbChannel *cgb = (struct CgbChannel *)chan;

            cgb->length = tone->length;
            {
                u8 sweep = tone->pan_sweep;
                if ((sweep & 0x80) || (sweep & 0x70))
                    sweep = 0x8;
                cgb->sweep = sweep;
            }
            if (cgbType == 3)
                cgb->wavePointer = (u32 *)tone->wav;

            freq = soundInfo->MidiKeyToCgbFreq(cgbType, key2, track->pitM);
            cgb->frequency = freq;
        }
        else
        {
            u32 count = track->unk_3C;
            chan->count = count;
            freq = MidiKeyToFreq(chan->wav, key2, track->pitM);
            chan->frequency = freq;
        }
    }

    chan->statusFlags = SOUND_CHANNEL_SF_START;
    track->flags &= 0xF0;
}

/* ------------------------------------------------------------------------
 * ply_* command handlers (gMPlayJumpTable[0..29])
 * ------------------------------------------------------------------------ */

/* Reads one byte past the command pointer, with the BIOS-ROM pointer guard the
 * assembler applies (`chk_adr_r2`). The guard rejects pointers that land in
 * ROM below the jump table, which cannot happen on the host; keeping the shape
 * documents why the assembler had it. */
static u8 ld_r3_tp_adr_i(struct MusicPlayerTrack *track)
{
    return *track->cmdPtr++;
}

void ply_fine(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan = track->chan;

    (void)mplayInfo;

    while (chan)
    {
        struct SoundChannel *next = chan->nextChannelPointer;

        if (chan->statusFlags & SOUND_CHANNEL_SF_ON)
            chan->statusFlags |= SOUND_CHANNEL_SF_STOP;
        RealClearChain(chan);
        chan = next;
    }

    track->flags = 0;
}

/* ply_goto / ply_patt / ply_rept read a 4-byte target.
 *
 * Generated songs store an INDEX into gNativeSongPtrs there, because a 4-byte
 * operand cannot hold a host address (fix #23's treatment). The two Pokemon cry
 * songs are built at runtime by SetPokemonCryTone instead, and their loop
 * target is the address of their own `cont` field, which only exists once the
 * struct does -- so those two operands carry an index into
 * gNativeSongRuntimePtrs, offset by a sentinel no generated index can reach.
 * Both encodings are resolved here and nothing else has to know which is which.
 */
void ply_goto(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    const u8 *cmd = track->cmdPtr;
    u32 target = cmd[0] | (cmd[1] << 8) | (cmd[2] << 16) | ((u32)cmd[3] << 24);

    (void)mplayInfo;

    if (target >= NATIVE_SONG_RUNTIME_BASE)
        track->cmdPtr = gNativeSongRuntimePtrs[target - NATIVE_SONG_RUNTIME_BASE];
    else
        track->cmdPtr = (u8 *)gNativeSongPtrs[target];
}

void ply_patt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    if (track->patternLevel >= 3)
    {
        ply_fine(mplayInfo, track);
        return;
    }

    track->patternStack[track->patternLevel] = track->cmdPtr + 4;
    track->patternLevel++;
    ply_goto(mplayInfo, track);
}

void ply_pend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;

    if (track->patternLevel == 0)
        return;

    track->patternLevel--;
    track->cmdPtr = track->patternStack[track->patternLevel];
}

void ply_rept(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    const u8 *cmd = track->cmdPtr;

    (void)mplayInfo;

    /* A zero repeat count means "play the pattern once", which falls straight
     * through to the goto. */
    if (cmd[0] == 0)
    {
        track->cmdPtr = (u8 *)(cmd + 1);
        ply_goto(mplayInfo, track);
        return;
    }

    track->repN++;
    if (track->repN < cmd[0])
    {
        ply_goto(mplayInfo, track);
    }
    else
    {
        track->repN = 0;
        track->cmdPtr = (u8 *)(cmd + 5);
    }
}

void ply_prio(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->priority = ld_r3_tp_adr_i(track);
}

void ply_tempo(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 tempo = ld_r3_tp_adr_i(track) * 2;

    mplayInfo->tempoD = tempo;
    mplayInfo->tempoI = (tempo * mplayInfo->tempoU) >> 8;
}

void ply_keysh(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->keyShift = ld_r3_tp_adr_i(track);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_voice(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    const struct ToneData *voice = &mplayInfo->tone[*track->cmdPtr++];

    track->tone.type = voice->type;
    track->tone.wav = voice->wav;
    track->tone.attack = voice->attack;
    track->tone.decay = voice->decay;
    track->tone.sustain = voice->sustain;
    track->tone.release = voice->release;
    track->tone.length = voice->length;
    track->tone.pan_sweep = voice->pan_sweep;
#ifdef PORTABLE
    track->tone.keySplitTable = voice->keySplitTable;
#endif
}

void ply_vol(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->vol = ld_r3_tp_adr_i(track);
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_pan(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->pan = ld_r3_tp_adr_i(track) - C_V;
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_bend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->bend = ld_r3_tp_adr_i(track) - C_V;
    track->flags |= MPT_FLG_PITCHG;
}

void ply_bendr(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->bendRange = ld_r3_tp_adr_i(track);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_lfos(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->lfoSpeed = ld_r3_tp_adr_i(track);
    if (track->lfoSpeed == 0)
        ClearModM(track);
}

void ply_lfodl(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->lfoDelay = ld_r3_tp_adr_i(track);
}

void ply_mod(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->mod = ld_r3_tp_adr_i(track);
    if (track->mod == 0)
        ClearModM(track);
}

void ply_modt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 value = ld_r3_tp_adr_i(track);

    (void)mplayInfo;

    if (track->modT != value)
    {
        track->modT = value;
        track->flags |= MPT_FLG_VOLCHG | MPT_FLG_PITCHG;
    }
}

void ply_tune(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tune = ld_r3_tp_adr_i(track) - C_V;
    track->flags |= MPT_FLG_PITCHG;
}

/* ply_port writes a byte straight into a GBA sound register (NR10..NR51); the
 * host has no such register, and no song in the game uses the command, so the
 * operand is skipped to keep the command stream in step. */
void ply_port(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 reg = *track->cmdPtr++;
    u8 value = ld_r3_tp_adr_i(track);

    (void)mplayInfo;
    (void)reg;
    (void)value;
}

void ply_endtie(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan;
    u8 key;

    (void)mplayInfo;

    if (*track->cmdPtr < 0x80)
    {
        key = *track->cmdPtr++;
        track->key = key;
    }
    else
    {
        key = track->key;
    }

    chan = track->chan;
    while (chan)
    {
        if ((chan->statusFlags & (SOUND_CHANNEL_SF_START | SOUND_CHANNEL_SF_STOP))
            == SOUND_CHANNEL_SF_START)
        {
            if (chan->midiKey == key)
            {
                chan->statusFlags |= SOUND_CHANNEL_SF_STOP;
                return;
            }
        }
        chan = chan->nextChannelPointer;
    }
}

/* ply_memacc and the extended (XCMD) commands live in src/m4a.c, which already
 * implements them.
 */


/* ------------------------------------------------------------------------
 * MPlayMain: run one player's tracks for one vblank
 * ------------------------------------------------------------------------ */

/* Advances one music player by one tick. This is the assembler's MPlayMain.
 *
 * Three details are load-bearing and easy to get wrong:
 *
 *   * `status`'s low 16 bits are a per-track ALIVE MASK, not a copy of any
 *     track's flags: each track contributes a single bit (`orrs r4, r3` where
 *     r3 walks 1,2,4,...) and a zero mask means the song has ended, which parks
 *     the player in MUSICPLAYER_STATUS_PAUSE.
 *   * the LFO advance happens per track inside the tick, in the `wait != 0`
 *     branch, so it runs once per tick that the track is waiting.
 *   * the volume/pitch refresh pass needs MPT_FLG_EXIST as well as one of
 *     VOLCHG/PITCHG -- an any-bit test, not an equality.
 *
 * FadeOutBody is reached through the jump table because src/m4a.c owns its C
 * implementation.
 */
void MPlayMain(struct MusicPlayerInfo *mplayInfo)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    u32 ident;
    u8 trackCount;
    struct MusicPlayerTrack *track;
    u32 aliveMask;

    if (mplayInfo->ident != ID_NUMBER)
        return;
    mplayInfo->ident++;

    ident = mplayInfo->ident;

    /* o_MusicPlayerInfo_func / _intp alias MPlayMainNext / musicPlayerNext, so
     * this is the player-chain walk: run this player, then hand off to the next
     * one down the list MPlayOpen built. SoundMain enters at the head
     * (SoundInfo.musicPlayerHead). */
    if (mplayInfo->MPlayMainNext)
        mplayInfo->MPlayMainNext(mplayInfo->musicPlayerNext);

    FadeOutBody(mplayInfo);

    if ((s32)mplayInfo->status < 0)
    {
        mplayInfo->ident = ID_NUMBER;
        return;
    }

    /* One command tick per 150 accumulated tempo; tempoI is added once per
     * vblank, so a 150-tempo song ticks once per vblank. */
    mplayInfo->tempoC += mplayInfo->tempoI;
    while (mplayInfo->tempoC >= 150)
    {
        u32 trackBit;

        mplayInfo->tempoC -= 150;
        trackCount = mplayInfo->trackCount;
        track = mplayInfo->tracks;
        aliveMask = 0;
        trackBit = 1;

        while (trackCount > 0)
        {
            if (track->flags & MPT_FLG_EXIST)
            {
                struct SoundChannel *chan;

                aliveMask |= trackBit;

                /* Expire gate times and drop dead channels. */
                chan = track->chan;
                while (chan)
                {
                    struct SoundChannel *next = chan->nextChannelPointer;

                    if (chan->statusFlags & SOUND_CHANNEL_SF_ON)
                    {
                        if (chan->gateTime != 0)
                        {
                            chan->gateTime--;
                            if (chan->gateTime == 0)
                                chan->statusFlags |= SOUND_CHANNEL_SF_STOP;
                        }
                    }
                    else
                    {
                        ClearChain(chan);
                    }
                    chan = next;
                }

                if (track->flags & MPT_FLG_START)
                {
                    Clear64byte(track);
                    track->flags = MPT_FLG_EXIST;
                    track->bendRange = 2;
                    track->volX = 64;
                    track->lfoSpeed = 22;
                    track->tone.type = 1;
                }

                /* Run commands until the track waits. Every branch comes back
                 * to this wait test, which is where the LFO advances. */
                for (;;)
                {
                    u8 cmd;

                    if (track->wait != 0)
                    {
                        track->wait--;

                        if (track->lfoSpeed != 0 && track->mod != 0)
                        {
                            if (track->lfoDelayC != 0)
                            {
                                track->lfoDelayC--;
                            }
                            else
                            {
                                s32 speed = track->lfoSpeedC + track->lfoSpeed;
                                s32 value;

                                track->lfoSpeedC = speed;

                                /* Triangle wave over the 0..0x80 range: rising
                                 * below 0x40, falling above it. */
                                if (speed - 0x40 < 0)
                                    value = (s8)speed;
                                else
                                    value = 0x80 - speed;

                                value = (track->mod * value) >> 6;
                                if ((u8)(track->modM ^ value) != 0)
                                {
                                    track->modM = value;
                                    track->flags |= (track->modT == 0)
                                        ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
                                }
                            }
                        }
                        break;
                    }

                    cmd = track->cmdPtr[0];
                    if (cmd < 0x80)
                    {
                        /* Running status: reuse the last command byte. */
                        cmd = track->runningStatus;
                    }
                    else
                    {
                        track->cmdPtr++;
                        if (cmd >= 0xBD)
                            track->runningStatus = cmd;
                    }

                    if (cmd >= 0xCF)
                    {
                        soundInfo->plynote(cmd - 0xCF, mplayInfo, track);
                    }
                    else if (cmd > 0xB0)
                    {
                        mplayInfo->cmd = cmd - 0xB1;
                        ((void (*)(struct MusicPlayerInfo *, struct MusicPlayerTrack *))
                         soundInfo->MPlayJumpTable[cmd - 0xB1])(mplayInfo, track);
                        /* A command that cleared the track's flags ends it. */
                        if (track->flags == 0)
                            break;
                    }
                    else
                    {
                        track->wait = gClockTable[cmd - 0x80];
                    }
                }
            }

            trackCount--;
            if (trackCount == 0)
                break;
            track++;
            trackBit <<= 1;
        }

        mplayInfo->clock++;

        if (aliveMask == 0)
        {
            mplayInfo->status = MUSICPLAYER_STATUS_PAUSE;
            break;
        }

        mplayInfo->status = aliveMask;
    }

    /* Refresh pass: tracks that asked for volume and/or pitch recomputation get
     * their channels updated, then the request flags are cleared. */
    trackCount = mplayInfo->trackCount;
    track = mplayInfo->tracks;
    while (trackCount > 0)
    {
        u8 flags = track->flags;

        if ((flags & MPT_FLG_EXIST)
            && (flags & (MPT_FLG_VOLCHG | MPT_FLG_PITCHG)))
        {
            struct SoundChannel *chan;

            TrkVolPitSet(mplayInfo, track);

            chan = track->chan;
            while (chan)
            {
                struct SoundChannel *next = chan->nextChannelPointer;

                if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON))
                {
                    ClearChain(chan);
                }
                else
                {
                    u8 cgbType = chan->type & TONEDATA_TYPE_CGB;

                    if (flags & MPT_FLG_VOLCHG)
                    {
                        ChnVolSetAsm(chan, track);
                        if (cgbType)
                            ((struct CgbChannel *)chan)->modify |= CGB_CHANNEL_MO_VOL;
                    }

                    if (flags & MPT_FLG_PITCHG)
                    {
                        s32 key = chan->key + track->keyM;

                        if (key < 0)
                            key = 0;

                        if (cgbType)
                        {
                            ((struct CgbChannel *)chan)->frequency =
                                soundInfo->MidiKeyToCgbFreq(cgbType, key, track->pitM);
                            ((struct CgbChannel *)chan)->modify |= CGB_CHANNEL_MO_PIT;
                        }
                        else
                        {
                            chan->frequency = MidiKeyToFreq(chan->wav, key, track->pitM);
                        }
                    }
                }
                chan = next;
            }

            track->flags &= 0xF0;
        }

        trackCount--;
        track++;
    }

    mplayInfo->ident = ident - 1;
}

/* ------------------------------------------------------------------------
 * The mixer
 * ------------------------------------------------------------------------ */

/* Decoded-DPCM scratch. The assembler keeps this in bss and keys it by block
 * index (SoundChannel.xpi), so a run of samples inside one block decodes once.
 * 64 samples per block, as the format below describes. */
static s8 sDpcmBuffer[64];

/* Decodes the sample at `offset` of a DPCM-compressed wave.
 * SoundMainRAM_Unk2.
 *
 * A compressed wave is a sequence of 33-byte blocks that each hold 64 samples:
 * one raw sample followed by 32 bytes of 4-bit deltas (low nibble first), each
 * indexing gDeltaEncodingTable and accumulating onto the previous value. This
 * is `wav2agb -c`, which is how every cry in the game is stored.
 */
static s8 DecodeDpcm(struct SoundChannel *chan, s32 offset)
{
    const struct WaveData *wav = chan->wav;
    u32 block = (u32)offset >> 6;

    if (block != chan->xpi)
    {
        const u8 *src = (const u8 *)wav->data + block * 33;
        s32 value = (s8)*src++;
        s32 i;

        chan->xpi = block;
        sDpcmBuffer[0] = value;
        for (i = 1; i < 64; i += 2)
        {
            u8 byte = *src++;

            value += gDeltaEncodingTable[byte & 0xF];
            sDpcmBuffer[i] = value;
            value += gDeltaEncodingTable[byte >> 4];
            sDpcmBuffer[i + 1] = value;
        }
    }

    return sDpcmBuffer[offset & 0x3F];
}

/* Fetches the interpolated sample for one output frame and advances the
 * channel's read position, wrapping at the loop point (or stopping the channel
 * when it runs off the end of a non-looping wave). Returns the sample, or a
 * flag that the channel finished.
 *
 * `fw` is the 23-bit fractional position between `pos` and `pos + 1`; the step
 * is the channel frequency scaled by divFreq, exactly as the assembler's
 * `mul r8, r12, r1` computes. TONEDATA_TYPE_FIX voices play at a fixed rate
 * (step 0x800000, i.e. one sample per output frame) and skip interpolation,
 * which is what makes `voice_directsound_no_resample` sound the way it does.
 */
static bool8 FetchSample(struct SoundChannel *chan, s32 *pos, u32 *fw,
                         s32 *sampleOut)
{
    const struct WaveData *wav = chan->wav;
    u32 step = (chan->type & TONEDATA_TYPE_FIX)
             ? 0x800000
             : (u32)((u64)chan->frequency * gSoundInfo.divFreq);
    s32 sample;
    s32 next;
    bool8 interpolate = !(chan->type & TONEDATA_TYPE_FIX);

    if (chan->type & TONEDATA_TYPE_CMP)
    {
        /* DPCM: `pos` indexes decoded samples. */
        sample = DecodeDpcm(chan, *pos);
        next = (chan->type & TONEDATA_TYPE_REV)
             ? DecodeDpcm(chan, *pos - 1)
             : DecodeDpcm(chan, *pos + 1);
    }
    else
    {
        sample = wav->data[*pos];
        next = wav->data[*pos + 1];
    }

    *sampleOut = interpolate ? sample + (((next - sample) * (s32)*fw) >> 23)
                             : sample;

    /* Advance. The assembler subtracts the whole-sample part in one go and
     * keeps the fraction, so a step larger than one sample skips samples. */
    *fw += step;
    {
        s32 advance = (s32)*fw >> 23;

        *fw &= 0x7FFFFF;
        *pos += advance;
        chan->count -= advance;

        if ((s32)chan->count <= 0)
        {
            if (chan->statusFlags & SOUND_CHANNEL_SF_LOOP)
            {
                chan->count = wav->size - wav->loopStart;
                *pos = wav->loopStart;
            }
            else
            {
                chan->statusFlags = 0;
                return FALSE;
            }
        }
    }

    return TRUE;
}

/* Mixes one channel for one vblank: `count` output frames into both halves of
 * the PCM buffer. This is the per-channel body of SoundMainRAM.
 *
 * The contribution arithmetic matches the assembler's byte-wise accumulate:
 * each output sample adds `(sample * envelopeVolume) >> 8` to the buffer byte,
 * with the result truncated to 8 bits. The GBA's masking (bic/ror) exists only
 * to stop carries crossing byte boundaries inside a packed word; stating the
 * operation per sample is equivalent and readable.
 */
static void MixChannel(struct SoundChannel *chan, s32 count)
{
    /* FIFO A (the start of pcmBuffer) is the RIGHT output and FIFO B
     * (pcmBuffer + PCM_DMA_BUF_SIZE) the LEFT: m4aSoundInit programs
     * SOUND_A_RIGHT_OUTPUT and SOUND_B_LEFT_OUTPUT, and the assembler's mixer
     * writes envelopeVolumeRight into the first half. */
    s8 *mixR = gSoundInfo.pcmBuffer;
    s8 *mixL = gSoundInfo.pcmBuffer + PCM_DMA_BUF_SIZE;
    s32 pos = (s32)(chan->currentPointer - chan->wav->data);
    u32 fw = chan->fw;
    s32 i;

    for (i = 0; i < count; i++)
    {
        s32 sample;

        if (!FetchSample(chan, &pos, &fw, &sample))
            break;
        
        mixR[i] = (s8)(mixR[i] + ((sample * chan->envelopeVolumeRight) >> 8));
        mixL[i] = (s8)(mixL[i] + ((sample * chan->envelopeVolumeLeft) >> 8));
    }

    chan->fw = fw;
    chan->currentPointer = (s8 *)(chan->wav->data + pos);
}

/* Advances one PCM channel's envelope by one vblank. This is the envelope
 * state machine at the top of the assembler's channel loop. */
static void TickChannelEnvelope(struct SoundChannel *chan)
{
    u8 flags = chan->statusFlags;
    u8 volume = chan->envelopeVolume;

    if (flags & SOUND_CHANNEL_SF_START)
    {
        if (flags & SOUND_CHANNEL_SF_STOP)
        {
            chan->statusFlags = 0;
            return;
        }

        /* First vblank of a note. `count` is not the remaining length yet: ply_note
         * left the track's start offset there, so the read pointer is
         * `data + count` and the real remaining length is `size - count`. This
         * is also where the loop flag is latched from the wave's own flags.
         * Skipping it (as an earlier version did) leaves count at 0 and the
         * mixer reads a wild sample index. */
        chan->statusFlags = SOUND_CHANNEL_SF_ENV_ATTACK;
        chan->currentPointer = chan->wav->data + chan->count;
        chan->count = chan->wav->size - chan->count;

        chan->envelopeVolume = 0;
        chan->fw = 0;

        /* The loop flag is the high byte of WaveData.status (the assembler
         * reads `ldrb r2, [r3, o_WaveData_flags]`, and `flags` is that byte).
         * WaveData has no separate field for it on the host either. */
        if ((chan->wav->status >> 8) & WAVE_DATA_FLAG_LOOP)
            chan->statusFlags |= SOUND_CHANNEL_SF_LOOP;

        /* Attack from volume 0. */
        volume = 0;
        {
            s32 attack = volume + chan->attack;

            if (attack >= 0xFF)
            {
                attack = 0xFF;
                chan->statusFlags--;
            }
            volume = (u8)attack;
        }
    }
    else if (flags & SOUND_CHANNEL_SF_IEC)
    {
        /* Pseudo-echo tail, counted down in pseudoEchoLength frames. */
        if (--chan->pseudoEchoLength <= 0)
        {
            chan->statusFlags = 0;
            return;
        }
    }
    else if (flags & SOUND_CHANNEL_SF_STOP)
    {
        /* Release: the release byte is an exponential-ish decay factor. */
        volume = (volume * chan->release) >> 8;
        if (volume <= chan->pseudoEchoVolume)
        {
            volume = chan->pseudoEchoVolume;
            if (volume == 0)
            {
                chan->statusFlags = 0;
                return;
            }
            chan->statusFlags |= SOUND_CHANNEL_SF_IEC;
        }
    }
    else
    {
        u8 env = flags & SOUND_CHANNEL_SF_ENV;

        if (env == SOUND_CHANNEL_SF_ENV_DECAY)
        {
            volume = (volume * chan->decay) >> 8;
            if (volume <= chan->sustain)
            {
                volume = chan->sustain;
                if (volume == 0)
                {
                    if (chan->pseudoEchoVolume == 0)
                    {
                        chan->statusFlags = 0;
                        return;
                    }
                    chan->statusFlags |= SOUND_CHANNEL_SF_IEC;
                }
                else
                {
                    chan->statusFlags--;
                }
            }
        }
        else if (env == SOUND_CHANNEL_SF_ENV_ATTACK)
        {
            s32 attack = volume + chan->attack;

            if (attack >= 0xFF)
            {
                attack = 0xFF;
                chan->statusFlags--;
            }
            volume = (u8)attack;
        }
    }

    chan->envelopeVolume = volume;

    /* The per-side output volumes scale by the envelope and the MASTER volume.
     *
     * The assembler is easy to misread here:
     *
     *     strb r5, [r4, o_SoundChannel_envelopeVolume]
     *     ldr  r0, [sp, 0x18]                      @ the SoundInfo pointer
     *     ldrb r0, [r0, o_SoundChannel_release]    @ SoundInfo + 7
     *     adds r0, 0x1
     *     muls r0, r5
     *     lsrs r5, r0, 4
     *
     * `[sp,0x18]` was stored from the function's original r0, which is the
     * SoundInfo pointer (the same slot is read a few lines above for
     * o_SoundInfo_divFreq and o_SoundInfo_maxChans). SoundInfo is
     * { ident(0..3), pcmDmaCounter(4), reverb(5), maxChans(6), masterVolume(7) },
     * so o_SoundChannel_release -- 7 -- lands on masterVolume. Using the
     * channel's own release field instead scales the output down 16-fold and
     * the mix comes out silent. */
    {
        u32 scale = (u32)((SOUND_INFO_PTR->masterVolume + 1) * volume) >> 4;

        chan->envelopeVolumeRight = (chan->rightVolume * scale) >> 8;
        chan->envelopeVolumeLeft = (chan->leftVolume * scale) >> 8;
    }
}

/* ------------------------------------------------------------------------
 * SoundMain and SoundMainRAM
 * ------------------------------------------------------------------------ */

/* SoundMix: mixes one vblank of audio into SoundInfo.pcmBuffer.
 *
 * Named SoundMix rather than SoundMainRAM because the latter is an existing
 * symbol: include/gba/m4a_internal.h declares it as the IWRAM-resident copy of
 * the mixer (`extern char SoundMainRAM[]`), which the GBA build makes with
 * CpuCopy32 in m4aSoundInit and jumps to. There is no relocation to do on the
 * host, so the body lives here under its own name and SoundMain calls it
 * directly.
 *
 * `pcmDmaCounter` counts down the vblanks until the buffer is swapped, and
 * `pcmDmaPeriod` is how many it reloads to; the buffer is
 * `pcmSamplesPerVBlank` BYTES total, split evenly between the two halves (each
 * is a mono stream: pcmBuffer feeds FIFO A, pcmBuffer + PCM_DMA_BUF_SIZE feeds
 * FIFO B). Clearing `pcmSamplesPerVBlank` bytes starting at the half's base
 * therefore clears both halves -- getting that length wrong is not a silent
 * bug, it leaves stale samples in one channel and buzzes.
 *
 * The assembler's clear loop is:
 *
 *     mov r1, r8           @ r8 = pcmSamplesPerVBlank
 *     lsrs r1, #3          @ ... test low bit, clear whole pair if odd
 *     lsrs r1, #1          @ ... then clear 8 bytes at a time
 *
 * i.e. it clears `pcmSamplesPerVBlank` bytes from each half. The two halves are
 * PCM_DMA_BUF_SIZE apart, so an explicit memset of that many bytes per half is
 * exact.
 */
void SoundMix(void)
{
    struct SoundInfo *soundInfo = &gSoundInfo;
    s32 samplesPerVblank = soundInfo->pcmSamplesPerVBlank;
    s32 i;

    if (soundInfo->reverb != 0)
    {
        /* Reverb: add the previous frame's delayed samples back in. The
         * assembler reads `PCM_DMA_BUF_SIZE` behind the write pointer, which is
         * why the buffer is at least that big plus one frame. */
        s8 *cur = soundInfo->pcmBuffer;
        s32 n = samplesPerVblank;
        s32 reverb = soundInfo->reverb;

        while (n > 0)
        {
            s32 value = cur[PCM_DMA_BUF_SIZE] + cur[0];
            s8 filtered;

            /* `mov r0, r1, asr 9` then round-to-nearest-negative correction:
             * the assembler does an arithmetic shift and adds 1 when bit 7 of
             * the result is set. */
            value = (value * reverb) >> 9;
            if (value & 0x80)
                value++;
            filtered = (s8)value;

            cur[PCM_DMA_BUF_SIZE] = filtered;
            cur[0] = filtered;
            cur++;
            n--;
        }
    }
    else
    {
        /* BOTH halves, `samplesPerVblank` bytes each. The assembler's clear
         * loop walks r5 (pcmBuffer + the write cursor) and r6 (r5 +
         * PCM_DMA_BUF_SIZE) in lockstep with `stm r5!`/`stm r6!` pairs, so each
         * half is cleared to the same length. MixChannel below ACCUMULATES into
         * the buffer, so an uncleared half keeps the previous vblank's samples
         * and each note piles on top of them -- the channel saturates rather
         * than plays. Clearing only the first half is exactly that bug.
         *
         * This branch is the reverb-off path: with reverb set, the block above
         * already writes both halves (and mixes the previous frame in). Field
         * music sets reverb (m4aSoundMode from the song header), so this one is
         * reached before a reverb-setting song starts, and by any song whose
         * header clears it. */
        memset(soundInfo->pcmBuffer, 0, samplesPerVblank);
        memset(soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE, 0, samplesPerVblank);
    }

    /* Advance each active channel's envelope, then mix it. */
    for (i = 0; i < soundInfo->maxChans; i++)
    {
        struct SoundChannel *chan = &soundInfo->chans[i];

        if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON))
            continue;

        TickChannelEnvelope(chan);

        if (chan->statusFlags & SOUND_CHANNEL_SF_ON)
            MixChannel(chan, samplesPerVblank);
    }

    soundInfo->ident = ID_NUMBER;
}

/* SoundMain: one vblank of the whole sound system.
 *
 * Called from VBlankIntr (src/main.c). The assembler copy of this routine in
 * IWRAM (SoundMainRAM_Buffer) is what m4aSoundInit's CpuCopy32 installs; on the
 * host there is no copy to make and the routine runs directly.
 *
 * Order matters: the CGB synthesiser runs before the PCM mixer because the
 * latter clears the buffer the former has already finished with, and MPlayMain
 * runs first so this vblank's notes are queued before they are mixed.
 */
void SoundMain(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (!soundInfo || soundInfo->ident != ID_NUMBER)
        return;

    soundInfo->ident++;

    if (soundInfo->MPlayMainHead)
        soundInfo->MPlayMainHead(soundInfo->musicPlayerHead);

    soundInfo->CgbSound();

    if (soundInfo->pcmDmaCounter - 1 > 0)
    {
        /* Skip: this vblank does not reach a buffer boundary. */
    }

    SoundMix();

    /* The CGB (PSG) channels are computed into the NRxx registers by CgbSound
     * above; on hardware those registers drive the pulse/wave/noise generators
     * straight into the same DAC the two FIFOs feed. There is no such hardware
     * here, so synthesise them into the buffer the DirectSound mixer just
     * filled -- without this every CGB voice in the game's music is silent
     * (src/platform/psg.c explains why that is not a corner case). */
    Platform_PsgMix(soundInfo);
}

/* m4aSoundVSync: the per-vblank PCM handoff. On the GBA this restarts the two
 * FIFO DMAs so the hardware plays the freshly mixed half; under PORTABLE the
 * platform layer takes the buffer instead (Platform_SubmitAudioFrame), so the
 * DMA bookkeeping is replaced by nothing at all and the counter is advanced so
 * the engine's own pcmDmaCounter tracking stays consistent. */
void m4aSoundVSync(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (!soundInfo)
        return;
    if (soundInfo->ident != ID_NUMBER && soundInfo->ident != ID_NUMBER + 1)
        return;

    if (--soundInfo->pcmDmaCounter <= 0)
        soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;
}
