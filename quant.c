/*
 * quant.c
 * =======
 * 
 * Implementation of quant.h
 * 
 * See the header for further information.
 */

#include "quant.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * The number of samples in the lookup table for loudness.
 */
#define LOUD_LUT_LEN (1058)

/*
 * The number of samples in the lookup table for panning.
 */
#define PAN_LUT_LEN (435)

/*
 * Local data
 * ==========
 */

/*
 * Flag that is set to one once all the local data has been initialized.
 */
static int q_init = 0;

/*
 * Lookup table for loudness.
 * 
 * Each entry is an offset from 1.0.
 */
static float loud_lut[LOUD_LUT_LEN];

/*
 * Pitch lookup tables.
 * 
 * First is an index table that is used only during initialization,
 * which stores the number of semitones away from A for each pitch
 * starting with C.
 * 
 * Second is a small lookup table for the frequencies of all twelve
 * tones of the octave [C4, B4].
 * 
 * Third is a lookup table for all 1/5-cent units.  Values are offset
 * from 1.0.
 */
static int pitch_index[12] = {3, 4, 5, 6, -5, -4, -3, -2, -1, 0, 1, 2};
static double pitch_lut[12];
static float qcent_lut[500];

/*
 * Stereo pan lookup table.
 */
static float pan_lut[PAN_LUT_LEN];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static double leftCalc(int32_t p);

/*
 * Perform the panning calculation for just the left channel.
 * 
 * Parameters:
 * 
 *   p - the quantized stereo location
 * 
 * Return:
 * 
 *   the left channel amplitude scaling value
 */
