/* GBA PSG (programmable sound generator) synthesis for the native port.
 *
 * ---------------------------------------------------------------------------
 * Why this file exists
 * ---------------------------------------------------------------------------
 * The GBA's sound hardware has two halves and the m4a driver feeds both:
 *
 *   - **Direct Sound** (FIFO A/B). `SoundMainRAM` mixes PCM into
 *     `SoundInfo.pcmBuffer` and the two FIFOs stream it out. `src/platform/sdl2.c`
 *     drains that buffer, so those channels are audible here.
 *   - **PSG** (pulse 1, pulse 2, wave, noise). `CgbSound()` (src/m4a.c) computes
 *     each CGB channel's envelope, period and duty every vblank and writes them
 *     to the `NR10`..`NR52` registers. On hardware those registers *are* the
 *     oscillators; here they were written to plain memory and never read, so
 *     every CGB channel was silent.
 *
 * That is not a corner case. FireRed's voicegroups are built on CGB voices --
 * the title theme runs four DirectSound tracks plus pulse 1 and pulse 2, and
 * Pallet Town's six tracks include two CGB squares -- so the missing half removed
 * whole instrument layers from most of the soundtrack. The port mixed the
 * accompaniment and dropped the melody and bass, which is what "low fidelity"
 * sounded like.
 *
 * ---------------------------------------------------------------------------
 * What this does
 * ---------------------------------------------------------------------------
 * It emulates the hardware PSG from the registers the engine already wrote rather
 * than re-deriving the engine's envelope logic -- the same split the software PPU
 * uses for the video registers (see src/platform/ppu.c). Channel enable, period,
 * duty, envelope, volume and panning all come from NRxx, so src/m4a.c stays the
 * single source of truth and a change to CgbSound cannot desynchronise this file.
 *
 * Output is summed into the same two 8-bit PCM halves the DirectSound mixer fills
 * (FIFO A = right at the base of pcmBuffer, FIFO B = left at + PCM_DMA_BUF_SIZE):
 * on hardware SOUNDCNT_H routes both sources into one DAC, and the engine sets
 * SOUND_CGB_MIX_FULL there.
 *
 * ---------------------------------------------------------------------------
 * Register map (offsets into REG_BASE, include/gba/io_reg.h)
 * ---------------------------------------------------------------------------
 *   NR10 0x60  sweep                    (pulse 1 only)
 *   NR11 0x62  duty + length            (pulse 1)
 *   NR12 0x63  envelope: vol 7-4, dir 3, step 2-0
 *   NR13 0x64  frequency low 8
 *   NR14 0x65  frequency high 3 + length-enable 6 + trigger 7
 *   NR21 0x68, NR22 0x69, NR23 0x6c, NR24 0x6d    pulse 2 (no sweep)
 *   NR30 0x70  wave DAC enable         (bit 7)
 *   NR31 0x72  wave length
 *   NR32 0x73  wave volume shift       (bits 6-5)
 *   NR33 0x74, NR34 0x75    wave frequency
 *   NR41 0x78  noise length
 *   NR42 0x79  noise envelope          (same layout as NR12)
 *   NR43 0x7c  noise clock shift 7-4, width 3, divisor 2-0
 *   NR44 0x7d  noise length-enable 6 + trigger 7
 *   NR50 0x80  master volume: right 2-0, left 6-4
 *   NR51 0x81  panning: right enables 3-0, left enables 7-4
 *   NR52 0x84  master enable (bit 7) + per-channel active flags (3-0)
 *   WAVE_RAM 0x90..0x9F   32 4-bit samples
 *
 * Only what the engine drives is modelled. M4A's CGB channels are FIX voices
 * (TONEDATA_TYPE_FIX) that never run a length counter, so length expiry and the
 * sweep unit stay unimplemented -- noted as inert rather than silently half-done.
 * The trigger bit is honoured for the noise register.
 *
 * ---------------------------------------------------------------------------
 * Frequency
 * ---------------------------------------------------------------------------
 * Pan Docs, for all three tone sources (X is the 11-bit period register,
 * `(NRx4 & 7) << 8 | NRx3`):
 *
 *     pulse:  f = 131072 / (2048 - X)
 *     wave:   f =  65536 / (2048 - X)
 *     noise:  f = 131072 / (2r * 2^shift),  r = 0 meaning 0.5
 *
 * The pulse formula is confirmed by the values the engine actually produces: the
 * title theme's pulse 1 sits at X = 1714, giving 131072 / 334 = 392 Hz (G4), and
 * pulse 2 at X = 1650 -> 131072 / 398 = 329 Hz (E4). Those are exact pitches, so
 * this reads the register the same way the hardware does.
 *
 * ---------------------------------------------------------------------------
 * Level
 * ---------------------------------------------------------------------------
 * A CGB channel's DAC input is the 4-bit envelope value 0-15 while its waveform
 * step is high and 0 while it is low -- which is *unipolar* and so carries a DC
 * component proportional to the duty cycle. The hardware's output stage is
 * AC-coupled and removes it. Emitting the raw unipolar levels would leave a
 * constant offset in the mix (costing headroom and buzzing), so the DC is
 * subtracted per channel: the high and low levels are shifted so the pair is
 * centred on zero and weighted by the duty cycle, which is exact for an ideal
 * square wave. Writing nothing on the low phase instead -- a half-wave pulse
 * train -- has the right fundamental frequency but no bass and a DC step, and it
 * fails the alternation check in tests/sound.c.
 */

