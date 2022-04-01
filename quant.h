#ifndef QUANT_H_INCLUDED
#define QUANT_H_INCLUDED

/*
 * quant.h
 * =======
 * 
 * The quantization module of the Retro synthesizer.
 * 
 * Compilation:
 * May require the math library with -lm
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The minimum and maximum quantization values.
 */
#define QUANT_MIN (-32767)
#define QUANT_MAX (32767)

/*
 * Initialize the quantization module.
 * 
 * This builds all the lookup tables needed for quantization.  It must
 * be called before any other functions of this module, and it may only
 * be called once.
 * 
 * The center parameter indicates what center boost to use for the
 * stereo imaging function.  This is a quantized loudness value, that 
 * may have any value in range [QUANT_MIN, QUANT_MAX].
 * 
 * The basis of the stereo imaging function is a sine wave and a consine
 * wave for the left and right channels.  This preserves the power of
 * the sound from left to right because:
 * 
 *   (cos(left_amp))^2 + (sin(right_amp))^2 = full_amp^2
 * 
 * However, due to the pan law, it may sound louder in the center than
 * in the left and right extremes.  To correct for this, as the stereo
 * image approaches the center, it is increasingly scaled, up to a
 * scaling value given by the quantized center loudness value.  If the
 * center loudness value is 0, then the scaling value is 1.0 and there
 * is 0dB change in the center.  Positive loudness values boost the
 * center even further, while negative loudness values drop the center.
 * 
 * The best value to use depends on a lot of things about the acoustic
 * environment.  -3dB (quantized value -1080) or -4.5dB (quantized value
 * -1620) are two conventional values used for this center correction.
 * If you are going to be mixing the stereo down to single-channel by
 * summing the two channels together, -6dB (quantized value -2160) might
 * be a good choice because that's a value of about 1/2.
 * 
 * Parameters:
 * 
 *   center - the quantized center correction loudness
 */
void quant_init(int32_t center);

/*
 * Given a quantized pitch value, return the frequency in Hz.
 * 
 * You must call quant_init() before using this function or a fault
 * occurs.
 * 
 * p may have any value in range [QUANT_MIN, QUANT_MAX].  The return
 * value is a finite floating-point value that is greater than zero and
 * represents the frequency of the pitch in Hz.  A lookup table is used
 * internally to speed computation.
 * 
 * A quantized value of zero corresponds to Middle C (C4).  Each
 * quantized unit is 1/5 of a cent, or in other words 1/500 of a
 * semitone.  Therefore, 500 quantized units are a semitone (half step)
 * and 1000 quantized units are a whole tone (whole step).  There are
 * 6000 quantized units in an octave, which is equal to 12 semitones.
 * 
 * Therefore, for example, a quantized value of -500 is B3, which is B
 * below middle C, and a quantized value of 8000 is E5, which is E above
 * the C one octave above middle C.
 * 
 * 12-tone equal temperament is used as the tuning system (though the
 * quantized resolution of 1/5 of a cent is more than accurate enough to
 * represent other tuning systems, if you compute where the tones lie in
 * relation to 12-tone equal temperament).  The frequency for quantized
 * value 4500 (corresponding to A4) is exactly 440.0 Hz, and all other
 * frequencies are calculated relative to this.
 * 
 * The range of [QUANT_MIN, QUANT_MAX] expressed in standard pitch names
 * is a bit wider than the range [G(-2), F9], which easily includes the
 * whole 88-key piano range of [A0, C8].  The Retro synthesizer range of
 * [QUANT_MIN, QUANT_MAX] is very close to the standard MIDI note number
 * range, except the MIDI range goes two semitones above Retro on the
 * high end of the range, and Retro goes five semitones below MIDI on
 * the low end of the range.  (Expressed in MIDI note numbers, Retro's
 * range would be [-5, 125] versus the standard MIDI range of [0, 127].)
 * 
 * Expressed in raw frequencies, [QUANT_MIN, QUANT_MAX] covers a range
 * of approximately [6 Hz, 11175 Hz].
 * 
 * Parameters:
 * 
 *   p - the quantized pitch value, as an offset in 1/5-cents from
 *   middle C
 * 
 * Return:
 * 
 *   the frequency in Hz
 */
double quant_pitch(int32_t p);

/*
 * Given a quantized loudness value, return the amplitude multiplier.
 * 
 * You must call quant_init() before using this function or a fault
 * occurs.
 * 
 * i may have any value in range [QUANT_MIN, QUANT_MAX].  The return
 * value is a finite floating-point value that is greater than zero and
 * represents a value to multiply to something else to adjust amplitude.
 * A lookup table is used internally to speed computation.
 * 
 * A quantized value of zero corresponds to a scaling value of 1.0 which
 * means no change during scaling.  Quantized values greater than zero
 * correspond to scaling values greater than 1.0, which increase
 * amplitude.  Quantized values less than zero correspond to scaling
 * values between 0.0 and 1.0 (exclusive of boundaries), which decrease
 * amplitude.
 * 
 * The quantized unit is 1/360 of a decibel.  6dB is a scaling value of
 * approximately 2.0, which is a quantized value of 2160.  -6dB is a
 * scaling value of approximately 0.5, which is a quantized value of
 * -2160.  If a quantized value q maps to a scaling value w, then the
 * quantized value -q will always map to the scaling value 1/w.
 * 
 * The maximum value QUANT_MAX maps to approximately 91 dB, which is a
 * multiplier of about 35561.  The minimum value QUANT_MIN therefore
 * maps to approximately -91 dB, which is a multiplier of about 1/35561.
 * 
 * Parameters:
 * 
 *   i - the quantized loudness value, in 1/360 dB units
 * 
 * Return:
 * 
 *   the amplitude multiplier
 */
double quant_loud(int32_t i);

/*
 * Given a quantized stereo position value, return the left and right
 * channel multipliers.
 * 
 * You must call quant_init() before using this function or a fault
 * occurs.  The center parameter that you pass to quant_init() will
 * affect the values returned by this function.
 * 
 * p may have any value in range [QUANT_MIN, QUANT_MAX].  The return
 * values will be written to *sl and *sr for the left and right channel
 * scaling values, respectively.  A lookup table is used internally to
 * speed computation.
 * 
 * A quantized value of zero corresponds to full center stereo location.
 * A quantized value of QUANT_MIN corresponds to full left stereo
 * location.  A quantized value of QUANT_MAX corresponds to full right
 * stereo location.
 * 
 * Samples from a single-channel source should be multipled by *sl to
 * get the left channel samples, or multipled by *sr to get the right
 * channel samples.  These computed multipliers will position the sound
 * within the stereo image at a position corresponding to the quantized
 * p value.
 * 
 * From left to right, the left channel follows a cosine curve from zero
 * to PI/2 radians, while the right channel follows a sine curve from
 * zero to PI/2 radians.  This is intended to preserve power across the
 * whole left to right spectrum because:
 * 
 *   (cos(left_amp))^2 + (sin(right_amp))^2 = full_amp^2
 * 
 * Due the pan law, however, the sides normally sound softer than the
 * center.  To fix this, the center value that is passed to quant_init()
 * is tapered in on a sine curve that reaches its maximum value at full
 * center and tapers to zero on both left and right sides.  See the
 * documentation of quant_init() for further information.
 * 
 * Parameters:
 * 
 *   p - the quantized stereo position
 * 
 *   sl - pointer to variable to receive the left channel multiplier
 * 
 *   sr - pointer to variable to receive the right channel multiplier
 */
void quant_pan(int32_t p, double *sl, double *sr);

#endif
