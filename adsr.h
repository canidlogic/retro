#ifndef ADSR_H_INCLUDED
#define ADSR_H_INCLUDED

/*
 * adsr.h
 * ======
 * 
 * The ADSR (Attack-Decay-Sustain-Release) module of the Retro synth.
 * 
 * Compilation
 * -----------
 * 
 * May require the math library to be included (-lm).
 */

#include <stddef.h>
#include <stdint.h>

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
 * The t_attack, t_decay, and t_release parameters are the number of
 * control units that the attack, decay, and release phases last.  Each
 * must be zero or greater.
 * 
 * The t_limit is the number of control units it takes the sustain phase
 * to fade to silence if it were never released, or zero if there is no
 * fading during the sustain phase.  t_limit must be zero or greater.
 * 
 * The peak is the intensity multiplier of the peak during the attack,
 * relative to the beginning of the sustain.  It may have any finite
 * value that is greater than zero.
 * 
 * Parameters:
 * 
 *   t_attack - the attack duration, in control units
 * 
 *   t_decay - the decay duration, in control units
 * 
 *   t_release - the release duration, in control units
 * 
 *   t_limit - the sustain fade limit, in control units, or zero for
 *   no fade
 * 
 *   peak - the intensity multiplier of the peak during the attack
 */
ADSR_OBJ *adsr_alloc(
    int32_t t_attack,
    int32_t t_decay,
    int32_t t_release,
    int32_t t_limit,
    double  peak);

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
 * Given an event duration in control units, get the ADSR envelope
 * length in control units.
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
 *   dur - the duration of the event in control units
 * 
 * Return:
 * 
 *   the duration of the envelope in control units for the event
 */
int32_t adsr_length(ADSR_OBJ *pa, int32_t dur);

/*
 * Compute the ADSR envelope multiplier for a given t and duration.
 * 
 * pa is the ADSR envelope.
 * 
 * t is the offset from the start of the envelope in control units.  It
 * must be zero or greater.
 * 
 * dur is the duration of the event in control units.  It must be at
 * least one.  The duration of the event is not necessarily the same as
 * the duration of the envelope, so t may well be greater than dur.
 * 
 * The return value is a computed multiplier value.
 * 
 * Parameters:
 * 
 *   pa - the ADSR envelope
 * 
 *   t - the t offset in control units
 * 
 *   dur - the duration in control units
 * 
 * Return:
 * 
 *   the ADSR multiplier
 */
double adsr_compute(ADSR_OBJ *pa, int32_t t, int32_t dur);

#endif