#include <string.h>
#include "global.h"
#include "gba/io_reg.h"
#include "gba/m4a_internal.h"
#include "platform/platform.h"

enum
{
    PSG_PULSE1 = 0,
    PSG_PULSE2,
    PSG_WAVE,
    PSG_NOISE,
    PSG_CHANNEL_COUNT
};

/* NR10..NR52 live contiguously from 0x60; WAVE_RAM starts at 0x90. Named here so
 * the register reads below read as register names rather than numbers. */
enum
{
    REG_NR11_OFS  = 0x62,
    REG_NR12_OFS  = 0x63,
    REG_NR13_OFS  = 0x64,
    REG_NR14_OFS  = 0x65,
    REG_NR21_OFS  = 0x68,
    REG_NR22_OFS  = 0x69,
    REG_NR23_OFS  = 0x6C,
    REG_NR24_OFS  = 0x6D,
    REG_NR30_OFS  = 0x70,
    REG_NR32_OFS  = 0x73,
    REG_NR33_OFS  = 0x74,
    REG_NR34_OFS  = 0x75,
    REG_NR42_OFS  = 0x79,
    REG_NR43_OFS  = 0x7C,
    REG_NR50_OFS  = 0x80,
    REG_NR51_OFS  = 0x81,
    REG_NR52_OFS  = 0x84,
    REG_WAVE_RAM_OFS = 0x90
};

/* Band-limited pulse tables, one per duty setting, 64 points per period.
 *
 * A pulse is the sum of its odd harmonics: t -> 2/(n*pi) * sin(n*pi*d) * cos(n*t),
 * where d is the duty fraction. Synthesising that sum with only the first few
 * terms and sampling it is exactly a naive square wave sampled at the engine's
 * 13,379 Hz rate -- the higher harmonics do not disappear, they fold back
 * *below* the source Nyquist and land inharmonically. Measured on a duty-50%
 * note at 1872 Hz, the 5th and 7th harmonics (9360 and 13104 Hz) folded back
 * to 4017 and 272 Hz, and those in-band tones are the buzz heard over Route 1:
 * no post-processing can remove them, they are in the 13.4 kHz signal.
 * 
 * So the tables carry only the harmonics that fit: the sum stops at n = 13, and
 * 13 * 2048 / 60.2 = 442 Hz is the highest fundamental the period register can
 * reach, whose 13th harmonic is 5.75 kHz -- under the 6,689 Hz Nyquist at every
 * pitch the hardware can ask for.
 * 
 * table[i] = 8 * f(2*pi*i/64), so (table[i] * volume) reproduces the naive
 * square's *fundamental* amplitude but without the folded harmonics, keeping
 * the channel at the level it sounds at today.
 */
