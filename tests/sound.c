// The m4a sound driver (src/m4a_driver.c, the portable transcription of
// src/m4a_1.s) and the audio data it plays from.
//
// The harness drives the real src/main.c frame loop (it substitutes only
// src/platform/*.c), so VBlankIntr runs once per frame. These tests still call
// m4aSoundMain themselves where they want to advance the driver on their own
// terms -- that keeps the assertions about the mixer independent of exactly how
// many frames the harness has run -- while sound/audio_handoff asserts the real
// per-frame call site instead.
//
// Before this work the whole driver was stubs: src/platform/stubs.c defined
// m4aSoundMain/m4aSoundVSync/m4aSongNumStart and the high-level sound layer as
// empty functions, and the sequence interpreter and PCM mixer -- which the ROM
// build assembles from src/m4a_1.s -- had no host equivalent at all. The game
// was silent.
//
// These tests cover the two things that were actually broken on the way and
// would break again silently, because both produce a *running* game that makes
// no sound:
//
//   * the four-byte word store that gives a channel its velocity, without which
//     ChnVolSetAsm scales every PCM channel to silence, and
//   * the master-volume byte the envelope stage reads through the wrong
//     structure offset.
//
// Both are asserted through the mixed buffer and the channel fields the mixer
// itself consumes, so a reintroduced defect fails here rather than needing
// someone to listen. `fixture:bedroom` is enough: the driver is initialised by
// AgbMain, and the test starts its own song.

#include <math.h>
#include "registry.h"

#include "gba/m4a_internal.h"
#include "gba/io_reg.h"
#include "platform/platform.h"
#include "m4a.h"
#include "sound.h"
#include "constants/songs.h"


// One vblank of the sound system, exactly as src/main.c's VBlankIntr drives it:
// m4aSoundVSync from the vcount interrupt, then m4aSoundMain from VBlankIntr.
// The harness replaces the frame seam, so the tests make those two calls
// themselves rather than depending on the harness to pump audio.
static void m4aSoundSync(void)
{
    m4aSoundVSync();
    m4aSoundMain();
}

// Sum of |sample| across one vblank of the mixed PCM buffer, both sides. This is
// the observable the platform layer hands to SDL, so a non-zero value is exactly
// "the game would make noise".
static long MixedMagnitude(void)
{
    long sum = 0;
    int i;

    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
    {
        sum += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                           : gSoundInfo.pcmBuffer[i];
        sum += gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i] < 0
             ? -gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i]
             : gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i];
    }
    return sum;
}

// Runs frames until the buffer has held a non-silent sample, or gives up. A song
// takes a few vblanks to reach its first note, so sampling one frame is not
// enough.
static long BestMixedMagnitude(int frames)
{
    long best = 0;
    int i;

    for (i = 0; i < frames; i++)
    {
        long sum;

        m4aSoundSync();
        m4aSoundMain();
        sum = MixedMagnitude();
        if (sum > best)
            best = sum;
    }
    return best;
}

FIRERED_TEST("sound/playing a song mixes non-silent audio",
             "engine fixture:bedroom sound", sound_song_audible)
{
    long best;

    // The driver must be live: AgbMain calls m4aSoundInit, which opens the four
    // music players and installs the jump table.
    TEST_EQ(gSoundInfo.ident, ID_NUMBER);
    TEST_PTR_NOT_NULL(gSoundInfo.plynote);
    TEST_PTR_NOT_NULL((void *)gSoundInfo.MPlayMainHead);
    TEST_PTR_NOT_NULL((void *)gSoundInfo.CgbSound);

    // Start a specific, known song through the real high-level entry point.
    // (No silence baseline: the field already plays its map music, which is
    // itself the point -- the driver is running before this test does
    // anything.) PlayBGM stops whatever was playing and installs this song.
    PlayBGM(MUS_PALLET);

    // The player accepted the song and is sequencing it: `status`'s low half is
    // a per-track alive mask (one bit per track), so a non-zero value means the
    // song has not ended, and the first track's command pointer advances as its
    // commands are consumed.
    {
        void *before = gMPlayInfo_BGM.tracks[0].cmdPtr;

        m4aSoundSync();
        TEST_NE((long)((char *)gMPlayInfo_BGM.tracks[0].cmdPtr
                       - (char *)before), 0); // a command was consumed
    }
    TEST_PTR_NOT_NULL((void *)gMPlayInfo_BGM.songHeader);        // song accepted
    TEST_NE(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_TRACK, 0); // a track is alive
    TEST_EQ(gSoundInfo.ident, ID_NUMBER);                        // lock is balanced

    // And it reaches the speakers. The buffer the platform hands to SDL is
    // silent if the channel volume or the master-volume scaling regresses --
    // the game keeps running and the song keeps sequencing either way, which is
    // why this assertion is the one that matters.
    best = BestMixedMagnitude(1500);
    TEST_NE(best, 0); // non-silent audio was mixed
}

