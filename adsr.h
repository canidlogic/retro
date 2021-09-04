#ifndef ADSR_H_INCLUDED
#define ADSR_H_INCLUDED

/*
 * adsr.h
 * 
 * The ADSR (Attack-Decay-Sustain-Release) module of the Retro synth.
 * 
 * Compilation
 * ===========
 * 
 * May require the math library to be included (-lm).
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum value for ADSR time durations.
 */
#define ADSR_MAXTIME (INT32_C(100000000))

/*
 * The maximum integer intensity value in an envelope.
 */
#define ADSR_MAXVAL (16384)

/*
 * Structure prototype for ADSR_OBJ.
 */
struct ADSR_OBJ_TAG;
typedef struct ADSR_OBJ_TAG ADSR_OBJ;

/*
 * Create an ADSR envelope object.
 * 
 * The returned object must eventually be freed with adsr_free().
 * 
 * i_max and i_min are the maximum and minimum intensity multipliers for
 * the envelope.  Both must be finite values in range [0.0, 1.0].
 * Additionally, i_max must be greater than or equal to i_min, and i_max
 * may not be 0.0.
 * 
 * When a specific intensity is requested for the ADSR envelope, it will
 * first be mapped to the range defined by i_max and i_min, and then it
 * will be multiplied against the input sample value to transform the
 * intensity.  If i_max and i_min are equal, then the input sample will
 * always be transformed by a constant multiplier, regardless of what
 * the given intensity for a particular note is.
 * 
 * The t_attack, t_decay, and t_release parameters are the times, in
 * milliseconds, of the attack, decay, and release periods.  They must
 * be finite and greater than or equal to zero.  If when they are
 * transformed into a sample count this value would exceed ADSR_MAXTIME,
 * the duration is shortened to ADSR_MAXTIME.
 * 
 * The envelope (excluding the sustain period) is cached in memory with
 * two bytes per sample, so be cautious of specifying long attack,
 * decay, or release times.
 * 
 * sustain is the sustain level multiplier.  It must be finite and in
 * range [0.0, 1.0].  If it is 1.0, then t_decay is ignored and the
 * decay is set to zero.  If it is 0.0, then t_release is ignored and
 * the release is set to zero.
 * 
 * rate is the sampling rate, in Hz.  It must be either 44100 or 48000.
 * 
 * Parameters:
 * 
 *   i_max - the maximum intensity multiplier
 * 
 *   i_min - the minimum intensity multiplier
 * 
 *   t_attack - the attack duration, in milliseconds
 * 
 *   t_decay - the decay duration, in milliseconds
 * 
 *   sustain - the sustain level multiplier
 * 
 *   t_release - the release duration, in milliseconds
 * 
 *   rate - the sampling rate, in Hz
 */
ADSR_OBJ *adsr_alloc(
    double       i_max,
    double       i_min,
    double       t_attack,
    double       t_decay,
    double       sustain,
    double       t_release,
    int32_t      rate);

/*
 * Free a given ADSR envelope object.
 * 
 * pa points to the object to free.  If it is NULL, the call is ignored.
 * 
 * Parameters:
 * 
 *   pa - the ADSR envelope object to free, or NULL
 */
void adsr_free(ADSR_OBJ *pa);

/*
 * Given an event duration in samples, get the ADSR envelope length in
 * samples.
 * 
 * The given duration must be greater than zero.  The return value will
 * also be greater than zero.
 * 
 * The return value might be less than, equal to, or greater than dur,
 * depending on the particular envelope.
 * 
 * Parameters:
 * 
 *   pa - the ADSR object
 * 
 *   dur - the duration of the event in samples
 * 
 * Return:
 * 
 *   the duration of the envelope in samples for the event
 */
int32_t adsr_length(ADSR_OBJ *pa, int32_t dur);

/*
 * Transform a given sample according to an ADSR envelope.
 * 
 * t is the time offset in samples from the beginning of the envelope.
 * It must be zero or greater.
 * 
 * dur is the duration of the event in samples.  This is not necessarily
 * the same as the duration of the envelope.  It must be greater than
 * zero.
 * 
 * The ADSR envelope is defined for t in [0, (length - 1)], where length
 * is the return value of adsr_length() for dur.  Any t value greater
 * than this range will result in a sample value of zero.
 * 
 * intensity is the intensity of the event at time offset t.  It must be
 * in range [0, ADSR_MAXVAL], where zero maps to the minimum intensity
 * multiplier for this envelope and ADSR_MAXVAL maps to the maximum
 * intensity multiplier for this envelope.
 * 
 * s is the input sample to transform by the ADSR envelope.  The return
 * value is the transformed sample.
 * 
 * Parameters:
 * 
 *   pa - the ADSR object
 * 
 *   t - the time offset in samples from the start of the envelope
 * 
 *   dur - the duration of the event in samples
 * 
 *   intensity - the intensity at time t
 * 
 *   s - the sample to transform
 * 
 * Return:
 * 
 *   the transformed sample
 */
int16_t adsr_mul(
    ADSR_OBJ * pa,
    int32_t    t,
    int32_t    dur,
    int32_t    intensity,
    int16_t    s);

#endif
