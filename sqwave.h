#ifndef SQWAVE_H_INCLUDED
#define SQWAVE_H_INCLUDED

/*
 * sqwave.h
 * 
 * Square wave module of retro synthesizer.
 * 
 * Compilation
 * ===========
 * 
 * May require the math library to be included (-lm).
 */

#include "retrodef.h"

/*
 * The range of quantization amplitudes for the initialization function.
 */
#define SQWAVE_AMP_MIN (16.0)
#define SQWAVE_AMP_MAX (32000.0)

/*
 * The minimum and maximum pitches that may be requested.
 * 
 * This represents the full 88-key keyboard, with middle C at pitch
 * zero.
 */
#define SQWAVE_PITCH_MIN (-39)
#define SQWAVE_PITCH_MAX (48)

/*
 * Initialize the square wave module.
 * 
 * This must be called before all other functions in this module.  It
 * may only be called once.
 * 
 * amp is what to multiply the normalized results by to quantize into
 * integer samples.  It must be finite and greater than zero.  It will
 * automatically be clamped to range [SQWAVE_AMP_MIN, SQWAVE_AMP_MAX].
 * 
 * A floating-point result of 1.0 will be quantized into an integer
 * sample of (amp * result).
 * 
 * samprate is the sampling rate for the computed square waves.  It must
 * be either the RATE_CD or RATE_DVD constant.
 * 
 * Parameters:
 * 
 *   amp - the quantization amplitude
 * 
 *   samprate - the sampling rate
 */
void sqwave_init(double amp, int32_t samprate);

/*
 * Get the square wave sample at a given time point for a given pitch.
 * 
 * The square wave module must be initialized with sqwave_init() before
 * using this function.  The amplitude passed to the initialization
 * function determines the amplitude of the returned wave samples.  The
 * sampling rate passed to the initialization function determines the
 * sampling rate of the returned audio data.
 * 
 * The square waves are generated additively so as to avoid aliasing
 * distortion.
 * 
 * pitch is the pitch of the square wave.  A value of zero is middle C,
 * a value of -1 is one semitone below middle C, a value of +1 is one
 * semitone above middle C, and so forth.  The full range is the 88-key
 * piano keyboard, defined by [SQWAVE_PITCH_MIN, SQWAVE_PITCH_MAX].
 * 
 * t is the sample offset within the wave to return.  t must be zero or
 * greater.  Each square wave sample is automatically looped so that all
 * non-negative values of t are available for all pitches.
 * 
 * Parameters:
 * 
 *   pitch - the pitch of the square wave
 * 
 *   t - the sample offset to request
 * 
 * Return:
 * 
 *   the sample value
 */
int16_t sqwave_get(int32_t pitch, int32_t t);

#endif
