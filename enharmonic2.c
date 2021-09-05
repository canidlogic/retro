/*
 * enharmonic2.c
 * ============
 * 
 * Test program for enharmonic percussion.
 * 
 * Syntax
 * ------
 * 
 *   enharmonic [path]
 * 
 * [path] is the path to the output WAV file to write.  If it already
 * exists, it will be overwritten.
 * 
 * Compilation
 * -----------
 * 
 * Compile with the adsr and wavwrite modules.
 * 
 * May require the math library to be included with -lm
 */

#include "adsr.h"
#include "wavwrite.h"
#include "retrodef.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void sineFill(int16_t *pbuf, int32_t count, double f, double a);
static void waveFill(int16_t *pbuf, int32_t count);
static void normBuf(int16_t *pbuf, int32_t count);
static int enharmonic(const char *pPath);

/*
 * Add a sine wave into the given buffer.
 * 
 * Assumes that sampling rate is RATE_DVD.  Output clamped at +/-32000
 * when converted to integer.
 * 
 * Parameters:
 * 
 *   pbuf - pointer to the buffer to add the sine wave into
 * 
 *   count - the number of samples in the buffer
 * 
 *   f - the frequency of the sine wave in Hz
 * 
 *   a - the amplitude of the since wave
 */
static void sineFill(int16_t *pbuf, int32_t count, double f, double a) {
  
  int32_t i = 0;
  int32_t sv = 0;
  int32_t nv = 0;
  double p = 0.0;
  double fv = 0.0;
  
  /* Check parameters */
  if ((pbuf == NULL) || (count < 1)) {
    abort();
  }
  if ((!isfinite(f)) || (!isfinite(a))) {
    abort();
  }
  if (!(f > 0.0)) {
    abort();
  }
  if (!(a > 0.0)) {
    abort();
  }
  
  /* Compute the period in 48kHz samples */
  p = ((double) RATE_DVD) / f;
  if (!isfinite(p)) {
    p = 1.0;
  }
  
  /* Go through each sample and compute it */
  for(i = 0; i < count; i++) {
    
    /* Floating-point value is sine wave stretched out so that 2*PI is
     * at the period length */
    fv = sin(((double) i) * 2.0 * M_PI / p);
    
    /* Multiply floating-point value by amplitude */
    fv = fv * a;
    
    /* If floating-point value not finite, set to zero */
    if (!isfinite(fv)) {
      fv = 0.0;
    }
    
    /* Clamp floating-point value */
    if (fv > 32000.0) {
      fv = 32000.0;
    } else if (fv < -32000.0) {
      fv = -32000.0;
    }
    
    /* Get the integer value */
    nv = (int32_t) floor(fv);
    
    /* Get current sample value */
    sv = (int32_t) pbuf[i];
    
    /* Add new sample value to current */
    sv = sv + nv;
    
    /* Clamp to range */
    if (sv < -32000) {
      sv = -32000;
    } else if (sv > 32000) {
      sv = 32000;
    }
    
    /* Write sample back into buffer */
    pbuf[i] = (int16_t) sv;
  }
}

/*
 * Write the enharmonic sound without envelope into the given buffer.
 * 
 * Parameters:
 * 
 *   pbuf - pointer to the buffer to fill with samples
 * 
 *   count - the number of samples to compute
 */
static void waveFill(int16_t *pbuf, int32_t count) {
  
  double base_f = 261.6256;
  double f = 0.0;
  double spread = 0.5;
  int32_t layers = 7;
  unsigned int seedval = 5812347;
  int32_t x = 0;
  
  /* Check parameters */
  if ((pbuf == NULL) || (count < 1)) {
    abort();
  }
  
  /* Clear buffer */
  memset(pbuf, 0, sizeof(int16_t) * ((size_t) count));

  /* Set the random seed */
  srand(seedval);
  
  /* Add the sine layers */
  for(x = 0; x < layers; x++) {
    
    /* Get a random float value in [0.0, 2.0] */
    f = (((double) rand()) / ((double) RAND_MAX)) * 2.0;
    
    /* Adjust to random value in [-1.0, 1.0] */
    f = f - 1.0;
    
    /* Apply the spread */
    f = f * spread;
    
    /* Get random multiplier centered around 1.0 */
    f = 1.0 + f;
    
    /* Compute the frequency for this sine wave */
    f = f * base_f;
    
    /* Write the sine wave and some harmonics */
    sineFill(pbuf, count, f, 500.0);
    sineFill(pbuf, count, f * 1.75, 250.0);
    sineFill(pbuf, count, f * 2.6, 125.0);
    sineFill(pbuf, count, f * 3.24, 75.0);
    sineFill(pbuf, count, f * 4.87, 50.0);
    sineFill(pbuf, count, f * 5.39, 25.0);
  }
}

/*
 * Normalize the samples in the buffer for maximum 30000.
 * 
 * Parameters:
 * 
 *   pbuf - pointer to the buffer to normalize
 * 
 *   count - the number of samples to normalize
 */
