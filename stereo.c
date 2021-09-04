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

/* @@TODO: */
#include <stdio.h>
void stereo_setField(
    STEREO_POS * psp,
    int32_t      low_pos,
    int32_t      low_pitch,
    int32_t      high_pos,
    int32_t      high_pitch) {
  printf("stereo_setField %d %d %d %d\n", (int) low_pos,
    (int) low_pitch, (int) high_pos, (int) high_pitch);
}
void stereo_setPos(STEREO_POS *psp, int32_t pos) {
  printf("stereo_setPos %d\n", (int) pos);
}
