/*
 * enharmonic4.c
 * =============
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
 * Compile with the adsr sbuf and wavwrite modules.
 * 
 * May require the math library to be included with -lm
 */

#include "adsr.h"
#include "wavwrite.h"
#include "retrodef.h"
#include "sbuf.h"
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
#define DUR_SAMPLES (1000)

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
 * Type declarations
 * =================
 */

/*
 * Function pointer to a generator function.
 * 
 * The void pointer is a custom parameter that is passed through to the
 * function.
 * 
 * The first integer parameter is the sample offset from the start of
 * the sound.
 * 
 * The second integer is the sample rate, which is either RATE_DVD or
 * RATE_CD.
 * 
 * The return value is the generated floating-point value at this
 * location.
 */
typedef double (*fp_gen)(void *, int32_t, int32_t);

/*
 * Structure holding a generator function pointer and its custom data.
 * 
 * Use gen_invoke() to invoke the generator.  Use gen_init() to
 * initialize.
 */
typedef struct {
  
  /*
   * The generator function pointer.
   */
  fp_gen fp;
  
  /*
   * The custom data.
   */
  void *pCustom;
  
} GENERATOR;

/*
 * Information for a sine wave generator.
 */
typedef struct {
  
  /*
   * The frequency generator.
   */
  GENERATOR *pFreq;
  
  /*
   * The amplitude generator.
   */
  GENERATOR *pAmp;
  
  /*
   * The current time location in the generator.
   * 
   * -1 if nothing generated yet.
   */
  int32_t t;
  
  /*
   * The current radian location in the generator.
   */
  double r;
  
} SINE_INFO;

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static double const_func(void *pCustom, int32_t i, int32_t r);
static double sine_func(void *pCustom, int32_t i, int32_t r);

static void gen_constant(GENERATOR *pg, double val);
static void gen_sine(
    GENERATOR *pg,
    GENERATOR *pFreq,
    GENERATOR *pAmp);

static void gen_init(GENERATOR *pg);
static double gen_invoke(GENERATOR *pg, int32_t i, int32_t r);

static int enharmonic(const char *pPath);

/*
 * Constant value generator.
 */
static double const_func(void *pCustom, int32_t i, int32_t r) {
  
  double *pval = NULL;
  
  /* Ignore time parameters */
  (void) i;
  (void) r;
  
  /* Check parameters */
  if (pCustom == NULL) {
    abort();
  }
  
  /* Get data */
  pval = (double *) pCustom;
  
  /* Return constant */
  return *pval;
}

/*
 * Sine wave generator.
 */
static double sine_func(void *pCustom, int32_t i, int32_t r) {
  
  SINE_INFO *psi = NULL;
  double freq = 0.0;
  
  /* Check parameters */
  if (pCustom == NULL) {
    abort();
  }
  if (i < 0) {
    abort();
  }
  if ((r != RATE_DVD) && (r != RATE_CD)) {
    abort();
  }
  
  /* Get data */
  psi = (SINE_INFO *) pCustom;
  
  /* If time not current, update time and radians */
  if (psi->t != i) {
    /* Update time */
    psi->t = i;
    
    /* Get current frequency */
    freq = gen_invoke(psi->pFreq, i, r);
    
    /* Convert frequency to radians per second */
    freq = freq * M_PI * 2.0;
    
    /* Convert frequency to radians per sample */
    freq = freq / ((double) r);
    
    /* Add to current radian location */
    psi->r = psi->r + freq;
    
    /* While current radian location is greater than 2*PI, reduce by
     * 2*PI */
    while (psi->r > 2.0 * M_PI) {
      psi->r = psi->r - (2.0 * M_PI);
    }
  }
  
  /* Compute current sample */
  return (sin(psi->r) * gen_invoke(psi->pAmp, i, r));
}

/*
 * Define a constant value generator.
 * 
 * Parameters:
 * 
 *   pg - the generator structure to set as the constant value generator
 * 
 *   val - the constant value
 */
