/*
 * cmajor.c
 * ========
 * 
 * Perform a C major scale up and down with a specified instrument and
 * a crescendo the full length.
 * 
 * Syntax
 * ------
 * 
 *   cmajor [path] [rate] [amp] [i_min] [attack] [decay]
 *          [sustain] [release] [pos]
 * 
 *   cmajor [path] [rate] [amp] [i_min] [attack] [decay]
 *          [sustain] [release] [pos_1] [pos_2]
 * 
 * [path] is the path to the output WAV file to write.  If it already
 * exists, it will be overwritten.
 * 
 * [rate] is the sampling rate to use.  This must be an integer that is
 * either 44100 or 48000.
 * 
 * [amp] is the amplitude of the square waves.  This must be an integer
 * in range [16, 32000].
 * 
 * [i_min] is the minimum intensity as a fraction of the amplitude.  The
 * range is [0.0, 1.0].  The maximum intensity as a fraction of the
 * amplitude is always 1.0 for this program.
 * 
 * [attack] is the duration of the attack, in milliseconds.  It must be
 * finite and 0.0 or greater.
 * 
 * [decay] is the duration of the decay, in milliseconds.  It must be
 * finite and 0.0 or greater.
 * 
 * [sustain] is the sustain level as a fraction of the full intensity.
 * It must be in range [0.0, 1.0].
 * 
 * [release] is the duration of the release, in milliseconds.  It must
 * be finite and 0.0 or greater.  Note that long releases add to the
 * duration of each note, so avoid huge values here.
 * 
 * [pos] / [pos_1] [pos_2] give the stereo position.  In the first
 * invocation syntax, only one stereo position is given, which is used
 * for all the notes.  In the second invocation syntax, two stereo
 * positions are given, with the first one associated with the bottom
 * pitch of the scale and the second one associated with the top pitch
 * of the scale.  Positions must be in range [-1.0, 1.0], with -1.0
 * meaning full left, 1.0 meaning full right, 0.0 meaning center, and
 * everything else interpolated.
 * 
 * Compilation
 * -----------
 * 
 * @@TODO:  sqwave, wavwrite, adsr
 * 
 * May require the math library to be included with -lm
 */

#include "adsr.h"
#include "sqwave.h"
#include "wavwrite.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Static data
 * ===========
 */

/*
 * The pitches of the C-Major scale in ascending order.
 */
static int32_t CMAJOR[8] = {
  0, 2, 4, 5, 7, 9, 11, 12
};

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static int cmajor(
    const char    * pPath,
          int32_t   rate,
          int32_t   amp,
          double    i_min,
          double    attack,
          double    decay,
          double    sustain,
          double    release,
          double    pos_1,
          double    pos_2);

static int parseInt(const char *pstr, int32_t *pv);

/*
 * Perform the program action -- see the top of the file for a further
 * description.
 * 
 * If only one stereo position should be used for everything, set pos_1
 * and pos_2 equal to each other.
 * 
 * Parameters:
 * 
 *   pPath - the output WAV path
 * 
 *   rate - the sampling rate
 * 
 *   amp - the amplitude
 * 
 *   i_min - the minimum intensity of the instrument
 * 
 *   attack - the duration of the attack
 * 
 *   decay - the duration of the decay
 * 
 *   sustain - the sustain level
 * 
 *   release - the duration of the release
 * 
 *   pos_1 - the stereo position of the lowest pitch
 * 
 *   pos_2 - the stereo position of the highest pitch
 * 
 * Return:
 * 
 *   non-zero if successful, zero if couldn't open output file
 */