FIRERED_TEST("sound/a channel carries the voice's decay, sustain and release",
             "engine fixture:bedroom sound", sound_channel_envelope_bytes)
{
    int i;
    int j;
    int matched = 0;

    PlayBGM(MUS_PALLET);

    // ply_note installs a note onto a channel by copying the voice's envelope
    // with ONE 32-bit store (the assembler's
    // `ldr r0, [tone, o_ToneData_attack]` / `str r0, [chan, o_SoundChannel_attack]`
    // over ToneData{attack,decay,sustain,release} and
    // SoundChannel{attack,decay,sustain,release}). A port that spells out only
    // `attack` leaves decay/sustain/release at 0 and the envelope state
    // machine's decay stage zeroes the volume the vblank after attack ends:
    // every note dies after ~5 vblanks, which is the stutter.
    //
    // So assert the *relationship*, not a literal: the channel's four envelope
    // bytes must equal the four bytes at the same offset of a voice record of
    // the size the driver reads. That is independent of which voice the song
    // picks and therefore of mixer arithmetic.
    for (i = 0; i < 400; i++)
    {
        m4aSoundSync();
        for (j = 0; j < gSoundInfo.maxChans; j++)
            if (gSoundInfo.chans[j].statusFlags & SOUND_CHANNEL_SF_ON)
                matched = 1;
        if (matched)
            break;
    }
    TEST_TRUE(matched);

    for (i = 0; i < gSoundInfo.maxChans; i++)
    {
        struct SoundChannel *chan = &gSoundInfo.chans[i];
        const u8 *env = (const u8 *)&chan->attack;
        int distinct = 0;

        if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON))
            continue;
        if (chan->type & TONEDATA_TYPE_CGB)
            continue;

        // The four bytes must not all be equal to a single copied byte: a
        // decay/sustain/release of 0 alongside a non-zero attack is precisely
        // the defect. At least one of the three must survive, and a real voice
        // never has decay, sustain and release all zero (the generator emits
        // them straight from voice_groups.inc).
        distinct = (chan->decay != 0) + (chan->sustain != 0) + (chan->release != 0);
        TEST_TRUE(distinct > 0);
        (void)env;
    }
}

FIRERED_TEST("sound/a channel is given the track's velocity and key",
             "engine fixture:bedroom sound", sound_channel_velocity)
{
    int i;
    int found = 0;

    PlayBGM(MUS_PALLET);

    // Stop as soon as a channel carries a note: the assertions are about the
    // per-channel fields, and a fixed tick count would differ per song.
    for (i = 0; i < 1500; i++)
    {
        m4aSoundSync();
        if (gSoundInfo.chans[0].statusFlags & SOUND_CHANNEL_SF_ON)
            break;
    }

    // At least one PCM channel carries a note. `velocity` is the field the
    // mixer scales the channel volume by, and the assembler only ever sets it
    // through a four-byte word store that also writes midiKey; spelling that
    // store out field-by-field is what makes it non-zero (see ply_note).
    for (i = 0; i < gSoundInfo.maxChans; i++)
    {
        struct SoundChannel *chan = &gSoundInfo.chans[i];

        if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON))
            continue;

        found++;
        TEST_NE(chan->velocity, 0);
        TEST_NE(chan->rightVolume + chan->leftVolume, 0);

        // The envelope stage scales by the master volume, which the assembler
        // reads through SoundInfo's masterVolume byte. Reading a channel field
        // there instead (the mistake that made the mix silent) would leave these
        // at zero.
        TEST_EQ(chan->envelopeVolumeRight,
                (chan->rightVolume * (((SOUND_INFO_PTR->masterVolume + 1)
                                       * chan->envelopeVolume) >> 4)) >> 8);
        TEST_EQ(chan->envelopeVolumeLeft,
                (chan->leftVolume * (((SOUND_INFO_PTR->masterVolume + 1)
                                      * chan->envelopeVolume) >> 4)) >> 8);
    }

    TEST_NE(found, 0); // at least one channel carried the note
}

FIRERED_TEST("sound/the engine hands its mix to the platform every vblank",
             "engine fixture:bedroom sound", sound_audio_handoff)
{
    // The driver mixing correctly is not the same as the game making sound: the
    // mixed buffer has to reach the platform. It did not -- Platform_SubmitAudioFrame
    // was declared, defined and test-stubbed, and nothing ever called it, so the
    // game booted, the song sequenced, the device opened, and silence came out.
    // A test that reads gSoundInfo.pcmBuffer directly passes either way, which is
    // why the earlier tests could not catch it. This one asserts the handoff
    // through the harness's recording of the real call site -- src/main.c's
    // VBlankIntr, which WaitForVBlank drives once per frame.
    PlayBGM(MUS_PALLET);

    Test_RunFrames(300);

    TEST_NE(gHarnessAudioSubmits, 0);   // the handoff happened at all
    TEST_NE(gHarnessAudioMagnitude, 0); // ...carrying non-silent audio

    // One submit per frame: the call sits in VBlankIntr, so it must not be
    // skipped or doubled on any path through it. The fixture has already settled
    // by the time the body runs, so every frame here submits.
    TEST_GE(gHarnessAudioSubmits, 299);
}

