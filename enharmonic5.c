/*
 * enharmonic5.c
 * =============
 * 
 * Test program for enharmonic percussion.
 * 
 * Syntax
 * ------
 * 
 *   enharmonic5 [path]
 * 
 * [path] is the path to the output WAV file to write.  If it already
 * exists, it will be overwritten.
 * 
 * Compilation
 * -----------
 * 
 * Compile with the adsr generator sbuf and wavwrite modules.
 * 
 * May require the math library to be included with -lm
 */

#include "adsr.h"
#include "wavwrite.h"
#include "retrodef.h"
#include "sbuf.h"
#include "generator.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * The target sample level to normalize the results to.
 */
#define NORM_MAX (INT32_C(30000))

/*
 * The sampling rate to use.
 * 
 * Must be either RATE_DVD or RATE_CD.
 */
#define SAMP_RATE RATE_DVD

/*
 * The duration of the event in *samples.*
 * 
 * This does not include release time.
 */
#define DUR_SAMPLES (48000)

/*
 * The main ADSR envelope options.
 * 
 * ATTACK, DECAY, and RELEASE are double constants in milliseconds.
 * 
 * SUSTAIN is a double multiplier for the sustain level.
 */
#define FULL_ATTACK   (25.0)
#define FULL_DECAY    (0.0)
#define FULL_SUSTAIN  (1.0)
#define FULL_RELEASE  (250.0)

/*
 * The frequency of the pitch to render in Hz.
 */
#define PITCH_FREQ (440.0)

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static int enharmonic(const char *pPath);

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
  int32_t edur = 0;
  double gv = 0.0;
  int32_t s = 0;
  
  GENERATOR *pRoot = NULL;
  GENERATOR *pMod = NULL;
  GENERATOR_OPDATA opd[2];
  
  /* Initialize structures */
  memset(&opd, 0, sizeof(GENERATOR_OPDATA) * 2);
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  
  /* Set WAV initialization flags */
  if (SAMP_RATE == RATE_DVD) {
    wavflags = WAVWRITE_INIT_48000 | WAVWRITE_INIT_MONO;
  } else if (SAMP_RATE == RATE_CD) {
    wavflags = WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO;
  } else {
    abort();
  }
  
  /* Initialize WAV writer */
  if (!wavwrite_init(pPath, wavflags)) {
    status = 0;
  }
  
  /* Initialize sample buffer */
  if (status) {
    sbuf_init();
  }
  
  /* Allocate ADSR envelope for the sound */
  if (status) {
    adsr = adsr_alloc(
            FULL_ATTACK,    /* Attack duration in milliseconds */
            FULL_DECAY,     /* Decay duration in milliseconds */
            FULL_SUSTAIN,   /* Sustain level multiplier */
            FULL_RELEASE,   /* Release duration in milliseconds */
            SAMP_RATE);
  }
  
  /* Initialize generator graph */
  if (status) {
    pMod = generator_op(
              GENERATOR_F_SINE,   /* function */
              5.9,                /* frequency multiplier */
              0.0,               /* frequency boost */
              0.01,              /* base amplitude */
              adsr,               /* ADSR envelope */
              0.0,                /* FM feedback */
              0.0,                /* AM feedback */
              NULL,               /* FM modulator */
              NULL,               /* AM modulator */
              0.0,                /* FM modulator scale */
              0.0,                /* AM modulator scale */
              0);                 /* instance data offset */
    
    pRoot = generator_op(
              GENERATOR_F_SINE,   /* function */
              1.0,                /* frequency multiplier */
              0.0,                /* frequency boost */
              20000.0,            /* base amplitude */
              adsr,               /* ADSR envelope */
              0.0,                /* FM feedback */
              0.0,                /* AM feedback */
              pMod,               /* FM modulator */
              NULL,               /* AM modulator */
              1.0,                /* FM modulator scale */
              0.0,                /* AM modulator scale */
              1);                 /* instance data offset */
  }
  
  /* Initialize instance data */
  if (status) {
    generator_opdata_init(
              &(opd[0]),
              PITCH_FREQ,
              DUR_SAMPLES,
              SAMP_RATE,
              20000,          /* ny_limit */
              0);             /* hlimit */
    generator_opdata_init(
              &(opd[1]),
              PITCH_FREQ,
              DUR_SAMPLES,
              SAMP_RATE,
              20000,          /* ny_limit */
              0);             /* hlimit */
  }
  
  /* Compute the sound duration in samples */
  if (status) {
    edur = generator_length(pRoot, &(opd[0]), 2);
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < SAMP_RATE; x++) {
      sbuf_sample(0, 0);
    }
  }
  
  /* Write the generated sound */
  if (status) {
    for(x = 0; x < edur; x++) {
      /* Get the generated sample value */
      gv = generator_invoke(pRoot, &(opd[0]), 2, x);
      
      /* Clamp to range and get integer sample value */
      if (!isfinite(gv)) {
        gv = 0.0;
      }
      
      if (!(gv <= NORM_MAX)) {
        s = NORM_MAX;
      
      } else if (!(gv >= -(NORM_MAX))) {
        s = -(NORM_MAX);
        
      } else {
        s = (int32_t) floor(gv);
      }
      
      /* Write the sample */
      sbuf_sample(s, s);
    }
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < SAMP_RATE; x++) {
      sbuf_sample(0, 0);
    }
  }
  
  /* Free ADSR envelope if allocated */
  adsr_release(adsr);
  adsr = NULL;
  
  /* Free generator graph if allocated */
  generator_release(pMod);
  pMod = NULL;
  
  generator_release(pRoot);
  pRoot = NULL;
  
  /* Stream normalized samples to wave file */
  if (status) {
    sbuf_stream(NORM_MAX);
  }
  
  /* Close down */
  sbuf_close();
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