static int cmajor(
    const char    * pPath,
          int32_t   rate,
          int32_t   amp,
          double    i_min,
          double    attack,
          double    decay,
          double    sustain,
          double    release,
          double    pos_1,
          double    pos_2) {
  
  int32_t p = 0;
  int32_t i = 0;
  int32_t plen = 0;
  int32_t plenx = 0;
  int16_t s = 0;
  double intensity = 0.0;
  double progress_mul = 0.0;
  double progress_add = 0.0;
  int status = 1;
  int wavflags = 0;
  int32_t sqrate = 0;
  ADSR_OBJ *pa = NULL;
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  if ((rate != 48000) && (rate != 44100)) {
    abort();
  }
  if ((amp < 16) || (amp > 32000)) {
    abort();
  }
  if ((!isfinite(i_min)) || (!isfinite(attack)) ||
      (!isfinite(decay)) || (!isfinite(sustain)) ||
      (!isfinite(release)) || (!isfinite(pos_1)) ||
      (!isfinite(pos_2))) {
    abort();
  }
  if (!((i_min >= 0.0) && (i_min <= 1.0))) {
    abort();
  }
  if (!((sustain >= 0.0) && (sustain <= 1.0))) {
    abort();
  }
  if ((!(attack >= 0.0)) || (!(decay >= 0.0)) ||
      (!(release >= 0.0))) {
    abort();
  }
  if (!((pos_1 >= -1.0) && (pos_1 <= 1.0))) {
    abort();
  }
  if (!((pos_2 >= -1.0) && (pos_2 <= 1.0))) {
    abort();
  }
  
  /* Set WAV initialization flags and sqwave rate */
  if (rate == 48000) {
    wavflags = WAVWRITE_INIT_48000 | WAVWRITE_INIT_MONO;
    sqrate = SQWAVE_RATE_DVD;
  
  } else if (rate == 44100) {
    wavflags = WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO;
    sqrate = SQWAVE_RATE_CD;
  
  } else {
    /* Unrecognized rate */
    abort();
  }
  
  /* Initialize square wave module */
  sqwave_init((double) amp, sqrate);
  
  /* Initialize WAV writer */
  if (!wavwrite_init(pPath, wavflags)) {
    status = 0;
  }
  
  /* Create ADSR envelope */
  pa = adsr_alloc(1.0, i_min, attack, decay, sustain, release, rate);
  
  /* Write the scale */
  if (status) {
    
    /* Write one second of silence beforehand */
    for(i = 0; i < rate; i++) {
      wavwrite_sample(0, 0);
    }
    
    /* Compute the length in samples of each pitch */
    plen = rate / 2;
    
    /* Compute length of envelope */
    plenx = adsr_length(pa, plen);
    
    /* Attempt to abridge pitch length to account for additional
     * envelope space, if necessary */
    if (plenx > plen) {
      plen = plen - (plenx - plen);
      if (plen < 1) {
        plen = 1;
      }
      plenx = adsr_length(pa, plen);
    }
    
    /* Write the ascending scale */
    for(p = 0; p < 8; p++) {
      
      /* Compute the progress multiplier and additive */
      progress_mul = (1.0 / (8.0 * ((double) plenx))) * 0.5;
      progress_add = (((double) p) / 8.0) * 0.5;
      
      /* Write the current pitch */
      for(i = 0; i < plenx; i++) {
        
        /* Get the current sample from square wave */
        s = sqwave_get(CMAJOR[p], i);
        
        /* Transform by ADSR envelope */
        intensity = progress_mul * ((double) i) + progress_add;
        s = adsr_mul(
                pa,
                i,
                plen,
                (int32_t) (intensity * ((double) ADSR_MAXVAL)),
                s);
        
        /* Write the sample */
        wavwrite_sample(s, s);
      }
    }
    
    /* Write the descending scale */
    for(p = 7; p >= 0; p--) {
      
      /* Compute the progress multiplier and additive */
      progress_mul = (1.0 / (8.0 * ((double) plenx))) * 0.5;
      progress_add = ((((double) (7 - p)) / 8.0) * 0.5) + 0.5;
      
      /* Write the current pitch */
      for(i = 0; i < plenx; i++) {
        
        /* Get the current sample from square wave */
        s = sqwave_get(CMAJOR[p], i);
        
        /* Transform by ADSR envelope */
        intensity = progress_mul * ((double) i) + progress_add;
        s = adsr_mul(
                pa,
                i,
                plen,
                (int32_t) (intensity * ((double) ADSR_MAXVAL)),
                s);
        
        /* Write the sample */
        wavwrite_sample(s, s);
      }
    }
    
    /* Write one second of silence afterwards */
    for(i = 0; i < rate; i++) {
      wavwrite_sample(0, 0);
    }
  }
  
  /* Close down */
  if (status) {
    wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  } else {
    wavwrite_close(WAVWRITE_CLOSE_RMFILE);
  }
  
  /* Release ADSR envelope */
  adsr_free(pa);
  pa = NULL;
  
  /* Return status */
  return status;
}