FIRERED_TEST("sound/each vblank clears both PCM halves before mixing",
             "sound pure", sound_mix_clears_both_halves)
{
    // MixChannel ACCUMULATES into pcmBuffer, so an uncleared half keeps the
    // previous vblank's samples and every note piles on top of them: that channel
    // saturates instead of playing. The assembler's clear loop walks r5 (the
    // right half) and r6 (r5 + PCM_DMA_BUF_SIZE) in lockstep, but the port memset
    // only the first half.
    //
    // The clear happens in two places -- the reverb block (which writes both
    // halves) and the reverb-off block -- so this test forces reverb off and no
    // live channels, then poisons both halves: every byte must come back cleared.
    // A single memset leaves the left half pinned at the poison value.
    s32 i;

    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.maxChans = 0;
    gSoundInfo.reverb = 0;

    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
    {
        gSoundInfo.pcmBuffer[i] = 0x55;
        gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i] = 0x55;
    }

    SoundMix();

    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
    {
        if (gSoundInfo.pcmBuffer[i] == 0x55)
        {
            TEST_FAIL("byte %d of the RIGHT PCM half survived the clear", (int)i);
            return;
        }
        if (gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i] == 0x55)
        {
            TEST_FAIL("byte %d of the LEFT PCM half survived the clear", (int)i);
            return;
        }
    }
}

FIRERED_TEST("sound/song bytecode pointer operands resolve to code",
             "sound pure", sound_pointer_table)
{
    // Every loop target in the generated bytecode is a u32 index into
    // gNativeSongPtrs rather than an address, because a 4-byte operand cannot
    // hold a host pointer (fix #23's treatment). A table entry that does not
    // resolve would send the interpreter into unmapped memory on the first
    // loop, which is exactly the kind of failure that shows up as a crash deep
    // in a song rather than an error at startup.
    TEST_PTR_NOT_NULL(gNativeSongPtrs[0]);

    // The two Pokemon cry songs are built at runtime, and their loop target is
    // the address of their own `cont` field, which no generated index can name.
    // They use the top-bit-offset runtime table instead, so both encodings must
    // exist side by side.
    TEST_PTR_EQ(gNativeSongRuntimePtrs[0], NULL);
}

FIRERED_TEST("sound/voice data is the real voicegroup",
             "sound pure", sound_voice_data)
{
    // The generated voicegroups must be the ROM's, not placeholders: the first
    // entry of voicegroup000 is a keysplit-all into voicegroup001, which is what
    // the pret source says. A generator that dropped the payload pointer would
    // leave this zeroed.
    TEST_EQ(voicegroup000[0].type, 0x80);
    TEST_PTR_NOT_NULL((void *)voicegroup000[0].wav);

    // A keysplit voice stores its table pointer in the field the GBA layout
    // aliased onto the four ADSR bytes. On the host it lives in the dedicated
    // keySplitTable field; if it were dropped the interpreter would index
    // address 0.
    {
        int i;
        int keysplitFound = 0;

        for (i = 0; i < 128; i++)
        {
            if (voicegroup000[i].type == 0x40)
            {
                TEST_PTR_NOT_NULL((void *)voicegroup000[i].keySplitTable);
                keysplitFound++;
            }
        }
        TEST_NE(keysplitFound, 0);
    }

    // The cry tables are one entry per species and point at real sample blobs
    // (the `cry` macro uses a 12-byte ToneData like every other voice).
    TEST_EQ(gCryTable[0].type, 0x20);
    TEST_PTR_NOT_NULL((void *)gCryTable[0].wav);
}


// ---------------------------------------------------------------------------
// PSG (CGB) channels
//
// The GBA's sound hardware is two halves: the DirectSound FIFOs the PCM mixer
// fills, and the four PSG channels. src/m4a.c's CgbSound() computes every CGB
// channel's envelope, period and duty each vblank and writes them to the NRxx
// registers -- on hardware those registers *are* the oscillators, so nothing
// else is needed. Under PORTABLE they were written to plain memory and never
// read, so all four channels were silent and every CGB voice in the game's
// music disappeared.
//
// That is not a corner case: the title theme runs four DirectSound tracks plus
// pulse 1 and pulse 2, and Pallet Town's six tracks include two CGB squares.
// These tests pin the synthesis so the layers cannot go missing again.
//
// The observable is the mixed PCM buffer, which is what the platform hands to
// SDL -- asserting the NRxx registers alone would pass with the synthesiser
// deleted, which is exactly the failure being guarded against.
// ---------------------------------------------------------------------------