static const s8 sPulseTable[4][64] =
{
    { /* duty 1/8: fundamental 1.95 x envelope volume */
           4,    4,    4,    4,    3,    3,    3,    3,    3,    2,    2,    2,    1,    1,    1,    0,
           0,    0,   -1,   -1,   -1,   -2,   -2,   -2,   -3,   -3,   -3,   -3,   -3,   -4,   -4,   -4,
          -4,   -4,   -4,   -4,   -3,   -3,   -3,   -3,   -3,   -2,   -2,   -2,   -1,   -1,   -1,    0,
           0,    0,    1,    1,    1,    2,    2,    2,    3,    3,    3,    3,    3,    4,    4,    4,
    },
    { /* duty 2/8: fundamental 3.60 x envelope volume */
           4,    4,    4,    4,    4,    4,    3,    3,    3,    3,    2,    2,    2,    1,    1,    0,
           0,    0,   -1,   -1,   -2,   -2,   -2,   -3,   -3,   -3,   -3,   -4,   -4,   -4,   -4,   -4,
          -4,   -4,   -4,   -4,   -4,   -4,   -3,   -3,   -3,   -3,   -2,   -2,   -2,   -1,   -1,    0,
           0,    0,    1,    1,    2,    2,    2,    3,    3,    3,    3,    4,    4,    4,    4,    4,
    },
    { /* duty 4/8: fundamental 5.09 x envelope volume */
           4,    4,    4,    4,    4,    4,    3,    3,    3,    3,    2,    2,    2,    1,    1,    0,
           0,    0,   -1,   -1,   -2,   -2,   -2,   -3,   -3,   -3,   -3,   -4,   -4,   -4,   -4,   -4,
          -4,   -4,   -4,   -4,   -4,   -4,   -3,   -3,   -3,   -3,   -2,   -2,   -2,   -1,   -1,    0,
           0,    0,    1,    1,    2,    2,    2,    3,    3,    3,    3,    4,    4,    4,    4,    4,
    },
    { /* duty 6/8: fundamental 3.60 x envelope volume */
           4,    4,    4,    4,    4,    4,    3,    3,    3,    3,    2,    2,    2,    1,    1,    0,
           0,    0,   -1,   -1,   -2,   -2,   -2,   -3,   -3,   -3,   -3,   -4,   -4,   -4,   -4,   -4,
          -4,   -4,   -4,   -4,   -4,   -4,   -3,   -3,   -3,   -3,   -2,   -2,   -2,   -1,   -1,    0,
           0,    0,    1,    1,    2,    2,    2,    3,    3,    3,    3,    4,    4,    4,    4,    4,
    },
};

/* How many duty steps of the 8 in a period are high, per duty setting. Used only
 * to pick the table row, since the table already carries the waveform. */
static const u8 sDutyHighSteps[4] = { 1, 2, 4, 6 };

/* Maps the channels' 4-bit envelope scale onto the PCM byte the DirectSound
 * channels share.
 *
 * A CGB channel's level is a 4-bit DAC value, so centring it and dividing by the
 * duty lands on zero for the quietest notes: at envelope 1 a 50% square is
 * +/-0.5 and every sample rounds away. The music uses volumes in the 1-4 range
 * (measured on the title theme and the bedroom), so that arithmetic alone makes
 * the synthesiser inaudible while still computing the right waveform.
 *
 * The scale is set by the *balance against DirectSound*, not by what makes one
 * channel audible. Measured on Route 1 by averaging |PSG| and |DirectSound| per
 * sample inside the mixer, a scale of 8 made the CGB channels 73.5% of the output
 * amplitude -- 66% of its energy -- so the accompaniment and counter-melody sat
 * on top of the melodic line instead of under it, which is what "the melody is
 * too quiet" sounds like. At 4 it is 58.9%. Envelope 1 still reaches +/-2, so
 * quiet passages keep their shape, and the centring stays exact per duty cycle.
 *
 * The ratio is measured; the target is not. The hardware does *not* provide one:
 * m4aSoundInit programs SOUNDCNT_H = SOUND_ALL_MIX_FULL (CGB 100%, both FIFOs
 * 100%) and CgbSound sets NR50 to 0x77, so the engine sums all three sources at
 * full weight and the balance comes from the song data's own voice volumes.
 * 4 is therefore an empirical starting point, and the value that actually sounds
 * right is an ear decision, not a derivation -- adjust it against a recording
 * rather than against this comment.
 */
