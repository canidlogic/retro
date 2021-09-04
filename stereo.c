/*
 * stereo.c
 * 
 * Implementation of stereo.h
 * 
 * See the header for further information.
 */

#include "stereo.h"
#include <stdlib.h>

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * stereo_flatten function.
 */
void stereo_flatten(void) {
  /* @@TODO: */
}

/*
 * stereo_image function.
 */
void stereo_image(int16_t s, const STEREO_POS *psp, STEREO_SAMP *pss) {
  /* @@TODO: */
  if (pss == NULL) {
    abort();
  }
  pss->left = s;
  pss->right = s;
}