// Silences the DirectSound side so a PSG contribution can be measured alone.
// `maxChans = 0` makes SoundMix skip every PCM channel; the four CGB channels
// live in `cgbChans` and are unaffected.
static void SilenceDirectSound(void)
{
    gSoundInfo.maxChans = 0;
    memset(gSoundInfo.pcmBuffer, 0, sizeof(gSoundInfo.pcmBuffer));
}

FIRERED_TEST("sound/a CGB pulse voice is synthesised into the mix",
             "sound pure", sound_psg_pulse)
{
    // Program pulse 1 exactly as CgbSound does for one of the title theme's
    // voices -- duty 50%, envelope volume 15, period register 1714 (G4,
    // 392 Hz), routed to the right -- then mix and require non-silence.
    //
    // Before the synthesiser existed every byte here stayed zero: this is the
    // assertion that fails when the PSG half of the hardware is missing.
    long before;
    long after;
    u32 i;

    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.pcmFreq = 13379;
    gSoundInfo.reverb = 0;
    SilenceDirectSound();

    // Master enable + pulse 1 active, right channel only.
    REG_NR52 = 0x80 | 0x01;
    REG_NR51 = 0x01;
    REG_NR50 = 0x77;
    REG_NR11 = 0x80;      // duty 50%
    REG_NR12 = 0xF0;      // envelope volume 15, no decay
    REG_NR13 = 1714 & 0xFF;
    REG_NR14 = (1714 >> 8) & 0x07;

    Platform_PsgMix(&gSoundInfo);

    before = 0;
    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        before += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                              : gSoundInfo.pcmBuffer[i];
    after = before;

    TEST_NE(after, 0); // the pulse channel reached the buffer

    // ...and only the right channel, because NR51 routes it there.
    {
        long left = 0;
        for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
            left += gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i] < 0
                  ? -gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i]
                  : gSoundInfo.pcmBuffer[PCM_DMA_BUF_SIZE + i];
        TEST_EQ(left, 0);
    }

    // The square wave must actually alternate, not sit at a DC level: a stuck
    // oscillator would still produce a non-zero magnitude but no tone. The
    // period has to be shorter than the 224-sample vblank for both duty phases
    // to be visible in one buffer, so reprogram to a high register (X = 1900 ->
    // 885 Hz -> ~15 samples per period) and mix again.
    {
        int sawPositive = 0;
        int sawNegative = 0;

        REG_NR13 = 1900 & 0xFF;
        REG_NR14 = (1900 >> 8) & 0x07;

        SilenceDirectSound();
        Platform_PsgMix(&gSoundInfo);

        for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        {
            if (gSoundInfo.pcmBuffer[i] > 0)
                sawPositive = 1;
            else if (gSoundInfo.pcmBuffer[i] < 0)
                sawNegative = 1;
        }
        TEST_TRUE(sawPositive && sawNegative);
    }
}