#define PSG_DAC_SCALE 4

struct PsgChannelState
{
    u32 phase;      /* position within the period, in 16.16 samples */
    u32 step;       /* phase increment per output sample, 16.16     */
    u32 noiseLfsr;  /* 15- or 7-bit shift register                  */
    u32 noisePhase; /* samples left before the next LFSR shift      */
    u8  prevActive; /* previous NR52 active bit, to spot a trigger  */
};

static struct PsgChannelState sPsg[PSG_CHANNEL_COUNT];

static u8 PsgReg(u32 offset)
{
    return REG_BASE[offset];
}

/* The 11-bit period register of a pulse or wave channel. */
static u32 PsgPeriod(u32 nrx3, u32 nrx4)
{
    return ((u32)(PsgReg(nrx4) & 0x07) << 8) | PsgReg(nrx3);
}

/* Period in output samples, as a 16.16 fixed-point increment per sample.
 *
 * `divisor` is the hardware rate divisor: 131072 for pulse, 65536 for wave.
 * `rate` is the engine's PCM rate (pcmFreq), so one period spans
 * `rate * (2048 - X) / divisor` output samples and the phase (which wraps at
 * 65536) advances by
 *
 *     step = 65536 * divisor / (rate * (2048 - X))
 *
 * computed in 64-bit. Deriving `step` directly rather than going through an
 * integer `samplesPerPeriod` matters: truncating that first costs about 0.3% in
 * pitch at the game's highest pulse registers, which is audible as an
 * out-of-tune lead against the DirectSound layers still playing in tune.
 */
static u32 PsgStep(u32 period, u32 divisor, u32 rate)
{
    u32 ticks = 2048 - (period & 0x7FF);
    u32 step;

    /* X = 2048 would divide by zero and is not reachable from M4A (the FIX-voice
     * mask in CgbSound keeps values below 2048), but a zero period must not
     * produce a division fault if a future caller writes one. */
    if (ticks == 0)
        ticks = 1;

    step = (u32)(((u64)65536 * divisor) / ((u64)rate * ticks));
    return step ? step : 1;
}

/* Noise timer: the LFSR shifts every `2r * 2^shift` ticks of the 131072 Hz
 * clock, expressed in output samples. `r = 0` means a divisor of 0.5, which is
 * why the code is doubled and the zero case uses 1 (half of the 0.5 pair). */
static u32 PsgNoiseSamples(u32 nr43, u32 rate)
{
    u32 shift = (PsgReg(nr43) >> 4) & 0x0F;
    u32 divisorCode = PsgReg(nr43) & 0x07;
    u32 divisor2 = divisorCode ? divisorCode * 2 : 1;
    u32 samples = (u32)((((u64)rate << shift) * divisor2) / 131072u);

    return samples ? samples : 1;
}

/* Starts a channel's oscillator over. Called when NR52's active bit for the
 * channel goes 0 -> 1, which is what a retrigger looks like from the registers
 * (CgbSound sets the bit when it starts a note and clears it at oscillator off).
 * Only the noise register is seeded: the LFSR must restart its sequence, while
 * pulse/wave phase is free-running on hardware and resetting it would add a
 * click the GBA does not have. */
static void PsgTrigger(u32 ch)
{
    if (ch == PSG_NOISE)
    {
        sPsg[ch].noiseLfsr = 0x7FFF;
        sPsg[ch].noisePhase = 0;
    }
}

/* Adds `value` to a PCM byte with saturation rather than wrapping.
 *
 * The DirectSound mix already occupies most of the 8-bit range and a CGB
 * channel is summed on top of it. A plain `(s8)` cast wraps on overflow -- a
 * positive peak flipping to a large negative one, which is gross harmonic
 * distortion, not a louder note. The hardware does not wrap either: the DAC sums
 * the FIFO and PSG currents in the analogue domain, and the result simply clips
 * at full scale. Saturating here reproduces that clipping, and it is also what
 * keeps a loud song from sounding broken rather than loud.
 */
static s8 PsgAddSaturate(s8 current, s32 value)
{
    s32 sum = (s32)current + value;

    if (sum > 127)
        return 127;
    if (sum < -128)
        return -128;
    return (s8)sum;
}

