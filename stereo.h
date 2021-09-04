#ifndef STEREO_H_INCLUDED
#define STEREO_H_INCLUDED

/*
 * stereo.h
 * 
 * The stereo module of the Retro synthesizer.
 */

#include "retrodef.h"
#include "sqwave.h"

/*
 * The STEREO_POS structure.
 * 
 * Clients should not directly access any of the internal fields.
 * Instead, use the functions provided by this module.
 */
typedef struct {
  
  /*
   * For stereo fields, this is the stereo position associated with
   * low_pitch.
   * 
   * For constant stereo positions, this is the stereo position.
   * 
   * The value is in range [-MAX_FRAC, MAX_FRAC].  The minimum value
   * means full left, the maximum value means full right, and the zero
   * value means full center.
   */
  int16_t low_pos;
  
  /*
   * For stereo fields, this is the lowest pitch of the field.
   * 
   * For constant stereo positions, low_pitch equals high_pitch.
   * 
   * The range is [SQWAVE_PITCH_MIN, SQWAVE_PITCH_MAX].  For stereo
   * fields, low_pitch must be less than high_pitch.  For constant
   * stereo positions, low_pitch must equal high_pitch.
   */
  int16_t low_pitch;
  
  /*
   * For stereo fields, this is the stereo position associated with
   * high_pitch.
   * 
   * For constant stereo positions, this field is ignored.
   * 
   * The value is in range [-MAX_FRAC, MAX_FRAC].  The minimum value
   * means full left, the maximum value means full right, and the zero
   * value means full center.
   */
  int16_t high_pos;
  
  /*
   * For stereo fields, this is the highest pitch of the field.
   * 
   * For constant stereo positions, high_pitch equals low_pitch.
   * 
   * The range is [SQWAVE_PITCH_MIN, SQWAVE_PITCH_MAX].  For stereo
   * fields, low_pitch must be less than high_pitch.  For constant
   * stereo positions, low_pitch must equal high_pitch.
   */
  int16_t high_pitch;
  
} STEREO_POS;

/*
 * The STEREO_SAMP structure.
 */
typedef struct {
  
  /*
   * The left channel value.
   */
  int16_t left;
  
  /*
   * The right channel value.
   */
  int16_t right;
  
} STEREO_SAMP;

/*
 * Set the stereo module into single-channel mode.
 * 
 * If this function has already been called, further calls have no
 * effect.
 * 
 * After this function has been called, stereo_image() will always
 * ignore stereo positions and just duplicate the input to both stereo
 * output channels.  This allows for single-channel output.
 */
void stereo_flatten(void);

/*
 * Compute the stereo image of a sample using a given stereo position.
 * 
 * s is the input sample.
 * 
 * pitch is the pitch being performed, used for computing position
 * within stereo fields.  It must be in range
 * [SQWAVE_PITCH_MIN, SQWAVE_PITCH_MAX].
 * 
 * psp is the properly initialized stereo position structure.
 * 
 * pss points to the structure to receive the stereo-imaged result.
 * 
 * If stereo_flatten() has been called, then the stereo position will be
 * ignored and the input sample s will just be duplicated to both the
 * left and right channels.
 * 
 * Parameters:
 * 
 *   s - the input sample
 * 
 *   pitch - the pitch
 * 
 *   psp - the stereo position
 * 
 *   pss - pointer to the result structure
 */
void stereo_image(
          int16_t s,
          int32_t pitch,
    const STEREO_POS  * psp,
          STEREO_SAMP * pss);

/*
 * Initialize a stereo position structure to represent a field.
 * 
 * psp is the stereo position structure to initialize.
 * 
 * low_pitch and high_pitch are the boundary pitches of the field.  Both
 * must be in range [SQWAVE_PITCH_MIN, SQWAVE_PITCH_MAX].  Furthermore,
 * low_pitch must be less than high_pitch.
 * 
 * low_pos and high_pos are the stereo positions associated with
 * low_pitch and high_pitch, respectively.  If low_pos and high_pos are
 * equal, then this call is equivalent to stereo_setPos() with pos set
 * to that constant value.  Both positions must be in the range
 * [-MAX_FRAC, MAX_FRAC], with the minimum value meaning full left, the
 * maximum value meaning full right, and the zero value meaning full
 * center.
 * 
 * Parameters:
 * 
 *   psp - the stereo position structure to initialize
 * 
 *   low_pos - the stereo position of the lower pitch
 * 
 *   low_pitch - the low pitch of the field
 * 
 *   high_pos - the stereo position of the higher pitch
 * 
 *   high_pitch - the high pitch of the field
 */
void stereo_setField(
    STEREO_POS * psp,
    int32_t      low_pos,
    int32_t      low_pitch,
    int32_t      high_pos,
    int32_t      high_pitch);

/* 
 * Initialize a stereo position structure to represent a constant stereo
 * position.
 * 
 * psp is the stereo position structure to initialize.
 * 
 * pos is the stereo position.  It must be in the range
 * [-MAX_FRAC, MAX_FRAC], with the minimum value meaning full left, the
 * maximum value meaning full right, and the zero value meaning full
 * center.
 * 
 * Parameters:
 * 
 *   psp - the stereo position structure to initialize
 * 
 *   pos - the constant stereo position
 */
void stereo_setPos(STEREO_POS *psp, int32_t pos);

#endif