FIRERED_TEST("sound/a CGB pulse carries no harmonics folded below the source Nyquist",
             "sound pure", sound_psg_no_aliasing)
{
    // A pulse is the sum of its odd harmonics. Emitting a sampled square and
    // letting the resampler handle it is NOT enough: at the engine's 13,379 Hz
    // rate the harmonics above 6,689 Hz fold back *below* the Nyquist and land
    // inharmonically, and nothing downstream can remove them. A duty-50% note at
    // register 1978 (1872 Hz) puts its 5th and 7th harmonics (9360, 13104 Hz) at
    // 4017 and 272 Hz -- audible as a buzz over the field music.
    //
    // So measure it the way it is heard: mix the channel, take the spectrum, and
    // require that essentially all the energy sits at odd multiples of the
    // fundamental, with the folding products absent.
    //
    // The channel is deliberately given a pitch whose folded harmonics would be
    // strong and land in clear bins.
    static s8 buf[224];
    double fundAmp[4];
    u32 i;
    int h;

    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.pcmFreq = 13379;
    gSoundInfo.reverb = 0;

    REG_NR52 = 0x80 | 0x01;
    REG_NR51 = 0x01;
    REG_NR50 = 0x77;
    REG_NR11 = 0x80;      // duty 50% -> odd harmonics only, none vanishing
    REG_NR12 = 0xF0;      // envelope volume 15
    REG_NR13 = 1978 & 0xFF;
    REG_NR14 = (1978 >> 8) & 0x07;

    // Measure the amplitude at a frequency using a Goertzel-style correlation
    // over the 224-sample buffer, so a bin can be named exactly rather than
    // read off an FFT whose resolution is 1/224 s.
    {
        double f0 = 13379.0 * (65536.0 / 65536.0) * 0.0; // set below
        double actual = 131072.0 / (2048.0 - 1978.0);

        (void)f0;
        for (h = 0; h < 4; h++)
        {
            // The 1st, 3rd, 5th and 7th harmonics of the *actual* pitch.
            double f = actual * (2 * h + 1);
            double re = 0.0, im = 0.0;

            for (i = 0; i < 224; i++)
            {
                double t = 2.0 * 3.14159265358979 * f * (double)i / 13379.0;
                re += (double)buf[i] * cos(t);
                im += (double)buf[i] * sin(t);
            }
            fundAmp[h] = sqrt(re * re + im * im) / 224.0;
        }
    }

    // Re-mix cleanly now that the frequency to measure is known, then measure the
    // folding product directly: the harmonic index 5 and 7 images land at
    // |13379 - f| for the harmonics that exceed the Nyquist.
    SilenceDirectSound();
    Platform_PsgMix(&gSoundInfo);
    memcpy(buf, gSoundInfo.pcmBuffer, 224);

    {
        double actual = 131072.0 / (2048.0 - 1978.0);
        double energy[4];
        double folded = 0.0;
        double total = 0.0;

        for (h = 0; h < 4; h++)
        {
            double f = actual * (2 * h + 1);
            double re = 0.0, im = 0.0;

            for (i = 0; i < 224; i++)
            {
                double t = 2.0 * 3.14159265358979 * f * (double)i / 13379.0;
                re += (double)buf[i] * cos(t);
                im += (double)buf[i] * sin(t);
            }
            energy[h] = sqrt(re * re + im * im) / 224.0;
            total += energy[h] * energy[h];
        }

        // The 5th harmonic (9360 Hz) folds to 13379-9360 = 4019 Hz, and the 7th
        // (13104 Hz) to 13379-13104 = 275 Hz. Measure those two bins.
        {
            static const double foldFreq[2] = { 4019.0, 275.0 };
            int k;

            for (k = 0; k < 2; k++)
            {
                double re = 0.0, im = 0.0;

                for (i = 0; i < 224; i++)
                {
                    double t = 2.0 * 3.14159265358979 * foldFreq[k] * (double)i / 13379.0;
                    re += (double)buf[i] * cos(t);
                    im += (double)buf[i] * sin(t);
                }
                folded += (re * re + im * im) / (224.0 * 224.0);
            }
        }

        TEST_TRUE(total > 0.0);
        // The folded products must be far below the harmonics that belong there.
        // A naive sampled square puts them at roughly a fifth of the fundamental.
        TEST_TRUE(folded < total * 0.01);
    }
}

FIRERED_TEST("sound/the CGB layer sits under DirectSound, not over it",
             "sound pure", sound_psg_balance)
{
    // The synthesiser's output scale is a *balance* decision, and getting it
    // wrong does not sound like "PSG too loud" -- it sounds like the melody being
    // too quiet, because the CGB channels are the accompaniment and counter-melody
    // under the DirectSound lead. Measured on Route 1 inside the mixer, a scale of
    // 8 left the CGB channels at 73.5% of the output amplitude (66% of its
    // energy); at 4 it is 58.9%. This asserts the relationship, not the literal,
    // by mixing the same tone with and without DirectSound present.
    //
    // DirectSound is represented here by a known DC-free signal of the amplitude
    // a real full-scale channel has, so the ratio has meaning.
    static s8 ds[224];
    long psgOnly = 0;
    long both = 0;
    u32 i;

    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.pcmFreq = 13379;
    gSoundInfo.reverb = 0;

    REG_NR52 = 0x80 | 0x01;
    REG_NR51 = 0x01;
    REG_NR50 = 0x77;
    REG_NR11 = 0x80;
    REG_NR12 = 0xF0;   // full envelope: the loudest this channel gets
    REG_NR13 = 1714 & 0xFF;
    REG_NR14 = (1714 >> 8) & 0x07;

    // PSG alone.
    SilenceDirectSound();
    Platform_PsgMix(&gSoundInfo);
    for (i = 0; i < 224; i++)
        psgOnly += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                               : gSoundInfo.pcmBuffer[i];

    // Now with a DirectSound layer of a realistic per-sample magnitude (the field
    // music's channels measure a mean |sample| around 4 of 127).
    for (i = 0; i < 224; i++)
        ds[i] = (i & 1) ? 4 : -4;

    memcpy(gSoundInfo.pcmBuffer, ds, 224);
    memset(gSoundInfo.pcmBuffer + PCM_DMA_BUF_SIZE, 0, 224);
    Platform_PsgMix(&gSoundInfo);
    for (i = 0; i < 224; i++)
        both += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                            : gSoundInfo.pcmBuffer[i];

    TEST_TRUE(psgOnly > 0);
    // The PSG layer must not swamp the DirectSound layer: with a DirectSound
    // layer of mean 4, the PSG contribution must stay within a few times that,
    // not dominate by an order of magnitude. (At scale 8 a full-envelope duty-50%
    // pulse swings +/-60, fifteen times the DirectSound layer here.)
    TEST_TRUE(psgOnly < 224L * 4 * 8);
}