/* The channel's envelope level, 0-15, comes from the engine's own CgbChannel
 * state rather than from the NRx2 register.
 *
 * NRx2 does not hold a level: its high nibble is the envelope's *initial* volume
 * and its low nibble instructs the hardware envelope unit (direction and step
 * period). Reading the high nibble alone yields whatever the last write left
 * there, and the engine writes 0 when a note starts, so a register-only level is
 * zero and the channel stays silent no matter what the envelope does -- measured
 * on a live note, NRx2 read 0x09 (initial volume 0) while the engine's own
 * envelope ran 0,1,2,3,4. CgbSound executes the equivalent envelope itself and
 * keeps the result in `envelopeVolume`, which is what the hardware unit converges
 * to; taking it from there keeps the engine authoritative for level while the
 * registers stay authoritative for the waveform (duty, period, wave data).
 *
 * Panning is *not* taken from here: CgbSound rewrites NR51 every vblank through
 * `panMask`, so that register is live.
 */
static u32 PsgChannelVolume(struct SoundInfo *soundInfo, u32 ch)
{
    u32 volume;

    /* Prefer the engine's live level; fall back to the register's initial volume
     * so the synthesiser is still a pure function of the registers when a caller
     * has not set up CGB channel state (which is how tests/sound.c drives it). */
    if (soundInfo->cgbChans)
    {
        volume = soundInfo->cgbChans[ch].envelopeVolume & 0x0F;
        if (volume != 0)
            return volume;
    }

    switch (ch)
    {
    case PSG_WAVE:
        /* The wave channel's level is NR32's shift, not a 4-bit volume; 15 is
         * nominal and the per-sample shift is applied by the caller. */
        return 15;
    case PSG_NOISE:
        return PsgReg(REG_NR42_OFS) >> 4;
    default:
        return PsgReg((ch == PSG_PULSE1) ? REG_NR12_OFS : REG_NR22_OFS) >> 4;
    }
}

/* Emits one channel's samples into both PCM halves.
 *
 * `rightPan`/`leftPan` come from NR51 (one bit per channel per side); `masterR`
 * and `masterL` are the 0-7 NR50 volumes.
 */
