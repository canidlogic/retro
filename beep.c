/*
 * beep.c
 * 
 * Include the math library.
 */

#include "wavwrite.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Add a sine wave to the given sample array.
 * 
 * ps points to the sample array.  samp_count is the number of samples
 * in the array, which must be greater than one.  There is only one
 * channel in the sample array.
 * 
 * wave_count is the number of full sine wave periods to include in the
 * samples.  It must be greater than zero.
 * 
 * amp is what to multiply each normalized sine wave by to get the
 * integer value.  It must be finite.
 * 
 * The generated samples are added to the samples that are already 
 * the array.
 * 
 * Parameters:
 * 
 *   ps - pointer to the sample array
 * 
 *   samp_count - the number of samples in the sample array
 * 
 *   wave_count - the number of wave periods to write
 * 
 *   amp - the amplitude of the sine wave
 */
static void sinewave(
    int16_t * ps,
    int32_t   samp_count,
    int32_t   wave_count,
    double    amp) {
  
  int32_t x = 0;
  int32_t i = 0;
  double sv = 0.0;
  double mult = 0.0;
  
  /* Check parameters */
  if (ps == NULL) {
    abort();
  }
  if ((samp_count < 2) || (wave_count < 1)) {
    abort();
  }
  if (!isfinite(amp)) {
    abort();
  }
  
  /* Precompute the multiplier */
  mult = (2 * M_PI * ((double) wave_count)) /
            ((double) (samp_count - 1));
  
  /* Generate the sine wave */
  for(x = 0; x < samp_count; x++) {
    
    /* Get the sine wave value */
    sv = sin(((double) x) * mult);
    
    /* Quantize */
    i = (int32_t) (sv * amp);
    
    /* Add in existing sample */
    i = i + ((int32_t) ps[x]);
    
    /* Clamp to range */
    if (i > INT16_MAX) {
      i = INT16_MAX;
    } else if (i < -(INT16_MAX)) {
      i = -(INT16_MAX);
    }
    
    /* Update sample */
    ps[x] = (int16_t) i;
  }
}

/*
 * Add a square wave to the given sample array.
 * 
 * This is a wrapper around sinewave.  See that function for a
 * description of the ps, samp_count, wave_count, and amp parameters.
 * 
 * The square wave is constructed as multiple calls to sinewave, each
 * adding a different harmonic of the square wave.  Square waves only
 * include odd-numbered harmonics.
 * 
 * harmonics indicates how many harmonics to add.  It must be greater
 * than zero.  If it is one, the result is the same as a sine wave.
 * Higher numbers get closer to a square wave.
 * 
 * Parameters:
 * 
 *   ps - pointer to the sample array
 * 
 *   samp_count - the number of samples in the sample array
 * 
 *   wave_count - the number of wave periods to write
 * 
 *   amp - the amplitude of the sine wave
 * 
 *   harmonics - the number of harmonics to add
 */
static void squarewave(
    int16_t * ps,
    int32_t   samp_count,
    int32_t   wave_count,
    double    amp,
    int32_t   harmonics) {
  
  int32_t h = 0;
  int32_t m = 0;
  
  /* Check harmonics, wave_count, and amp parameters */
  if ((harmonics < 1) || (wave_count < 1)) {
    abort();
  }
  if (!isfinite(amp)) {
    abort();
  }
  
  /* Add harmonics */
  for(h = 0; h < harmonics; h++) {
    
    /* Get multiplier */
    m = (h * 2) + 1;
    
    /* Call through */
    sinewave(
        ps,
        samp_count,
        wave_count * m,
        (4.0 / (M_PI * ((double) m))) * amp);
  }
}

int main(int argc, char *argv[]) {
  
  int32_t scount = 0;
  int16_t *ps = NULL;
  
  /* Initialize WAV writer */
  if (!wavwrite_init("a440_square.wav",
                      WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO)) {
    abort();
  }
  
  /* Allocate five-second buffer and clear to zero */
  ps = (int16_t *) malloc(44100 * 5 * sizeof(int16_t));
  if (ps == NULL) {
    abort();
  }
  memset(ps, 0, 44100 * 5 * sizeof(int16_t));
  
  /* Write a square wave */
  squarewave(ps, 44100 * 5, 440 * 5, 20000.0, 8);
  
  /* Write buffer to output */
  for(scount = 0; scount < (44100 * 5); scount++) {
    wavwrite_sample(ps[scount], ps[scount]);
  }
  
  /* Close down */
  wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  
  /* Free buffer if allocated */
  if (ps != NULL) {
    free(ps);
    ps = NULL;
  }
  
  /* Return success */
  return 0;
}