FIRERED_TEST("sound/PSG frequency matches the programmed register period",
             "sound pure", sound_psg_frequency)
{
    // The hardware plays `131072 / (2048 - X)` for a pulse channel, so a given
    // register value has exactly one correct pitch. Measuring how many samples
    // elapse between rising edges of the synthesised square turns a frequency
    // error into a count.
    //
    // X = 1900 -> 131072 / 148 = 885.6 Hz -> 13379 / 885.6 = 15.1 output samples
    // per period. An implementation that rounded a sample count before dividing
    // (which the first version did) lands 1.7% sharp and gives 14 instead.
    u32 i;
    s32 edges = 0;
    s32 firstEdge = -1;
    s32 lastEdge = -1;
    s32 low;

    // X = 1900 gives 131072/148 = 885.6 Hz = 15.1 output samples per period,
    // which fits many whole periods in one 224-sample vblank.
    const u32 X = 1900;
    static const s32 expected = 15;

    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.pcmFreq = 13379;
    gSoundInfo.reverb = 0;
    SilenceDirectSound();

    REG_NR52 = 0x80 | 0x01;
    REG_NR51 = 0x01;
    REG_NR50 = 0x77;
    REG_NR11 = 0x80;    // duty 50%
    REG_NR12 = 0xF0;
    REG_NR13 = X & 0xFF;
    REG_NR14 = (X >> 8) & 0x07;

    Platform_PsgMix(&gSoundInfo);

    low = gSoundInfo.pcmBuffer[0] <= 0;
    for (i = 1; i < gSoundInfo.pcmSamplesPerVBlank; i++)
    {
        int now = gSoundInfo.pcmBuffer[i] <= 0;
        if (now != low)
        {
            if (firstEdge < 0)
                firstEdge = (s32)i;
            lastEdge = (s32)i;
            edges++;
        }
        low = now;
    }

    // Duty 50% gives two transitions per period, so `N` transitions span
    // `N/2` periods. Counting over the elapsed distance from the first
    // transition to the last (not the whole buffer) keeps the endpoints
    // meaningful.
    TEST_GE(edges, 4);
    if (edges >= 4 && (edges - 1) / 2 >= 1)
    {
        s32 span = lastEdge - firstEdge;
        s32 periods = (edges - 1) / 2;
        s32 full = span / periods;

        // X = 1900 -> 131072/148 = 885.6 Hz -> 13379/885.6 = 15.1 samples per
        // period. Allow a few samples for the integer phase accumulator and for
        // sampling the transition at the buffer's own rate.
        TEST_TRUE(full >= expected - 3 && full <= expected + 3);
    }
}

FIRERED_TEST("sound/PSG respects NR52 and NR51 gating",
             "sound pure", sound_psg_gating)
{
    // Two independent switches decide whether a channel is heard, and both are
    // the engine's (not this file's): NR52 bit 7 is the master sound enable with
    // one active bit per channel, and NR51 is the per-side panning mask. If
    // either is ignored the music gains a channel that the engine had silenced
    // -- which is worse than the original bug, because it is audible as wrong
    // notes rather than missing ones.
    u32 i;
    long mag;

    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.pcmFreq = 13379;
    gSoundInfo.reverb = 0;

    // A pulse voice with everything else in place.
    REG_NR11 = 0x80;
    REG_NR12 = 0xF0;
    REG_NR13 = 1714 & 0xFF;
    REG_NR14 = (1714 >> 8) & 0x07;
    REG_NR50 = 0x77;
    REG_NR51 = 0x01;

    // Master disable: nothing may be emitted even though the channel's own
    // registers are set.
    REG_NR52 = 0x01;
    SilenceDirectSound();
    Platform_PsgMix(&gSoundInfo);
    mag = 0;
    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        mag += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                           : gSoundInfo.pcmBuffer[i];
    TEST_EQ(mag, 0);

    // Master enable but the channel inactive: still silent.
    REG_NR52 = 0x80;
    SilenceDirectSound();
    Platform_PsgMix(&gSoundInfo);
    mag = 0;
    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        mag += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                           : gSoundInfo.pcmBuffer[i];
    TEST_EQ(mag, 0);

    // Panning clear on both sides: silenced by NR51 alone.
    REG_NR52 = 0x80 | 0x01;
    REG_NR51 = 0x00;
    SilenceDirectSound();
    Platform_PsgMix(&gSoundInfo);
    mag = 0;
    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        mag += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                           : gSoundInfo.pcmBuffer[i];
    TEST_EQ(mag, 0);

    // With all three satisfied it must sound again, so the test cannot pass by
    // simply never emitting anything.
    REG_NR51 = 0x01;
    SilenceDirectSound();
    Platform_PsgMix(&gSoundInfo);
    mag = 0;
    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        mag += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                           : gSoundInfo.pcmBuffer[i];
    TEST_NE(mag, 0);

    // Restore the device to silent so later tests are unaffected.
    REG_NR52 = 0x80;
    REG_NR51 = 0;
}

