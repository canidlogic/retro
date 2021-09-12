#ifndef INSTR_H_INCLUDED
#define INSTR_H_INCLUDED

/*
 * instr.h
 * 
 * Instrument module of Retro synthesizer.
 * 
 * May need to compile with the math library -lm
 */

#include "adsr.h"
#include "retrodef.h"
#include "generator.h"
#include "stereo.h"
#include "sqwave.h"
#include "ttone.h"

/*
 * The maximum number of instruments that may be defined.
 * 
 * Space for instruments is statically allocated, so be cautious about
 * setting this too high.  This must not exceed 65536.
 */
#define INSTR_MAXCOUNT (4096)

/*
 * Clear the instrument register i.
 * 
 * i must be in range [0, INSTR_MAXCOUNT - 1].  If the given instrument
 * register is already clear, this operation has no effect.  Otherwise,
 * the instrument in register i is cleared to undefined.
 * 
 * At the start, all instrument registers are cleared.
 * 
 * Parameters:
 * 
 *   i - the instrument register to clear
 */
void instr_clear(int32_t i);

/*
 * Define a square wave instrument in register i.
 * 
 * The name of this function is a misnomer for reasons of backwards
 * compatibility.  It is only able to define square wave instruments.
 * 
 * i must be in range [0, INSTR_MAXCOUNT - 1].  instr_clear() is run
 * automatically on the indicated register before the new instrument is
 * defined.
 * 
 * i_max and i_min define the minimum and maximum intensity range of the
 * instrument.  Both must be in range [0, MAX_FRAC].  Also, i_min must
 * be less than or equal to i_max.  If i_min equals i_max, then the
 * instrument will have constant intensity.  If both values are zero,
 * then this call is equivalent to instr_clear().
 * 
 * pa is the ADSR envelope to use for the square wave instrument.  A
 * reference will be added to the ADSR envelope function, and the
 * reference will be released when the instrument register is cleared.
 * 
 * psp is the stereo position to use for the instrument.  It must be a
 * properly initialized structure.  The information is copied into the
 * instrument register.
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   i_max - the maximum intensity of the instrument
 * 
 *   i_min - the minimum intensity of the instrument
 * 
 *   pa - the ADSR envelope of the instrument
 * 
 *   psp - the stereo position of the instrument
 */
void instr_define(
          int32_t      i,
          int32_t      i_max,
          int32_t      i_min,
          ADSR_OBJ   * pa,
    const STEREO_POS * psp);

/*
 * Copy one instrument register to another.
 * 
 * i_target and i_src are the target and source registers, respectively.
 * Both must be in range [0, INSTR_MAXCOUNT - 1].  If both are equal,
 * the call is ignored.  If the source register is cleared, then
 * operation is equivalent to performing instr_clear() on the target
 * register.
 * 
 * If the source register is not cleared and the target is a different
 * register, the target register is first cleared, and then all source
 * register parameters are copied over.  Copied object references have
 * their reference count increased.
 * 
 * Parameters:
 * 
 *   i_target - the target register
 * 
 *   i_src - the source register
 */
void instr_dup(int32_t i_target, int32_t i_src);

/*
 * Set the maximum and minimum intensities of an instrument register.
 * 
 * i is the instrument register to apply the changes to.  It must be in
 * range [0, INSTR_MAXCOUNT - 1].  If the given register is cleared, the
 * call is ignored.
 * 
 * i_max and i_min define the minimum and maximum intensity range of the
 * instrument.  Both must be in range [0, MAX_FRAC].  Also, i_min must
 * be less than or equal to i_max.  If i_min equals i_max, then the
 * instrument will have constant intensity.  If both values are zero,
 * then this call is equivalent to instr_clear().
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   i_max - the maximum intensity for the instrument
 * 
 *   i_min - the minimum intensity for the instrument
 */
void instr_setMaxMin(int32_t i, int32_t i_max, int32_t i_min);

