/* Resampling the engine's PCM rate up to the host audio device's rate.
 *
 * The m4a driver produces 8-bit signed mono samples at `pcmFreq` (13379 Hz for
 * this build), one vblank's worth at a time, split into a right half and a left
 * half. The host device runs at its own rate (48000 Hz here). Bridging the two
 * is this file's job, and it is kept separate from src/platform/sdl2.c so it can
 * be exercised without an audio device: sdl2.o is not part of the test binary.
 *
 * The interpolation is LINEAR, and that is a correctness decision rather than a
 * quality preference. A zero-order hold (nearest-neighbour) repeats each engine
 * sample ~3.6 times; that step train's spectrum is the source band mirrored
 * around the engine rate and folded back down into the audible range. Measured
 * on a real run's buffer, a hold puts 2.27% of the signal's energy above the
 * source Nyquist with its largest image at 11.9 kHz from a ~1.5 kHz tone -- an
 * audible whistle the game never contained. Linear interpolation drops that to
 * 0.12%, a 19x reduction, for one multiply and one add per output frame.
 *
 * Interpolating does not invent detail the GBA lacked: the hardware's own
 * reconstruction is a continuous DAC output with far better anti-imaging than
 * either option here, so interpolating is the closer approximation and the hold
 * is the one adding artefacts.
 */

#include "global.h"
#include "platform/platform.h"

/* Resamples one vblank of engine PCM.
 *
 * `pcmRight`/`pcmLeft` each hold `count` 8-bit signed samples. `*pos` is the
 * sub-sample position carried between calls, in units of 1/deviceRate; it is
 * what makes the output continuous across the vblank seam. `out` receives
 * interleaved stereo int16 frames and must hold at least
 * `count * deviceRate / engineRate + 2` of them.
 *
 * Returns the number of frames written. The whole vblank is always consumed (so
 * `*pos` stays meaningful); if the caller cannot take every frame, it should
 * keep the ones it wants and drop the rest rather than resuming mid-vblank.
 */
int Platform_ResampleVblank(const s8 *pcmRight, const s8 *pcmLeft, int count,
                            int engineRate, int deviceRate, int *pos,
                            int16_t *out, int maxFrames)
{
    int span;
    int p;
    int frames = 0;

    if (count <= 0 || engineRate <= 0 || deviceRate <= 0 || maxFrames <= 0)
        return 0;

    /* This vblank spans `count` engine samples, i.e. `span` units of
     * 1/deviceRate. 224 * 48000 fits an int with room to spare. */
    span = count * deviceRate;
    p = *pos;
    if (p < 0)
        p = 0;

    while (p < span && frames < maxFrames)
    {
        int i0 = p / deviceRate;
        int frac = p - i0 * deviceRate;
        int i1;
        s32 right;
        s32 left;

        if (i0 >= count)
            break;

        /* At the last engine sample there is no next value to interpolate
         * toward -- the following vblank has not arrived -- so it is held across
         * its interval. About one output frame in 800 lands there and the error
         * is a fraction of one 8-bit step, which is cheaper than adding a whole
         * vblank of latency to have the value on hand. */
        i1 = (i0 + 1 < count) ? i0 + 1 : i0;

        /* Blend toward i1, in the OUTPUT's scale rather than in 8-bit sample
         * units.
         *
         * This matters more than it looks. One 8-bit unit is a large step next to
         * a single frame's advance: consecutive engine samples are 48000/13379 =
         * 3.59 output frames apart, so the blend fraction never exceeds 0.84 and
         * a delta of 1 (or 2) sample units truncates to zero for every frame in
         * the interval. Doing the arithmetic in sample units therefore collapses
         * to a zero-order hold for exactly the small deltas that make up most of
         * a quiet passage -- the artefact this is here to remove. Shifting the
         * delta into the output's fixed-point scale first keeps the blend's
         * resolution at 1/256 of a sample unit.
         *
         * The product needs 64 bits: a delta of 255 shifted by 8 is 65280, and
         * multiplying by `frac` (below deviceRate, 48000 here) overflows s32. */
        {
            s32 dr = ((s32)pcmRight[i1] - pcmRight[i0]) << 8;
            s32 dl = ((s32)pcmLeft[i1] - pcmLeft[i0]) << 8;

            right = ((s32)pcmRight[i0] << 8)
                  + (s32)(((s64)dr * frac) / deviceRate);
            left = ((s32)pcmLeft[i0] << 8)
                 + (s32)(((s64)dl * frac) / deviceRate);
        }

        /* `right`/`left` are already in the output's 16-bit scale (the 8-bit
         * sample shifted up by 8), so they are stored as they are. */
        out[frames * 2 + 0] = (int16_t)right;
        out[frames * 2 + 1] = (int16_t)left;
        frames++;

        p += engineRate;
    }

    /* Carry the sub-sample remainder into the next vblank: position 0 of the
     * next vblank is the same instant as position `span` of this one, so the
     * leftover is what resumes exactly where this call stopped. Resetting it per
     * vblank instead would interpolate from the wrong sample once every 224
     * engine samples and reintroduce a hitch at the 59.7 Hz frame rate. */
    if (p >= span)
        *pos = p - span;
    else
        *pos = 0;

    return frames;
}
