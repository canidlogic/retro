/*
 * stereo.c
 * 
 * Implementation of stereo.h
 * 
 * See the header for further information.
 */

#include "stereo.h"
#include <stdlib.h>
#include <string.h>

/*
 * Static data
 * ===========
 */

/*
 * The flatten flag.
 * 
 * If non-zero, it means that single-channel flattening has been
 * requested with stereo_flatten().
 */
static int m_stereo_flat = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void stereo_compute(int16_t s, int32_t pos, STEREO_SAMP *pss);

/*
 * Compute a stereo sample given an input sample and a stereo position.
 * 
 * s is the input sample.
 * 
 * pos is the position.  It must be in range [-MAX_FRAC, MAX_FRAC],
 * where the minimum value is full left, the maximum value is full
 * right, and zero value is full center.
 * 
 * pss is the stereo sample structure to compute.
 * 
 * This function ignores m_stereo_flat.
 * 
 * Parameters:
 * 
 *   s - the input sample
 * 
 *   pos - the stereo position
 * 
 *   pss - the stereo sample to compute
 */
static void stereo_compute2(int16_t s, int32_t pos, STEREO_SAMP *pss) {
  
  /* Check parameters */
  if ((pos < -MAX_FRAC) || (pos > MAX_FRAC) || (pss == NULL)) {
    abort();
  }
  
  /* Clear output structure */
  memset(pss, 0, sizeof(STEREO_SAMP));
  
  /* Handle different position cases */
  if (pos < 0) {
    /* Left position -- convert pos to an intensity fraction for the
     * right channel (remembering that pos is negative) */
    pos = MAX_FRAC + pos;
    
    /* Compute right channel */
    pss->right = (int16_t) ((pos * ((int32_t) s)) / MAX_FRAC);
    
    /* Left channel is full */
    pss->left = s;
    
  } else if (pos > 0) {
    /* Right position -- convert pos to an intensity fraction for the
     * left channel */
    pos = MAX_FRAC - pos;
    
    /* Compute left channel */
    pss->left = (int16_t) ((pos * ((int32_t) s)) / MAX_FRAC);
    
    /* Right channel is full */
    pss->right = s;
    
  } else if (pos == 0) {
    /* Full center position -- just duplicate sample */
    pss->left = s;
    pss->right = s;
    
  } else {
    /* Shouldn't happen */
    abort();
  }
}

static void stereo_compute(int16_t s, int32_t pos, STEREO_SAMP *pss) {
  
  int32_t mul_l = 0;
  int32_t mul_r = 0;
  
  /* Check parameters */
  if ((pos < -MAX_FRAC) || (pos > MAX_FRAC) || (pss == NULL)) {
    abort();
  }
  
  /* Clear output structure */
  memset(pss, 0, sizeof(STEREO_SAMP));
  
  /* Handle different position cases */
  if (pos < 0) {
    /* Left position -- convert pos to an intensity fraction for the
     * right channel (remembering that pos is negative) */
    mul_r = (MAX_FRAC + pos) / 2;
    mul_l = MAX_FRAC - mul_l;
    
    /* Compute right channel */
    
    
    /* Left channel is full */
    
    
  } else if (pos > 0) {
    /* Right position -- convert pos to an intensity fraction for the
     * left channel */
    mul_l = (MAX_FRAC - pos) / 2;
    mul_r = MAX_FRAC - mul_l;
    
    /* Compute left channel */
    
    
    /* Right channel is full */
    
    
  } else if (pos == 0) {
    /* Full center position -- just duplicate sample */
    mul_l = (MAX_FRAC / 2);
    mul_r = MAX_FRAC - mul_l;
    
  } else {
    /* Shouldn't happen */
    abort();
  }
  
  pss->left = (int16_t) ((mul_l * ((int32_t) s)) / MAX_FRAC);
  pss->right = (int16_t) ((mul_r * ((int32_t) s)) / MAX_FRAC);
}

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
  m_stereo_flat = 1;
}

/*
 * stereo_image function.
 */
void stereo_image(
          int16_t s,
          int32_t pitch,
    const STEREO_POS  * psp,
          STEREO_SAMP * pss) {
  
  int32_t pos = 0;
  
  /* Check parameters */
  if ((pss == NULL) || (psp == NULL) ||
      (pitch < SQWAVE_PITCH_MIN) || (pitch > SQWAVE_PITCH_MAX)) {
    abort();
  }
  
  /* Check whether in flat mode */
  if (m_stereo_flat) {
    
    /* Flat mode, so just duplicate sample on both channels regardless
     * of the position */
    pss->left = s;
    pss->right = s;
    
  } else {
    /* Not in flat mode, so we need to compute the stereo image; check
     * whether the stereo position is constant or a field */
    if (psp->low_pitch == psp->high_pitch) {
      /* Constant stereo position */
      stereo_compute(s, psp->low_pos, pss);
      
    } else if (psp->low_pitch < psp->high_pitch) {
      /* Stereo field -- compute position at current pitch */
      if (pitch <= psp->low_pitch) {
        /* Pitch at low end of field, so position is low position */
        pos = psp->low_pos;
        
      } else if (pitch >= psp->high_pitch) {
        /* Pitch at high end of field, so position is high position */
        pos = psp->high_pos;
        
      } else {
        /* Pitch in middle of field, so we need to interpolate */
        pos = ((((int32_t) (pitch - psp->low_pitch)) *
                ((int32_t) (psp->high_pos - psp->low_pos))) /
              ((int32_t) (psp->high_pitch - psp->low_pitch))) +
                  ((int32_t) psp->low_pos);
      }
    
      /* Compute at the proper position */
      stereo_compute(s, pos, pss);
    
    } else {
      /* Invalid structure */
      abort();
    }
  }
}

/*
 * stereo_setField function.
 */
void stereo_setField(
    STEREO_POS * psp,
    int32_t      low_pos,
    int32_t      low_pitch,
    int32_t      high_pos,
    int32_t      high_pitch) {
  
  /* Check parameters */
  if ((psp == NULL) ||
      (low_pos < -MAX_FRAC) || (low_pos > MAX_FRAC) ||
      (high_pos < -MAX_FRAC) || (high_pos > MAX_FRAC) ||
      (low_pitch < SQWAVE_PITCH_MIN) ||
      (low_pitch > SQWAVE_PITCH_MAX) ||
      (high_pitch < SQWAVE_PITCH_MIN) ||
      (high_pitch > SQWAVE_PITCH_MAX) ||
      (high_pitch <= low_pitch)) {
    abort();
  }
  
  /* Check for special case of low_pos equals high_pos */
  if (low_pos == high_pos) {
    /* The two stereo positions are equal, so just call through to the
     * set constant function */
    stereo_setPos(psp, low_pos);
    
  } else {
    /* The two stereo positions are not equal, so set up the full
     * field */
    memset(psp, 0, sizeof(STEREO_POS));
    psp->low_pos = (int16_t) low_pos;
    psp->low_pitch = (int16_t) low_pitch;
    psp->high_pos = (int16_t) high_pos;
    psp->high_pitch = (int16_t) high_pitch;
  }
}

/*
 * stereo_setPos function.
 */
void stereo_setPos(STEREO_POS *psp, int32_t pos) {
  
  /* Check parameters */
  if ((psp == NULL) || (pos < -MAX_FRAC) || (pos > MAX_FRAC)) {
    abort();
  }
  
  /* Clear structure */
  memset(psp, 0, sizeof(STEREO_POS));
  
  /* Set structure */
  psp->low_pitch = 0;
  psp->high_pitch = 0;
  psp->high_pos = 0;
  
  psp->low_pos = pos;
}