static void normBuf(int16_t *pbuf, int32_t count) {
  
  int32_t s_max = 0;
  int32_t s_min = 0;
  int32_t x = 0;
  int32_t s = 0;
  double sv = 0.0;
  
  /* Check parameters */
  if ((pbuf == NULL) || (count < 1)) {
    abort();
  }
  
  /* Determine minimum and maximum samples */
  for(x = 0; x < count; x++) {
    if (pbuf[x] > s_max) {
      s_max = pbuf[x];
    }
    if (pbuf[x] < s_min) {
      s_min = pbuf[x];
    }
  }

  /* Get absolute value of minimum */
  if (s_min < 0) {
    s_min = -(s_min);
  }
  
  /* Get maximum of maximum sample and absolute value of minimum */
  if (s_max < s_min) {
    s_max = s_min;
  }
  
  /* Make sure it is at least one */
  if (s_max < 1) {
    s_max = 1;
  }
  
  /* Determine scaling value */
  sv = 30000.0 / ((double) s_max);

  /* Now scale everything */
  for(x = 0; x < count; x++) {
    /* Get scaled sample */
    s = (int32_t) floor(((double) pbuf[x]) * sv);
    
    /* Clamp */
    if (s < -30000) {
      s = -30000;
    }
    if (s > 30000) {
      s = 30000;
    }
    
    /* Write normalized sample */
    pbuf[x] = (int16_t) s;
  }
}

/*
 * Perform the program action -- see the top of the file for a further
 * description.
 * 
 * Parameters:
 * 
 *   pPath - the output WAV path
 * 
 * Return:
 * 
 *   non-zero if successful, zero if couldn't open output file
 */
static int enharmonic(const char *pPath) {
  
  int status = 1;
  int wavflags = 0;
  ADSR_OBJ *adsr = NULL;
  int32_t x = 0;
  int32_t dur = 0;
  int32_t edur = 0;
  int16_t *pbuf = NULL;
  int s = 0;
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  
  /* Set WAV initialization flags */
  wavflags = WAVWRITE_INIT_48000 | WAVWRITE_INIT_MONO;
  
  /* Initialize WAV writer */
  if (!wavwrite_init(pPath, wavflags)) {
    status = 0;
  }
  
  /* Allocate ADSR envelope for the sound */
  if (status) {
    adsr = adsr_alloc(
            25.0,      /* Attack duration in milliseconds */
            0.0,       /* Decay duration in milliseconds */
            1.0,       /* Sustain level multiplier */
            5000.0,    /* Release duration in milliseconds */
            RATE_DVD);
  }
  
  /* Set the duration of the event in samples */
  if (status) {
    dur = 12000;
  }
  
  /* Compute the envelope duration in samples */
  if (status) {
    edur = adsr_length(adsr, dur);
  }
  
  /* Allocate a buffer with the duration of the envelope */
  if (status) {
    pbuf = (int16_t *) calloc((size_t) edur, sizeof(int16_t));
    if (pbuf == NULL) {
      abort();
    }
  }
  
  /* Compute the sound without envelope */
  if (status) {
    waveFill(pbuf, edur);
  }
  
  /* Apply the envelope */
  if (status) {
    for(x = 0; x < edur; x++) {
      pbuf[x] = adsr_mul(adsr, x, dur, pbuf[x]);
    }
  }
  
  /* Normalize the buffer */
  if (status) {
    normBuf(pbuf, edur);
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < RATE_DVD; x++) {
      wavwrite_sample(0, 0);
    }
  }
  
  /* Write the enharmonic sound */
  if (status) {
    for(x = 0; x < edur; x++) {
      /* Get the current sample */
      s = (int) pbuf[x];
      
      /* Write the sample */
      wavwrite_sample(s, s);
    }
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < RATE_DVD; x++) {
      wavwrite_sample(0, 0);
    }
  }
  
  /* Free ADSR envelope if allocated */
  adsr_release(adsr);
  adsr = NULL;
  
  /* Free sample buffer if allocated */
  if (pbuf != NULL) {
    free(pbuf);
    pbuf = NULL;
  }
  
  /* Close down */
  if (status) {
    wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  } else {
    wavwrite_close(WAVWRITE_CLOSE_RMFILE);
  }
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  const char *pModule = NULL;
  int status = 1;
  int x = 0;
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "enharmonic";
  }
  
  /* Verify one parameters in addition to module name */
  if (argc != 2) {
    status = 0;
    fprintf(stderr, "%s: Expecting one parameters!\n", pModule);
  }
  
  /* Check parameters are present */
  if (status) {
    if (argv == NULL) {
      abort();
    }
    for(x = 1; x < argc; x++) {
      if (argv[x] == NULL) {
        abort();
      }
    }
  }
  
  /* Call through */
  if (status) {
    if (!enharmonic(argv[1])) {
      status = 0;
      fprintf(stderr, "%s: Couldn't open output file!\n", pModule);
    }
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
