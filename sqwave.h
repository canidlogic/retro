#ifndef SQWAVE_H_INCLUDED
#define SQWAVE_H_INCLUDED

/*
 * sqwave.h
 * 
 * Square wave module of the retro synthesizer.
 * 
 * Compilation
 * ===========
 * 
 * May require the math library to be included with -lm
 */

#include <stdint.h>

/*
 * The minimum and maximum possible values for half the length of the
 * window function.
 */
#define SQWAVE_HLEN_MIN (4)
#define SQWAVE_HLEN_MAX (2048)

/*
 * The minimum and maximum possible values for the cutoff frequency as a
 * fraction of the Nyquist limit.
 */
#define SQWAVE_CUTOFF_MIN (0.25)
#define SQWAVE_CUTOFF_MAX (0.99)

/*
 * The minimum and maximum possible values for the integer output value
 * of floating-point 1.0.
 */
#define SQWAVE_SNORM_MIN (256)
#define SQWAVE_SNORM_MAX (32767)

/*
 * Initialize the square wave module.
 * 
 * This function must be called before any of the other functions.  It
 * may only be called once.
 * 
 * winhlen is half the length of the window function to use.  It must be
 * in range [SQWAVE_HLEN_MIN, SQWAVE_HLEN_MAX].  Higher values lead to
 * higher quality results, but cause the square wave generator to
 * require more computation for each sample.
 * 
 * cutoff indicates the cutoff frequency as a fraction of the Nyquist
 * limit.  It must be a finite value.  It is automatically clamped to
 * the range [SQWAVE_CUTOFF_MIN, SQWAVE_CUTOFF_MAX].  A value of 1.0
 * means the cutoff should be as close to the Nyquist limit as possible.
 * A value of 0.5 means half the Nyquist limit, and so forth.
 * 
 * sampnorm is the output sample integer value that should correspond to
 * a floating-point value of 1.0.  It must be in the range 
 * [SQWAVE_SNORM_MIN, SQWAVE_SNORM_MAX].
 * 
 * Parameters:
 * 
 *   winhlen - half the window function length
 * 
 *   cutoff - the cutoff as a fraction of the Nyquist limit
 * 
 *   sampnorm - the integer value corresponding to 1.0
 */
void sqwave_init(
    int32_t winhlen,
    double  cutoff,
    int32_t sampnorm);

/*
 * Compute the square wave sample at time t for a given frequency.
 * 
 * The frequency is measured by SAMPLES, *NOT* by seconds.  To get a
 * 440Hz frequency at a sampling rate of 48,000Hz, for example, one can
 * use an fnum of 440 and an fdenom of 48000.
 * 
 * fnum is the numerator of the frequency.  It must be greater than
 * zero.
 * 
 * fdenom is the denominator of the frequency.  It must be greater than
 * zero.
 * 
 * t is the time offset, in SAMPLES.  It must be zero or greater, and
 * less than or equal to (INT32_MAX - SQWAVE_HLEN_MAX).
 * 
 * If fnum is greater than or equal to half the value of fdenom, the
 * frequency is above the Nyquist limit and this function will simply
 * return zero for all values t.
 * 
 * The module must be initialized with sqwave_init() before using this
 * function.
 * 
 * Parameters:
 * 
 *   fnum - the numerator of the frequency per samples
 * 
 *   fdenom - the denominator of the frequency per samples
 * 
 *   t - the time offset in samples
 * 
 * Return:
 * 
 *   the computed sample of the square wave
 */
int16_t sqwave_get(int32_t fnum, int32_t fdenom, int32_t t);

#endif
