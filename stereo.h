#ifndef STEREO_H_INCLUDED
#define STEREO_H_INCLUDED

/*
 * stereo.h
 * 
 * The stereo module of the Retro synthesizer.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The STEREO_POS structure.
 * 
 * Clients should not directly access any of the internal fields.
 * Instead, use the functions provided by this module.
 */
typedef struct {
  
  /* @@TODO: */
  int dummy;
  
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
 *   psp - the stereo position
 * 
 *   pss - pointer to the result structure
 */
void stereo_image(int16_t s, const STEREO_POS *psp, STEREO_SAMP *pss);

/* @@TODO: */
void stereo_setField(
    STEREO_POS * psp,
    int32_t      low_pos,
    int32_t      low_pitch,
    int32_t      high_pos,
    int32_t      high_pitch);
void stereo_setPos(STEREO_POS *psp, int32_t pos);

#endif
