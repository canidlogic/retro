/*
 * beep2.c
 * =======
 * 
 * Generate a sound file using the Retro synthesizer.
 * 
 * This is similar to the beep program, except it uses the full Retro
 * instrument module, rather than just generating a simple square wave.
 * 
 * Syntax
 * ------
 * 
 *   beep2 [path] [pitch] [sec] [rate] [amp]
 * 
 * [path] is the path to the output WAV file to write.  If it already
 * exists, it will be overwritten.
 * 
 * [pitch] is the pitch of the square wave.  Middle C is pitch zero, and
 * all other pitches are given in semitones away from middle C.  For
 * example, -1 is one semitone below middle C and 2 is two semitones
 * above middle C.  The range is [-39, 48], which is the full range of
 * the 88-key keyboard.
 * 
 * [sec] is the number of seconds to write.  This must be in range 1-60.
 * 
 * [rate] is the sampling rate to use.  This must either be 44100 or
 * 48000.
 * 
 * [amp] is the amplitude of the square waves.  This must be in range
 * [16, 32000].
 * 
 * All numeric values are given as signed integers, with a "-" sign used
 * in front of negative values.  "+" may optionally precede positive
 * values.
 * 
 * Compilation
 * -----------
 * 
 * Compile with the following modules:
 * 
 *   adsr
 *   instr
 *   sqwave
 *   stereo
 *   wavwrite
 * 
 * The math library may need to be included with -lm
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adsr.h"
#include "stereo.h"
#include "instr.h"
#include "wavwrite.h"

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static int soundbeep2(
    const char    * pPath,
          int32_t   pitch,
          int32_t   sec,
          int32_t   rate,
          int32_t   amp);
static int parseInt(const char *pstr, int32_t *pv);

/*
 * Perform the program action -- see the top of the file for a further
 * description.
 * 
 * Parameters:
 * 
 *   pPath - the output WAV path
 * 
 *   pitch - the pitch
 * 
 *   sec - the number of seconds
 * 
 *   rate - the sampling rate
 * 
 *   amp - the amplitude
 * 
 * Return:
 * 
 *   non-zero if successful, zero if couldn't open output file
 */
static int soundbeep2(
    const char    * pPath,
          int32_t   pitch,
          int32_t   sec,
          int32_t   rate,
          int32_t   amp) {
  
  int32_t scount = 0;
  int32_t sfull = 0;
  int32_t dur = 0;
  int status = 1;
  int wavflags = 0;
  int32_t sqrate = 0;
  ADSR_OBJ *pa = NULL;
  STEREO_POS sp;
  STEREO_SAMP ssa;
  
  /* Initialize structures */
  memset(&sp, 0, sizeof(STEREO_POS));
  memset(&ssa, 0, sizeof(STEREO_SAMP));
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  if ((pitch < SQWAVE_PITCH_MIN) || (pitch > SQWAVE_PITCH_MAX)) {
    abort();
  }
  if ((sec < 1) || (sec > 60)) {
    abort();
  }
  if ((rate != 48000) && (rate != 44100)) {
    abort();
  }
  if ((amp < 16) || (amp > 32000)) {
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
  
  /* Initialize instrument */
  pa = adsr_alloc(10.0, 10.0, 0.5, 10.0, rate);
  stereo_setPos(&sp, 0);
  stereo_flatten();
  instr_define(0, INSTR_MAXINTENSITY, 0, pa, &sp);
  
  /* Initialize WAV writer */
  if (!wavwrite_init(pPath, wavflags)) {
    status = 0;
  }
  
  /* Write one second of silence */
  if (status) {
    for(scount = 0; scount < rate; scount++) {
      wavwrite_sample(0, 0);
    }
  }
  
  /* Write a tone */
  if (status) {
    dur = rate * sec;
    sfull = instr_length(0, dur);
    for(scount = 0; scount < sfull; scount++) {
      instr_get(0, scount, dur, pitch, GRAPH_MAXVAL, &ssa);
      wavwrite_sample(ssa.left, ssa.right);
    }
  }
  
  /* Write one second of silence */
  if (status) {
    for(scount = 0; scount < rate; scount++) {
      wavwrite_sample(0, 0);
    }
  }
  
  /* Free ADSR object if allocated */
  adsr_release(pa);
  pa = NULL;
  
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
  
  int32_t pitch = 0;
  int32_t sec = 0;
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
    pModule = "beep2";
  }
  
  /* Verify five parameters in addition to module name */
  if (argc != 6) {
    status = 0;
    fprintf(stderr, "%s: Expecting five parameters!\n", pModule);
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
    if (!parseInt(argv[2], &pitch)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse pitch parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[3], &sec)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse seconds parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[4], &rate)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse rate parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[5], &amp)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse amplitude parameter!\n",
          pModule);
    }
  }
  
  /* Range check numeric parameters */
  if (status) {
    if ((pitch < SQWAVE_PITCH_MIN) || (pitch > SQWAVE_PITCH_MAX)) {
      status = 0;
      fprintf(stderr, "%s: Pitch parameter out of range!\n", pModule);
    }
  }
  
  if (status) {
    if ((sec < 1) || (sec > 60)) {
      status = 0;
      fprintf(stderr, "%s: Seconds parameter out of range!\n", pModule);
    }
  }
  
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
    if (!soundbeep2(argv[1], pitch, sec, rate, amp)) {
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