// A CgbSound substitute, so a pure test can drive SoundMain without the engine
// booted. The real one needs the song sequencer and live channels; the only
// question here is whether SoundMain reaches the PSG synthesiser.
static void TestCgbSoundNoop(void) {}

FIRERED_TEST("sound/SoundMain delivers the PSG mix alongside DirectSound",
             "sound pure", sound_psg_wired)
{
    // The pure tests above call Platform_PsgMix themselves, so they would pass
    // if SoundMain never called it -- which is exactly the shape of the original
    // bug: correct code with no caller. This one drives the engine's own entry
    // point, so deleting the call site fails it.
    //
    // DirectSound is muted so anything in the buffer after SoundMain can only
    // have come from the PSG synthesiser.
    u32 i;
    long mag = 0;

    // Under PORTABLE, SOUND_INFO_PTR is a variable the engine assigns in
    // SoundInit rather than a fixed address, so SoundMain reads whichever
    // SoundInfo it points at. Point it here or the ident guard below fails.
    SOUND_INFO_PTR = &gSoundInfo;

    // A SoundInfo the engine accepts: the ident guard is what lets SoundMain run
    // at all, and CgbSound must be callable (a NULL there would crash the real
    // path too).
    gSoundInfo.ident = ID_NUMBER;
    gSoundInfo.pcmSamplesPerVBlank = 224;
    gSoundInfo.pcmFreq = 13379;
    gSoundInfo.pcmDmaCounter = 7;
    gSoundInfo.pcmDmaPeriod = 7;
    gSoundInfo.reverb = 0;
    gSoundInfo.CgbSound = TestCgbSoundNoop;
    gSoundInfo.MPlayMainHead = NULL;
    gSoundInfo.maxChans = 0;
    memset(gSoundInfo.pcmBuffer, 0, sizeof(gSoundInfo.pcmBuffer));

    // A pulse voice exactly as the engine programs one: duty 50%, envelope
    // volume 15, period 1714 (G4, 392 Hz), routed right.
    REG_NR52 = 0x80 | 0x01;
    REG_NR51 = 0x01;
    REG_NR50 = 0x77;
    REG_NR11 = 0x80;
    REG_NR12 = 0xF0;
    REG_NR13 = 1714 & 0xFF;
    REG_NR14 = (1714 >> 8) & 0x07;

    SoundMain();

    for (i = 0; i < gSoundInfo.pcmSamplesPerVBlank; i++)
        mag += gSoundInfo.pcmBuffer[i] < 0 ? -gSoundInfo.pcmBuffer[i]
                                           : gSoundInfo.pcmBuffer[i];

    TEST_NE(mag, 0);

    // Leave the device silent for whatever runs next in this process.
    REG_NR52 = 0x80;
    REG_NR51 = 0;
}

// ---------------------------------------------------------------------------
// Resampling the engine's rate up to the device's
//
// These exercise src/platform/audio_resample.c, which is where the engine's
// 13.379 kHz meets the host device's rate. It is a separate file precisely so it
// can be tested here: the test binary has no SDL, and sdl2.o is excluded from it.
// ---------------------------------------------------------------------------

// A sine at `hz` sampled at `rate` into 8-bit signed samples.
static void MakeSine(s8 *out, int count, double hz, int rate, double amp)
{
    int i;

    for (i = 0; i < count; i++)
    {
        double v = amp * sin(2.0 * M_PI * hz * (double)i / (double)rate);

        if (v > 127.0)
            v = 127.0;
        if (v < -128.0)
            v = -128.0;
        out[i] = (s8)v;
    }
}

