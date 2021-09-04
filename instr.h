#ifndef INSTR_H_INCLUDED
#define INSTR_H_INCLUDED

/*
 * instr.h
 * 
 * Instrument module of the Retro synthesizer.
 * 
 * Compilation
 * ===========
 * 
 * May require the math library to be included (-lm).
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum time allowed for attack, decay, and release values, given
 * in 1/300 of a second units.
 */
#define INSTR_MAXTIME (INT32_C(1000000))

/*
 * Structure prototype for INSTR_OBJ.
 */
struct INSTR_OBJ_TAG;
typedef struct INSTR_OBJ_TAG INSTR_OBJ;

/*
 * Create a new instrument object.
 * 
 * The returned instrument object should eventually be freed with
 * instr_free().
 * 
 * min_stereo_pitch and max_stereo_pitch define the stereo pitch range.
 * These parameters may have any numeric value, and the only restriction
 * is that max_stereo_pitch must be greater than or equal to
 * min_stereo_pitch.
 * 
 * If the min_stereo_pitch and max_stereo_pitch are equal, then the
 * instrument will always have the same stereo location regardless of
 * what the pitch is.  If the two parameters are not equal, then all
 * pitches less than min_stereo_pitch will have the stereo location of
 * min_stereo_pitch, all pitches greater than max_stereo_pitch will have
 * the stereo location of max_stereo_pitch, and all pitches in the range
 * [min_stereo_pitch, max_stereo_pitch] will be linearly interpolated
 * between the stereo locations of min_stereo_pitch and
 * max_stereo_pitch.
 * 
 * Varying the stereo location depending on pitch is especially helpful
 * for synthesizing keyboard-like instruments, where each pitch has its
 * own stereo location.
 * 
 * main_stereo_pos and aux_stereo_pos are stereo locations.  These must
 * be finite, in range [-1.0, 1.0].  A stereo position of -1.0 means
 * purely left channel, a stereo position of 1.0 means purely right
 * channel, a stereo position of 0.0 means equal in both channels, and
 * all other values are interpolated between those.
 * 
 * If the min_stereo_pitch and max_stereo_pitch are equal, then the
 * stereo location of the instrument is always main_stereo_pos, and
 * aux_stereo_pos is ignored.  Otherwise, main_stereo_pos is associated
 * with min_stereo_pitch and aux_stereo_pos is associated with
 * max_stereo_pitch.
 * 
 * attack, decay, and release are times specified in units of 1/300 of
 * a second.  Each of these parameters must be in range zero up to and
 * including INSTR_MAXTIME.
 * 
 * sustain is the sustain level of the sound.  It must be a finite value
 * in range [0.0, 1.0].  A value of 1.0 means that the sustain level is
 * equal to the full intensity at the top of the attack.  A value of 0.0
 * means that the sound ends after the decay.  Values in between specify
 * the sustain level as a fraction of the full attack level.
 * 
 * The attack, decay, sustain, and release parameters specify an ADSR
 * envelope that is used to vary the intensity over time.
 * 
 * ampl_min and ampl_max must both be in range [0.0, 1.0], and ampl_min
 * must be less than or equal to ampl_max.
 * 
 * rate is the sampling rate, in Hz.  It must be either 44100 or 48000.
 * 
 * Parameters:
 * 
 *   min_stereo_pitch - the minimum pitch of the stereo pitch range
 * 
 *   max_stereo_pitch - the maximum pitch of the stereo pitch range
 * 
 *   main_stereo_pos - the main stereo position
 * 
 *   aux_stereo_pos - the auxiliary stereo position
 * 
 *   attack - the attack length, in 1/300 second units
 * 
 *   decay - the decay length, in 1/300 second units
 * 
 *   sustain - the sustain level as a fraction of the attack level
 * 
 *   release - the release length, in 1/300 second units
 * 
 *   ampl_min - the minimum amplitude for this instrument
 * 
 *   ampl_max - the maximum amplitude for this instrument
 * 
 *   rate - the sampling rate
 * 
 * Return:
 * 
 *   a new instrument object
*/
INSTR_OBJ *instr_new(
    int32_t min_stereo_pitch,
    int32_t max_stereo_pitch,
    double  main_stereo_pos,
    double  aux_stereo_pos,
    int32_t attack,
    int32_t decay,
    double  sustain,
    int32_t release,
    double  ampl_min,
    double  ampl_max,
    int32_t rate);

/*
 * Free a given instrument object.
 * 
 * If NULL is passed, the call is ignored.
 * 
 * Parameters:
 * 
 *   pi - the instrument object to free, or NULL
 */
void instr_free(INSTR_OBJ *pi);

/*
 * Given a duration in samples, return an adjusted duration in samples
 * that is appropriate for the instrument.
 * 
 * scount must be greater than zero.
 * 
 * If the computed duration would exceed the range of a signed 32-bit
 * value, the maximum signed 32-bit value is returned.
 * 
 * Parameters:
 * 
 *   pi - the instrument object
 * 
 *   scount - the unadjusted duration in samples
 * 
 * Return:
 * 
 *   the duration in samples adjusted for the particular instrument
 */
int32_t instr_dur(INSTR_OBJ *pi, int32_t scount);

void instr_render(
    INSTR_OBJ * pi,
    int32_t     pitch,
    int32_t     t,
    int16_t     s,
    @@TODO:

#endif
