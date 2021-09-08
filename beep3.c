/*
 * beep3.c
 * =======
 * 
 * Generate a sound using the Retro synthesizer FM synthesis.
 * 
 * Syntax
 * ------
 * 
 *   beep3 [path] [freq] [msec] [rate] [amp] [genmap]
 * 
 * [path] is the path to the output WAV file to write.  If it already
 * exists, it will be overwritten.
 * 
 * [freq] is the frequency of the note.  This must be in range
 * [1, 20000].
 * 
 * [msec] is the number of milliseconds of duration of the note to
 * write.  This must be in range 1-30000.
 * 
 * [rate] is the sampling rate to use.  This must either be 44100 or
 * 48000.
 * 
 * [amp] is the target level to normalize result samples to.  This must
 * be in range [16, 32000].
 * 
 * [genmap] is the generator map script file to interpret
 * 
 * All numeric values are given as signed integers, with a "-" sign used
 * in front of negative values.  "+" may optionally precede positive
 * values.
 * 
 * Compilation
 * -----------
 * 
 * Compile with the following Retro modules:
 * 
 *   - adsr
 *   - generator
 *   - genmap
 *   - sbuf
 *   - wavwrite
 * 
 * Compile with libshastina 0.9.2 beta or compatible.
 * 
 * The math library may need to be included with -lm
 */

#include "retrodef.h"

#include "generator.h"
#include "genmap.h"
#include "sbuf.h"
#include "wavwrite.h"

#include "shastina.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Local data
 * ==========
 */

/*
 * The name of the module executing, for error reports.
 * 
 * Set at the start of main().
 */
static const char *pModule = NULL;

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static int soundbeep(
    const char    * pPath,
          int32_t   freq,
          int32_t   msec,
          int32_t   rate,
          int32_t   amp,
    const char    * pScript);
static int parseInt(const char *pstr, int32_t *pv);

