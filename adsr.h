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
 * Structure prototype for ADSR_OBJ.
 */
struct ADSR_OBJ_TAG;
typedef struct ADSR_OBJ_TAG ADSR_OBJ;

/*
 * Create an ADSR envelope object.
 * 
 * The returned object starts out with a reference count of one.  Use
 * adsr_addref() to add another reference, and use adsr_release() to
 * decrease the reference count.  The object is freed when the reference
 * count reaches zero.
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
    double       t_attack,
    double       t_decay,
    double       sustain,
    double       t_release,
    int32_t      rate);

/*
 * Add a reference to the given ADSR envelope object.
 * 
 * pa points to the object to add a reference to.  If it is NULL, the
 * call is ignored.
 * 
 * If this call causes the reference count to overflow, a fault occurs.
 * This only occurs if there are billions of references.
 * 
 * Parameters:
 * 
 *   pa - the ADSR envelope object to add a reference to, or NULL
 */
void adsr_addref(ADSR_OBJ *pa);

/*
 * Release a reference from a given ADSR envelope object.
 * 
 * pa points to the object to release.  If it is NULL, the call is
 * ignored.
 * 
 * If this call causes the reference count to drop to zero, the object
 * is freed.
 * 
 * Parameters:
 * 
 *   pa - the ADSR envelope object to release, or NULL
 */
void adsr_release(ADSR_OBJ *pa);

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
    int16_t    s);

#endif