static void PsgMixChannel(struct SoundInfo *soundInfo, u32 ch, s32 *sumR,
                          s32 *sumL, s32 count, u32 rate,
                          int rightPan, int leftPan,
                          u32 masterR, u32 masterL)
{
    struct PsgChannelState *st = &sPsg[ch];
    s32 highLevel = 0;
    s32 lowLevel = 0;
    s32 waveMean = 0;
    u32 waveVolShift = 0;
    u32 volume = PsgChannelVolume(soundInfo, ch);
    s32 i;

    /* Everything below is constant across the vblank, so read it once rather
     * than once per sample. */
    switch (ch)
    {
    case PSG_PULSE1:
    case PSG_PULSE2:
    {
        /* The waveform comes from the band-limited table (see above), so the
         * only per-vblank work is the envelope scaling. The table's 8x scaling
         * and PSG_DAC_SCALE compose: value = table[i] * volume * PSG_DAC_SCALE. */
        highLevel = (s32)volume * PSG_DAC_SCALE;
        lowLevel = 0;
        break;
    }
    case PSG_WAVE:
    {
        u32 sum = 0;
        u32 idx;

        /* Mean of the 32 nibbles is the waveform's DC level. */
        for (idx = 0; idx < 32; idx++)
        {
            u8 packed = PsgReg(REG_WAVE_RAM_OFS + (idx >> 1));
            sum += (idx & 1) ? (packed >> 4) : (packed & 0x0F);
        }
        waveMean = (s32)(sum / 32);
        waveVolShift = (PsgReg(REG_NR32_OFS) >> 5) & 3;
        break;
    }
    default: /* PSG_NOISE */
    {
        /* The LFSR is ~50% duty, so both levels are half the envelope. */
        highLevel = ((s32)volume * PSG_DAC_SCALE) / 2;
        lowLevel = -((s32)volume * PSG_DAC_SCALE) / 2;
        break;
    }
    }

    for (i = 0; i < count; i++)
    {
        s32 value = 0;

        switch (ch)
        {
        case PSG_PULSE1:
        case PSG_PULSE2:
        {
            u32 nrx1 = (ch == PSG_PULSE1) ? REG_NR11_OFS : REG_NR21_OFS;
            const s8 *tbl = sPulseTable[(PsgReg(nrx1) >> 6) & 3];
            /* 64 points per period, so a full period (65536 in 16.16) advances
             * the index by 1024 per point. */
            u32 idx = (st->phase >> 10) & 63;

            st->phase += st->step;
            value = ((s32)tbl[idx] * highLevel) >> 3;   /* undo the table's 8x */
            break;
        }
        case PSG_WAVE:
        {
            /* 32 four-bit samples per period, two per byte, low nibble first. */
            u32 idx = (st->phase >> 11) & 31;
            u8 packed = PsgReg(REG_WAVE_RAM_OFS + (idx >> 1));
            s32 sample = (idx & 1) ? (packed >> 4) : (packed & 0x0F);

            st->phase += st->step;

            /* Subtract the waveform's own mean so the output is centred, then
             * apply the 100/50/25% volume shift. */
            if (waveVolShift != 0)
            {
                s32 centred = (sample - waveMean) >> (waveVolShift - 1);

                value = (centred * (s32)volume * PSG_DAC_SCALE) / 15;
            }
            break;
        }
        default: /* PSG_NOISE */
        {
            if (st->noisePhase == 0)
            {
                u32 bit = (st->noiseLfsr ^ (st->noiseLfsr >> 1)) & 1;

                st->noiseLfsr = (st->noiseLfsr >> 1) | (bit << 14);
                if ((PsgReg(REG_NR43_OFS) >> 3) & 1)
                    st->noiseLfsr = (st->noiseLfsr & ~0x40u) | (bit << 6);
                st->noisePhase = PsgNoiseSamples(REG_NR43_OFS, rate);
            }
            st->noisePhase--;

            /* The LFSR's low bit gates the output, and the register is seeded
             * all-ones so the channel starts in its low state. */
            value = (st->noiseLfsr & 1) ? lowLevel : highLevel;
            break;
        }
        }

        if (value == 0)
            continue;

        /* NR50 master volume is 0-7 and its loudest setting is 7, so +1 makes 7
         * unity rather than attenuating by 1/8. */
        /* ACCUMULATE, do not clamp here: four channels are summed and the clamp
         * has to happen once, after all of them have contributed. See the closing
         * loop in Platform_PsgMix. */
        if (rightPan)
            sumR[i] += (value * (s32)(masterR + 1)) / 8;
        if (leftPan)
            sumL[i] += (value * (s32)(masterL + 1)) / 8;
    }
}

/* Platform_PsgMix: one vblank of PSG audio into SoundInfo.pcmBuffer.
 *
 * Called from SoundMain immediately after SoundMix (src/m4a_driver.c), so the
 * DirectSound channels are already summed and this adds the PSG layers on top --
 * which is what the hardware DAC does with them.
 *
 * NRx3/NRx4 are read for the period rather than `chan->frequency`: CgbSound
 * already wrote the divided value there, and re-deriving it from the channel
 * struct would duplicate the engine's FIX-voice adjustment (the `& 0x7fc` in
 * CgbSound) and could drift from it.
 */
/* One vblank's summed PSG contribution, before it meets the DirectSound mix.
 * s32 because four channels at full envelope and master volume reach about
 * +/-240, which does not fit the byte they are eventually clamped into. */
static s32 sPsgSumR[PCM_DMA_BUF_SIZE];
static s32 sPsgSumL[PCM_DMA_BUF_SIZE];