/*
 * Set the stereo position of an instrument register.
 * 
 * i is the instrument register to apply the changes to.  It must be in
 * range [0, INSTR_MAXCOUNT - 1].  If the given register is cleared, the
 * call is ignored.
 * 
 * psp is the new stereo position for the instrument.  It must be a
 * properly initialized structure.  The information is copied into the
 * instrument register.
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   psp - the new stereo position
 */
void instr_setStereo(int32_t i, const STEREO_POS *psp);

/*
 * Prepare instance data for a specific instrument.
 * 
 * Instance data is not required for all types of instruments.  NULL is
 * returned if the given instrument does not require instance data.
 * 
 * If the instrument does require instance data, then the return value
 * is a dynamically allocated block of instance data that needs to be
 * passed through to instr_get() and instr_length().  Undefined behavior
 * occurs if the instance data block is used with a different
 * instrument, or the instrument changes, or the duration and pitch in
 * the instr_get() call or the duration in the instr_length() call do
 * not match those that were prepared with this function.
 * 
 * If a non-NULL pointer is returned, then the memory block is
 * dynamically allocated and must eventually be freed with free() after
 * the note has been fully rendered.
 * 
 * dur is the duration of the event in samples.  This is not necessarily
 * the same as the duration of the envelope from instr_length().  It
 * must be greater than zero.
 * 
 * pitch is the pitch to generate.  It must be in the range
 * [PITCH_MIN, PITCH_MAX].
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   dur - the duration of the event, in samples
 * 
 *   pitch - the pitch index in semitones from middle C
 * 
 * Return:
 * 
 *   a dynamically allocated instance data block for rendering this
 *   note, or NULL if no instance data is required for this instrument
 */
void *instr_prepare(int32_t i, int32_t dur, int32_t pitch);

/*
 * Given an event duration in samples, return the envelope duration in
 * samples.
 * 
 * i is the instrument register to use.  It must be in range
 * [0, INSTR_MAXCOUNT - 1].  If the given register is cleared, this call
 * always returns a value of one.
 * 
 * dur is the event duration in samples.  It must be greater than zero.
 * 
 * pod is a pointer to instance data that has been generated with a call
 * to instr_prepare() for this instrument and for the given duration.
 * 
 * The return value will always be greater than zero.  It may be less
 * than, equal to, or greater than the given dur value, depending on the
 * envelope specified for the instrument.
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   dur - the event duration in samples
 * 
 *   pod - pointer to instance data
 * 
 * Return:
 * 
 *   the envelope duration in samples
 */
int32_t instr_length(int32_t i, int32_t dur, void *pod);

/*
 * Compute an instrument sample.
 * 
 * The square wave module must be initialized before calling this
 * function.
 * 
 * i is the instrument register to use.  It must be in range
 * [0, INSTR_MAXCOUNT - 1].  If the given register is cleared, this call
 * always computes zero samples.
 * 
 * t is the time offset in samples from the start of the event.  The
 * envelope is defined for [0, length - 1], where length is the return
 * from instr_length() for this instrument and the provided dur value.
 * Any value above this range will result in a return of zero samples.
 * t must be zero or greater.
 * 
 * dur is the duration of the event in samples.  This is not necessarily
 * the same as the duration of the envelope from instr_length().  It
 * must be greater than zero.
 * 
 * pitch is the pitch to generate.  It must be in the range
 * [PITCH_MIN, PITCH_MAX].
 * 
 * amp is the amplitude of the sound at time offset t.  It must be in
 * range [0, MAX_FRAC].
 * 
 * pss is the pointer to the structure to receive the computed stereo
 * sample.
 * 
 * pod is a pointer to instance data that has been generated with a call
 * to instr_prepare() for this instrument and for the given pitch and
 * duration.
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   t - the time offset from the start of the event, in samples
 * 
 *   dur - the duration of the event, in samples
 * 
 *   pitch - the pitch index in semitones from middle C
 * 
 *   amp - the amplitude at time t
 * 
 *   pss - the structure to receive the result
 * 
 *   pod - pointer to instance data
 */
void instr_get(
    int32_t       i,
    int32_t       t,
    int32_t       dur,
    int32_t       pitch,
    int16_t       amp,
    STEREO_SAMP * pss,
    void        * pod);

#endif
