#ifndef TTONE_H_INCLUDED
#define TTONE_H_INCLUDED

/*
 * ttone.h
 * =======
 * 
 * Retro module storing mappings of pitch numbers to the frequencies of
 * the equal-tempered 12-tone scale.
 */

#include "retrodef.h"

/*
 * The minimum and maximum pitch numbers.
 * 
 * This represents the full 88-key keyboard, with middle C at pitch
 * zero.
 */
#define PITCH_MIN (-39)
#define PITCH_MAX (48)

/*
 * Get the frequency in Hz of a specific pitch.
 * 
 * pitch is the pitch number, in range [PITCH_MIN, PITCH_MAX].  A fault
 * occurs if the pitch number is out of range.
 * 
 * Parameters:
 * 
 *   pitch - the pitch number
 * 
 * Return:
 * 
 *   the frequency in Hz
 */
double pitchfreq(int32_t pitch);

#endif