static void gen_constant(GENERATOR *pg, double val) {
  
  double *pval = NULL;
  
  /* Check parameters */
  if ((pg == NULL) || (!isfinite(val))) {
    abort();
  }
  
  /* Allocate constant information */
  pval = (double *) malloc(sizeof(double));
  if (pval == NULL) {
    abort();
  }
  memset(pval, 0, sizeof(double));
  
  /* Store constant valuer */
  *pval = val;
  
  /* Set generator */
  pg->fp = &const_func;
  pg->pCustom = (void *) pval;
}

/*
 * Define a sine wave generator.
 * 
 * Parameters:
 * 
 *   pg - the generator structure to set as the sine wave generator
 * 
 *   pFreq - the generator to use for driving the frequency
 * 
 *   pAmp - the generator to use for driving the amplitude
 */
static void gen_sine(
    GENERATOR *pg,
    GENERATOR *pFreq,
    GENERATOR *pAmp) {
  
  SINE_INFO *si = NULL;
  
  /* Check parameters */
  if ((pg == NULL) || (pFreq == NULL) || (pAmp == NULL)) {
    abort();
  }
  
  if ((pg == pFreq) || (pg == pAmp)) {
    abort();
  }
  
  /* Allocate sine information */
  si = (SINE_INFO *) malloc(sizeof(SINE_INFO));
  if (si == NULL) {
    abort();
  }
  memset(si, 0, sizeof(SINE_INFO));
  
  /* Store sine information */
  si->pFreq = pFreq;
  si->pAmp = pAmp;
  
  /* Initialize state */
  si->t = -1;
  si->r = 0.0;
  
  /* Set generator */
  pg->fp = &sine_func;
  pg->pCustom = (void *) si;
}

/*
 * Initialize a generator structure.
 * 
 * This writes a NULL pointer and NULL custom data.  Invokes will fault.
 * 
 * Parameter:
 * 
 *   pg - the generator structure to initialize
 */
static void gen_init(GENERATOR *pg) {
  
  /* Check parameter */
  if (pg == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pg, 0, sizeof(GENERATOR));
  pg->fp = NULL;
  pg->pCustom = NULL;
}

/*
 * Invoke a generator function.
 * 
 * Parameters:
 * 
 *   pg - the generator to invoke
 * 
 *   i - the sample offset
 * 
 *   r - the sampling rate, either RATE_CD or RATE_DVD
 * 
 * Return:
 * 
 *   the generated value
 */
static double gen_invoke(GENERATOR *pg, int32_t i, int32_t r) {
  
  /* Check parameters */
  if (pg == NULL) {
    abort();
  }
  if (i < 0) {
    abort();
  }
  if ((r != RATE_DVD) && (r != RATE_CD)) {
    abort();
  }
  
  /* Fault if nothing in structure */
  if (pg->fp == NULL) {
    abort();
  }
  
  /* Call through */
  return (*(pg->fp))(pg->pCustom, i, r);
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
  double gv = 0.0;
  int32_t s = 0;
  
  GENERATOR g_freq;
  GENERATOR g_amp;
  GENERATOR g;
  
  /* @@TODO: generator memory leaks */
  /* @@TODO: apply ADSR envelope */
  
  /* Initialize structures */
  gen_init(&g_freq);
  gen_init(&g_amp);
  gen_init(&g);
  
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
  
  /* Create generator graph */
  if (status) {
    gen_constant(&g_freq, 440.0);
    gen_constant(&g_amp, 1000.0);
    gen_sine(&g, &g_freq, &g_amp);
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
  
  /* Set the duration of the event in samples */
  if (status) {
    dur = DUR_SAMPLES;
  }
  
  /* Compute the envelope duration in samples */
  if (status) {
    edur = adsr_length(adsr, dur);
  }
  
  /* Write one second of silence */
  if (status) {
    for(x = 0; x < SAMP_RATE; x++) {
      sbuf_sample(0, 0);
    }
  }
  
  /* Write the enharmonic sound */
  if (status) {
    for(x = 0; x < edur; x++) {
      /* Get the generated sample value */
      gv = gen_invoke(&g, x, SAMP_RATE);
      
      /* Clamp to range */
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