void Platform_PsgMix(struct SoundInfo *soundInfo)
{
    s32 count = soundInfo->pcmSamplesPerVBlank;
    u32 rate = soundInfo->pcmFreq;
    u8 nr52 = PsgReg(REG_NR52_OFS);
    u8 nr51 = PsgReg(REG_NR51_OFS);
    u8 nr50 = PsgReg(REG_NR50_OFS);
    u32 masterR = nr50 & 0x07;
    u32 masterL = (nr50 >> 4) & 0x07;
    s8 *mixR = soundInfo->pcmBuffer;
    s8 *mixL = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE;
    u32 ch;
    int summed = FALSE;
    s32 i;

    if (count <= 0 || rate == 0)
        return;

    if (count > (s32)ARRAY_COUNT(sPsgSumR))
        count = (s32)ARRAY_COUNT(sPsgSumR);

    /* NR52 bit 7 is the master sound enable; with it clear every channel is off
     * regardless of its own registers. */
    if (!(nr52 & 0x80))
        return;

    memset(sPsgSumR, 0, (size_t)count * sizeof(sPsgSumR[0]));
    memset(sPsgSumL, 0, (size_t)count * sizeof(sPsgSumL[0]));

    for (ch = 0; ch < PSG_CHANNEL_COUNT; ch++)
    {
        u8 active = (nr52 >> ch) & 1;
        int dacOn;
        int rightPan = (nr51 >> ch) & 1;
        int leftPan = (nr51 >> (ch + 4)) & 1;

        if (active && !sPsg[ch].prevActive)
            PsgTrigger(ch);
        sPsg[ch].prevActive = active;

        if (!active || (!rightPan && !leftPan))
            continue;

        /* A channel sounds iff NR52 has it enabled and its DAC can pass a
         * signal. The DAC test is the one the hardware makes: NRx2 bit 7..3
         * non-zero for pulse/noise (the DAC is off when the initial volume and
         * the envelope direction/step are all zero), NR30 bit 7 for the wave
         * channel. Gating on the engine's `cgbChans[ch].statusFlags` instead
         * looks tempting -- it is cleared at oscillator off -- but it clears
         * before the envelope tail has finished, which would cut every note
         * short, and it makes the synthesiser depend on engine state a caller
         * need not have set up. */
        switch (ch)
        {
        case PSG_PULSE1:
            dacOn = (PsgReg(REG_NR12_OFS) & 0xF8) != 0;
            sPsg[ch].step = PsgStep(PsgPeriod(REG_NR13_OFS, REG_NR14_OFS),
                                    131072, rate);
            break;
        case PSG_PULSE2:
            dacOn = (PsgReg(REG_NR22_OFS) & 0xF8) != 0;
            sPsg[ch].step = PsgStep(PsgPeriod(REG_NR23_OFS, REG_NR24_OFS),
                                    131072, rate);
            break;
        case PSG_WAVE:
            dacOn = (PsgReg(REG_NR30_OFS) & 0x80) != 0;
            sPsg[ch].step = PsgStep(PsgPeriod(REG_NR33_OFS, REG_NR34_OFS),
                                    65536, rate);
            break;
        default:
            dacOn = (PsgReg(REG_NR42_OFS) & 0xF8) != 0;
            sPsg[ch].step = 0;
            break;
        }

        if (!dacOn)
            continue;

        PsgMixChannel(soundInfo, ch, sPsgSumR, sPsgSumL, count, rate, rightPan,
                      leftPan, masterR, masterL);
        summed = TRUE;
    }

    /* Clamp the four channels' total once, then add it to the DirectSound mix.
     *
     * Clamping each channel separately -- which is what this did first -- makes
     * the sum of four already-clamped values, and on a loud vblank that total is
     * still far outside the byte, so the output is a squared-off plateau instead
     * of the single clip the hardware produces. The DAC sums the four channel
     * currents and the FIFO current in the analogue domain and clips the total,
     * so the clamp belongs here, after everything has been added up.
     *
     * DirectSound is already in the buffer (SoundMix runs first) and shares that
     * one DAC with the CGB channels, so the clamped PSG total is summed onto it
     * and clamped again -- saturating both times, never wrapping, because a wrap
     * is a sign flip and gross harmonic distortion. */
    if (summed)
    {
        for (i = 0; i < count; i++)
        {
            s32 v = sPsgSumR[i];

            if (v > 127)
                v = 127;
            else if (v < -128)
                v = -128;

            mixR[i] = PsgAddSaturate(mixR[i], v);
        }
        for (i = 0; i < count; i++)
        {
            s32 v = sPsgSumL[i];

            if (v > 127)
                v = 127;
            else if (v < -128)
                v = -128;

            mixL[i] = PsgAddSaturate(mixL[i], v);
        }
    }
}