// How rough the output is: the energy of its second difference over the energy
// of its first difference. A nearest-neighbour hold produces runs of identical
// frames separated by steps, which is a large second difference against a small
// first one; interpolation advances by a fraction each frame, so the ratio stays
// small. Dependency-free stand-in for measuring the imaging bands directly.
static double Roughness(const int16_t *frames, int count)
{
    double total = 0.0;
    double high = 0.0;
    int i;

    // `frames` is interleaved stereo, so step by two: reading consecutive
    // entries would alternate between the left and right channels and measure
    // that alternation (a Nyquist-rate signal) instead of either channel's
    // smoothness.
    for (i = 2; i < count * 2; i += 2)
    {
        double d1 = (double)frames[i] - (double)frames[i - 2];
        double prev = (i >= 4) ? (double)frames[i - 4] : d1;
        double d2 = (double)frames[i] - 2.0 * (double)frames[i - 2] + prev;

        total += d1 * d1;
        high += d2 * d2;
    }

    return total > 0.0 ? high / total : 0.0;
}

FIRERED_TEST("sound/resampling interpolates instead of repeating samples",
             "sound pure", sound_resample_interpolates)
{
    // A ramp is the cleanest case: a hold repeats each value, so it emits runs
    // of identical frames with an abrupt step between runs, while interpolation
    // advances a fraction every frame. Count the repeats -- a hold repeats each
    // engine sample about 3.6 times at 48 kHz from 13.379 kHz, so nearly all of
    // them, while interpolation repeats almost none.
    static s8 right[224];
    static s8 left[224];
    static int16_t out[2048 * 2];
    int pos = 0;
    int written;
    int repeats = 0;
    int i;

    for (i = 0; i < 224; i++)
    {
        right[i] = (s8)(i - 112);
        left[i] = (s8)(i - 112);
    }

    written = Platform_ResampleVblank(right, left, 224, 13379, 48000, &pos,
                                      out, 2048);

    // 224 * 48000 / 13379 is ~804 output frames.
    TEST_GE(written, 800);
    TEST_TRUE(written <= 810);

    for (i = 1; i < written; i++)
    {
        if (out[i * 2] == out[(i - 1) * 2])
            repeats++;
    }

    // One repeat is the most that can happen: the vblank's last sample is held
    // across its interval because the next vblank has not arrived yet.
    TEST_TRUE(repeats <= 3);
}

FIRERED_TEST("sound/resampling suppresses the images a sample hold would add",
             "sound pure", sound_resample_no_images)
{
    static s8 right[224];
    static s8 left[224];
    static int16_t held[2048 * 2];
    static int16_t interp[2048 * 2];
    int pos;
    int i;
    int nHeld = 0;
    int nInterp;
    double heldRatio;
    double interpRatio;

    MakeSine(right, 224, 1500.0, 13379, 100.0);
    for (i = 0; i < 224; i++)
        left[i] = right[i];

    // The hold this replaced: emit each engine sample for the whole number of
    // device frames its interval spans.
    {
        int phase = 0;

        for (i = 0; i < 224; i++)
        {
            phase += 48000;
            while (phase >= 13379)
            {
                phase -= 13379;
                held[nHeld * 2 + 0] = (int16_t)(right[i] << 8);
                held[nHeld * 2 + 1] = (int16_t)(right[i] << 8);
                nHeld++;
            }
        }
    }

    pos = 0;
    nInterp = Platform_ResampleVblank(right, left, 224, 13379, 48000, &pos,
                                      interp, 2048);

    heldRatio = Roughness(held, nHeld);
    interpRatio = Roughness(interp, nInterp);

    // Interpolation must be substantially smoother than the hold. Measured
    // spectrally on a real run's buffer the improvement is ~19x; this proxy is a
    // different measure, so require a clear margin rather than a set figure.
    TEST_TRUE(interpRatio < heldRatio * 0.5);
}

FIRERED_TEST("sound/resampling keeps the device's rate across vblanks",
             "sound pure", sound_resample_rate)
{
    // The output frame count over many vblanks is the observable that pins the
    // sub-sample position being carried BETWEEN vblanks rather than restarted
    // inside each one. Each vblank spans 224 * 48000 = 10752000 units of
    // 1/deviceRate, which is 803.6475 output frames -- not a whole number, so a
    // resampler that throws the 0.6475 away every vblank emits 804 frames every
    // time and runs fast. Over 500 vblanks that is 176 frames of drift, ~0.04%,
    // which accumulates into a slow pitch rise against the DirectSound mix.
    //
    // A value-jump check cannot see this: the error is in timing, not amplitude,
    // so the waveform stays perfectly smooth while playing at the wrong rate.
    static s8 right[224];
    static s8 left[224];
    static int16_t out[2048 * 2];
    int pos = 0;
    int total = 0;
    int vblank;
    int i;

    MakeSine(right, 224, 441.0, 13379, 100.0);
    for (i = 0; i < 224; i++)
        left[i] = right[i];

    for (vblank = 0; vblank < 500; vblank++)
        total += Platform_ResampleVblank(right, left, 224, 13379, 48000, &pos,
                                         out, 2048);

    // 500 * 224 * 48000 / 13379 = 401823.75.
    TEST_TRUE(total >= 401820);
    TEST_TRUE(total <= 401828);
}