/*
 * Parse the given string as a signed integer.
 * 
 * pstr is the string to parse.
 * 
 * pv points to the integer value to use to return the parsed numeric
 * value if the function is successful.
 * 
 * In two's complement, this function will not successfully parse the
 * least negative value.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 *   pv - pointer to the return numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int parseInt(const char *pstr, int32_t *pv) {
  
  int negflag = 0;
  int32_t result = 0;
  int status = 1;
  int32_t d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* If first character is a sign character, set negflag appropriately
   * and skip it */
  if (*pstr == '+') {
    negflag = 0;
    pstr++;
  } else if (*pstr == '-') {
    negflag = 1;
    pstr++;
  } else {
    negflag = 0;
  }
  
  /* Make sure we have at least one digit */
  if (*pstr == 0) {
    status = 0;
  }
  
  /* Parse all digits */
  if (status) {
    for( ; *pstr != 0; pstr++) {
    
      /* Make sure in range of digits */
      if ((*pstr < '0') || (*pstr > '9')) {
        status = 0;
      }
    
      /* Get numeric value of digit */
      if (status) {
        d = (int32_t) (*pstr - '0');
      }
      
      /* Multiply result by 10, watching for overflow */
      if (status) {
        if (result <= INT32_MAX / 10) {
          result = result * 10;
        } else {
          status = 0; /* overflow */
        }
      }
      
      /* Add in digit value, watching for overflow */
      if (status) {
        if (result <= INT32_MAX - d) {
          result = result + d;
        } else {
          status = 0; /* overflow */
        }
      }
    
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Invert result if negative mode */
  if (status && negflag) {
    result = -(result);
  }
  
  /* Write result if successful */
  if (status) {
    *pv = result;
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
  
  int32_t rate = 0;
  int32_t amp = 0;
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "cmajor";
  }
  
  /* @@TODO: Parse the other parameters and pass them through */
  
  /* Verify three parameters in addition to module name */
  if (argc != 4) {
    status = 0;
    fprintf(stderr, "%s: Expecting three parameters!\n", pModule);
  }
  
  /* Check parameters are present */
  if (status) {
    for(x = 1; x < argc; x++) {
      if (argv[x] == NULL) {
        abort();
      }
    }
  }
  
  /* Parse numeric parameters */
  if (status) {
    if (!parseInt(argv[2], &rate)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse rate parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[3], &amp)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse amplitude parameter!\n",
          pModule);
    }
  }
  
  /* Range check numeric parameters */
  if (status) {
    if ((rate != 48000) && (rate != 44100)) {
      status = 0;
      fprintf(stderr, "%s: Rate parameter invalid!\n", pModule);
    }
  }
  
  if (status) {
    if ((amp < 16) || (amp > 32000)) {
      status = 0;
      fprintf(stderr, "%s: Amplitude parameter out of range!\n",
        pModule);
    }
  }
  
  /* Call through */
  if (status) {
    if (!cmajor(argv[1], rate, amp,
                  0.25, 5.0, 5.0, 0.9, 5.0, 0.0, 0.0)) {
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