static double leftCalc(int32_t p) {
  
  int d = 0;
  int r = 0;
  double result = 0.0;
  
  /* Check state and parameter */
  if ((!q_init) || (p < QUANT_MIN) || (p > QUANT_MAX)) {
    abort();
  }
  
  /* Increase p to unsigned range */
  p += 32767;
  
  /* Divide and remainder */
  d = ((int) p) / 151;
  r = ((int) p) % 151;
  
  /* Compute result */
  if (r == 0) {
    /* Remainder is zero, so return exact lookup table entry */
    result = (double) pan_lut[d];
    
  } else {
    /* Linear interpolation */
    result = (double)
      ((double) pan_lut[d])
        + (((((double) pan_lut[d + 1]) - ((double) pan_lut[d]))
            * ((double) r)) / 151.0);
  }
  
  /* Return result */
  return result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * quant_init function.
 */
void quant_init(int32_t center) {
  
  int i = 0;
  int v = 0;
  
  /* Check state */
  if (q_init) {
    abort();
  }
  
  /* Check parameter */
  if ((center < QUANT_MIN) || (center > QUANT_MAX)) {
    abort();
  }
  
  /* Set initialization flag here already since we will use the loudness
   * lookup already during initialization function */
  q_init = 1;
  
  /* Fill in the loudness lookup table */
  memset(loud_lut, 0, sizeof(float) * LOUD_LUT_LEN);
  loud_lut[0] = 0.0f;
  
  for(i = 1; i < LOUD_LUT_LEN; i++) {
    loud_lut[i] =
      (float) (pow(10.0, ((double) (i * 31)) / 7200.0) - 1.0);
  }
  
  /* Fill in the stereo pan lookup table */
  memset(pan_lut, 0, sizeof(float) * PAN_LUT_LEN);
  pan_lut[0] = 1.0f;
  pan_lut[PAN_LUT_LEN - 1] = 0.0f;
  
  for(i = 1; i < PAN_LUT_LEN - 1; i++) {
    pan_lut[i] =
      (float) (cos(((double) i) * M_PI / 868.0)
                * (1.0
                    + ((quant_loud(center) - 1.0)
                        * sin(((double) i) * M_PI / 434.0))));
  }
  
  /* Fill in the pitch lookup table using the pitch index array */
  memset(pitch_lut, 0, sizeof(double) * 12);
  for(i = 0; i < 12; i++) {
    /* Get index value */
    v = pitch_index[i];
    
    /* Compute pitch based on index range */
    if (v == 0) {
      /* A4 is fixed at 440.0 Hz */
      pitch_lut[i] = 440.0;
      
    } else if ((v == 1) || (v == 2)) {
      /* Bb4 and B4 are a number of semitones above A4 given by the
       * index value */
      pitch_lut[i] = 440.0 * exp(((double) v) * log(2.0) / 12.0);
      
    } else if (v >= 3) {
      /* For index values of 3 or greater we are in the C5 octave, so we
       * use the expression from the previous case but divide the
       * starting point of 440.0 by two to lower the result by an
       * octave */
      pitch_lut[i] = 220.0 * exp(((double) v) * log(2.0) / 12.0);
      
    } else if (v < 0) {
      /* For index values less than zero, the index gives the number of
       * semitones below A4 */
      pitch_lut[i] = 440.0 / exp(((double) (0 - v)) * log(2.0) / 12.0);
      
    } else {
      /* Shouldn't happen */
      abort();
    }
  }
  
  /* Fill in the qcent lookup table */
  memset(qcent_lut, 0, sizeof(float) * 500);
  qcent_lut[0] = 0.0f;
  
  for(i = 1; i < 500; i++) {
    qcent_lut[i] = (float) expm1(((double) i) * log(2.0) / 6000.0);
  }
}

/*
 * quant_pitch function.
 */
double quant_pitch(int32_t p) {
  
  int inv_flag = 0;
  int d = 0;
  int r = 0;
  double f = 0.0;
  
  /* Check state and parameter */
  if ((!q_init) || (p < QUANT_MIN) || (p > QUANT_MAX)) {
    abort();
  }
  
  /* If input negative, set invert flag and take its absolute value */
  if (p < 0) {
    inv_flag = 1;
    p = 0 - p;
  }
  
  /* Integer divide and remainder by 6000 */
  d = ((int) p) / 6000;
  r = ((int) p) % 6000;
  
  /* If invert flag set, adjust divide and remainder */
  if (inv_flag) {
    d = -1 - d;
    r = 6000 - r;
    if (r == 6000) {
      d++;
      r = 0;
    }
  }
  
  /* Compute frequency */
  if (d < 0) {
    f = pitch_lut[r / 500]
          * (((double) qcent_lut[r % 500]) + 1.0)
          / exp2((double) (0 - d));
  } else {
    f = pitch_lut[r / 500]
          * (((double) qcent_lut[r % 500]) + 1.0)
          * exp2((double) d);
  }
  
  /* Return frequency */
  return f;
}

/*
 * quant_loud function.
 */
double quant_loud(int32_t i) {
  
  int inv_flag = 0;
  int b = 0;
  int r = 0;
  double v = 0.0;
  double x = 0.0;
  double y = 0.0;
  
  /* Check state and parameter */
  if ((!q_init) || (i < QUANT_MIN) || (i > QUANT_MAX)) {
    abort();
  }
  
  /* If value is negative, set invert flag and take its absolute
   * value */
  if (i < 0) {
    inv_flag = 1;
    i = 0 - i;
  }
  
  /* Compute main value by interpolation */
  b = ((int) i) / 31;
  r = ((int) i) % 31;
  
  /* Check if we got an exact match */
  if (r == 0) {
    /* Exact match, so get entry directly from lookup table */
    v = ((double) loud_lut[b]) + 1.0;
    
  } else {
    /* Not an exact match, so linear interpolation */
    x = (double) loud_lut[b];
    y = (double) loud_lut[b + 1];
    v = x + (((y - x) * ((double) r)) / 31.0) + 1.0;
  }
  
  /* If invert flag is set, take 1/v */
  if (inv_flag) {
    v = 1.0 / v;
  }
  
  /* Return result */
  return v;
}

/*
 * quant_pan function.
 */
void quant_pan(int32_t p, double *sl, double *sr) {
  
  /* Check parameters */
  if ((p < QUANT_MIN) || (p > QUANT_MAX) ||
      (sl == NULL) || (sr == NULL)) {
    abort();
  }
  
  /* Compute the two values */
  *sl = leftCalc(p);
  *sr = leftCalc(0 - p);
}