/*
 * Perform the program action -- see the top of the file for a further
 * description.
 * 
 * Errors are reported to stderr.
 * 
 * Parameters:
 * 
 *   pPath - the output WAV path
 * 
 *   freq - the frequency
 * 
 *   msec - the number of milliseconds
 * 
 *   rate - the sampling rate
 * 
 *   amp - the amplitude
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int soundbeep(
    const char    * pPath,
          int32_t   freq,
          int32_t   msec,
          int32_t   rate,
          int32_t   amp,
    const char    * pScript) {
  
  int status = 1;
  int wavflags = 0;
  int32_t x = 0;
  int32_t s = 0;
  int32_t edur = 0;
  double gv = 0.0;
  
  FILE *fScript = NULL;
  SNSOURCE *psScript = NULL;
  GENERATOR_OPDATA *pInstance = NULL;
  
  GENMAP_RESULT gmr;

  /* Initialize structures */
  memset(&gmr, 0, sizeof(GENMAP_RESULT));
  gmr.pRoot = NULL;
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  if ((freq < 1) || (freq > 20000)) {
    abort();
  }
  if ((msec < 1) || (msec > 30000)) {
    abort();
  }
  if ((rate != RATE_DVD) && (rate != RATE_CD)) {
    abort();
  }
  if ((amp < 16) || (amp > 32000)) {
    abort();
  }
  if (pScript == NULL) {
    abort();
  }

  /* Open script file */
  if (status) {
    fScript = fopen(pScript, "rb");
    if (fScript == NULL) {
      status = 0;
      fprintf(stderr, "%s: Can't open script file!\n", pModule);
    }
  }

  /* Open multipass Shastina source around script file and transfer
   * ownership of file handle so that file handle will be closed when
   * source is closed */
  if (status) {
    psScript = snsource_stream(
                fScript, SNSTREAM_OWNER | SNSTREAM_RANDOM);
    fScript = NULL;
  }

  /* Interpret script */
  if (status) {
    genmap_run(psScript, &gmr, rate);
    if (gmr.errcode != GENMAP_OK) {
      status = 0;
      fprintf(stderr, "%s: Script interpretation failed!\n", pModule);
      fprintf(stderr, "Reason: %s\n", genmap_errstr(gmr.errcode));
      if (gmr.linenum > 0) {
        fprintf(stderr, "Line  : %ld\n", gmr.linenum);
      }
    }
  }

  /* We can now close the Shastina source */
  if (status) {
    snsource_free(psScript);
    psScript = NULL;
  }

  /* Allocate instance data and initialize */
  if (status) {
    pInstance = (GENERATOR_OPDATA *) calloc(
                                        (size_t) gmr.icount,
                                        sizeof(GENERATOR_OPDATA));
    if (pInstance == NULL) {
      abort();
    }
    for(x = 0; x < gmr.icount; x++) {
      generator_opdata_init(
        &(pInstance[x]), freq, (msec * rate) / 1000);
    }
  }

  /* Figure out total length of sound in samples */
  if (status) {
    edur = generator_length(gmr.pRoot, pInstance, gmr.icount);
  }

  /* Set WAV initialization flags */
  if (status) {
    if (rate == RATE_DVD) {
      wavflags = WAVWRITE_INIT_48000 | WAVWRITE_INIT_MONO;
    
    } else if (rate == RATE_CD) {
      wavflags = WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO;
    
    } else {
      /* Unrecognized rate */
      abort();
    }
  }
  
  /* Initialize WAV writer */
  if (status) {
    if (!wavwrite_init(pPath, wavflags)) {
      status = 0;
      fprintf(stderr, "%s: Can't open output file!\n", pModule);
    }
  }
  
  /* Initialize sample buffer */
  if (status) {
    sbuf_init();
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < rate; x++) {
      sbuf_sample(0, 0);
    }
  }

  /* Write the generated sound */
  if (status) {
    for(x = 0; x < edur; x++) {
      /* Get the generated sample value */
      gv = generator_invoke(gmr.pRoot, pInstance, gmr.icount, x);
      
      /* Clamp to range and get integer sample value */
      if (!isfinite(gv)) {
        gv = 0.0;
      }
      
      if (!(gv <= INT32_MAX)) {
        s = INT32_MAX;
      
      } else if (!(gv >= INT32_MIN)) {
        s = INT32_MIN;
        
      } else {
        s = (int32_t) floor(gv);
      }
      
      /* Write the sample */
      sbuf_sample(s, s);
    }
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < rate; x++) {
      sbuf_sample(0, 0);
    }
  }
  
  /* Stream normalized samples to wave file */
  if (status) {
    sbuf_stream(amp);
  }
  
  /* Close down */
  if (status) {
    wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  } else {
    wavwrite_close(WAVWRITE_CLOSE_RMFILE);
  }
  
  /* Free objects */
  if (pInstance != NULL) {
    free(pInstance);
    pInstance = NULL;
  }
  
  generator_release(gmr.pRoot);
  gmr.pRoot = NULL;
  
  snsource_free(psScript);
  psScript = NULL;
  
  /* Close files */
  if (fScript != NULL) {
    fclose(fScript);
    fScript = NULL;
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
  
  int status = 1;
  int x = 0;
  
  int32_t freq = 0;
  int32_t msec = 0;
  int32_t rate = 0;
  int32_t amp = 0;
  
  /* Get module name */
  pModule = NULL;
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "beep3";
  }
  
  /* Verify six parameters in addition to module name */
  if (argc != 7) {
    status = 0;
    fprintf(stderr, "%s: Expecting six parameters!\n", pModule);
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
  
  /* Parse numeric parameters */
  if (status) {
    if (!parseInt(argv[2], &freq)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse freq parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[3], &msec)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse msec parameter!\n", pModule);
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
    if ((freq < 1) || (freq > 20000)) {
      status = 0;
      fprintf(stderr, "%s: freq parameter out of range!\n", pModule);
    }
  }
  
  if (status) {
    if ((msec < 1) || (msec > 30000)) {
      status = 0;
      fprintf(stderr, "%s: msec parameter out of range!\n", pModule);
    }
  }
  
  if (status) {
    if ((rate != RATE_DVD) && (rate != RATE_CD)) {
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
    if (!soundbeep(argv[1], freq, msec, rate, amp, argv[6])) {
      status = 0;
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
