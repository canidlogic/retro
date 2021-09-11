/*
 * generator.c
 * ===========
 * 
 * Implementation of generator.h
 * 
 * See the header for further information.
 */

#include "generator.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* @@TODO: */
#include <stdio.h>

/*
 * Type declarations
 * -----------------
 */

/*
 * Function pointer to a generator function.
 * 
 * The void pointer is a custom parameter that is passed through to the
 * function.  This represents the class data for the generator object.
 * 
 * The first int32_t parameter is the sample offset from the start of
 * the sound.
 * 
 * The GENERATOR_OPDATA parameter is a pointer to the array of instance
 * data structures for operators.
 * 
 * The second int32_t parameter is the number of structures in the
 * instance data array.
 * 
 * The return value is the generated floating-point value at this
 * location.
 */
typedef double (*fp_gen)(
    void *,
    int32_t,
    GENERATOR_OPDATA *,
    int32_t);

/*
 * Function pointer to the length function.
 * 
 * This function determines the full length of the rendered sound for
 * the particular rendering instance.
 * 
 * The void pointer is a custom parameter that is passed through to the
 * function.  This represents the class data for the generator object.
 * 
 * The GENERATOR_OPDATA parameter is a pointer to the array of instance
 * data structures for operators.
 * 
 * The int32_t parameter is the number of structures in the instance
 * data array.
 * 
 * The return value is the full length in samples.
 */
typedef int32_t (*fp_len)(
    void *,
    GENERATOR_OPDATA *,
    int32_t);

/*
 * Function pointer to a bind implementation.
 * 
 * This function is used to bind generator objects to indices within an
 * instance data array and determine the total length of the instance
 * data array.
 * 
 * The void pointer is a custom parameter that is passed through to the
 * function.  This represents the class data for the generator object.
 * 
 * The int32_t parameter is the total number of instance data indices
 * that have been already assigned.
 * 
 * The return value is the total number of instance data indices that
 * have been assigned after this call completes.
 */
typedef int32_t (*fp_bind)(void *, int32_t);

/*
 * Function pointer to a destructor routine.
 * 
 * The void pointer is the custom parameter representing the class data.
 * It is the responsibility of the destructor routine to release this
 * class data block.
 */
typedef void (*fp_free)(void *);

/*
 * GENERATOR structure.
 * 
 * Prototype given in header.
 */
struct GENERATOR_TAG {
  
  /*
   * The class data for this generator object.
   */
  void *pClass;
  
  /*
   * Pointer to the generator function for this generator object.
   */
  fp_gen fGen;
  
  /*
   * Pointer to the length function for this generator object.
   */
  fp_len fLen;
  
  /*
   * Pointer to the bind function for this generator object.
   */
  fp_bind fBind;
  
  /*
   * Pointer to the destructor function for this generator object.
   */
  fp_free fFree;
  
  /*
   * The reference count of this generator object.
   */
  int32_t refcount;
};

/*
 * Class data for scaling objects.
 */
typedef struct {
  
  /*
   * The base generator object that is modified by scaling.
   * 
   * Class data structure owns a reference to this object.
   */
  GENERATOR *pBase;
  
  /*
   * The finite scaling value to apply to all samples.
   */
  double scale;
  
} SCALE_CLASS;

/*
 * Class data for operator objects.
 * 
 * All of these parameters are copied from the generator_op() function.
 * See the documentation of that function for the meaning of each of
 * these fields.
 */
typedef struct {
  
  /*
   * Object references.
   * 
   * Class data structure owns a reference to each of these objects.
   * 
   * The generator references may be NULL.
   */
  ADSR_OBJ *pAmp;
  GENERATOR *pFM;
  GENERATOR *pAM;
  
  /*
   * Floating-point parameters.
   */
  double freq_mul;
  double freq_boost;
  
  /*
   * Integer parameters.
   */
  int32_t pod_i;      /* -1 if not bound yet */
  int32_t samp_rate;
  int32_t hlimit;
  int32_t ny_limit;
  int fop;
  
} OP_CLASS;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static double f_sine(double w);
static double f_square(
    double  w,
    double  f,
    int32_t r,
    int32_t hlimit,
    int32_t ny_limit);
static double f_triangle(
    double  w,
    double  f,
    int32_t r,
    int32_t hlimit,
    int32_t ny_limit);
static double f_sawtooth(
    double  w,
    double  f,
    int32_t r,
    int32_t hlimit,
    int32_t ny_limit);
static double f_noise(void);

static double gen_additive(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static double gen_scale(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static double gen_op(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static int32_t len_additive(
    void             * pClass,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static int32_t len_scale(
    void             * pClass,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static int32_t len_op(
    void             * pClass,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static int32_t bind_additive(void *pClass, int32_t start);
static int32_t bind_scale(void *pClass, int32_t start);
static int32_t bind_op(void *pClass, int32_t start);

static void free_additive(void *pCustom);
static void free_scale(void *pCustom);
static void free_op(void *pCustom);

/*
 * The sine wave function.
 * 
 * w is the normalized location within the sine wave, where 0.0 is the
 * start of the sine wave and 1.0 is the end of the sine wave.  The
 * input is clamped, so that w values less than zero are set to zero and
 * w values greater than one are set to one.  Non-finite input is set to
 * value of zero.
 * 
 * This function always computes the pure sine function at the given w
 * location on the wave.  Sine waves do not have complications involving
 * harmonics and the Nyquist limit, unlike the other wave forms.  It is
 * assumed that the client has already checked that the frequency of the
 * sine wave is below the Nyquist limit before calling this function.
 * 
 * Parameters:
 * 
 *   w - the normalized location on the sine wave
 * 
 * Return:
 * 
 *   the sine wave value, in range [-1.0, 1.0]
 */
static double f_sine(double w) {
  
  /* Fix input parameter */
  if (!isfinite(w)) {
    w = 0.0;
  }
  if (!(w >= 0.0)) {
    w = 0.0;
  } else if (!(w <= 1.0)) {
    w = 1.0;
  }
  
  /* Compute normalized sine */
  return sin(w * 2.0 * M_PI);
}

/* @@TODO: */
static double f_square(
    double  w,
    double  f,
    int32_t r,
    int32_t hlimit,
    int32_t ny_limit) {
  /* @@TODO: */
  fprintf(stderr, "TODO: f_square\n");
  abort();
}
static double f_triangle(
    double  w,
    double  f,
    int32_t r,
    int32_t hlimit,
    int32_t ny_limit) {
  /* @@TODO: */
  fprintf(stderr, "TODO: f_triangle\n");
  abort();
}
static double f_sawtooth(
    double  w,
    double  f,
    int32_t r,
    int32_t hlimit,
    int32_t ny_limit) {
  /* @@TODO: */
  fprintf(stderr, "TODO: f_sawtooth\n");
  abort();
}

/*
 * The noise function.
 * 
 * Return:
 * 
 *   a random value in range [-1.0, 1.0]
 */
static double f_noise(void) {
  
  double result = 0.0;
  
  /* Generate random value in range [0.0, 2.0] */
  result = 2.0 * (((double) rand()) / ((double) RAND_MAX));
  
  /* Move result to range [-1.0, 1.0] */
  return (result - 1.0);
}

/*
 * Additive generator function.
 * 
 * Matches the interface of fp_gen.
 */
static double gen_additive(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {
  
  GENERATOR **ppg = NULL;
  double result = 0.0;
  double retval = 0.0;
  
  /* Check parameters */
  if ((pClass == NULL) || (t < 0) ||
      (pods == NULL) || (pod_count < 1)) {
    abort();
  }
  
  /* Cast the class data to a NULL-terminated array of generator
   * pointers */
  ppg = (GENERATOR **) pClass;
  
  /* Invoke all the generators and sum their results together */
  for( ; *ppg != NULL; ppg++) {
    
    /* Invoke current generator */
    retval = generator_invoke(*ppg, pods, pod_count, t);
    
    /* Set to zero if not finite */
    if (!isfinite(retval)) {
      retval = 0.0;
    }
    
    /* Add to result */
    result = result + retval;
  }
  
  /* If result is not finite, set to zero */
  if (!isfinite(result)) {
    result = 0.0;
  }
  
  /* Return summed result */
  return result;
}

/*
 * Scaling generator function.
 * 
 * Matches the interface of fp_gen.
 */
static double gen_scale(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {
  
  SCALE_CLASS *pc = NULL;
  double result = 0.0;
  
  /* Check parameters */
  if ((pClass == NULL) || (t < 0) ||
      (pods == NULL) || (pod_count < 1)) {
    abort();
  }
  
  /* Cast the class data to the appropriate structure pointer */
  pc = (SCALE_CLASS *) pClass;
  
  /* Call through to underlying generator to get sample */
  result = generator_invoke(pc->pBase, pods, pod_count, t);
  
  /* Multiply by scaling value */
  result = result * pc->scale;
  
  /* Return scaled result */
  return result;
}

/*
 * Operator generator function.
 * 
 * Matches the interface of fp_gen.
 */
static double gen_op(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {
  
  OP_CLASS *pc = NULL;
  GENERATOR_OPDATA *pod = NULL;
  double f = 0.0;
  double w_adv = 0.0;
  double dummy = 0.0;
  double nval = 0.0;
  double amp = 0.0;
  double result = 0.0;

  /* Check parameters */
  if ((pClass == NULL) || (t < 0) ||
      (pods == NULL) || (pod_count < 1)) {
    abort();
  }
  
  /* Cast the class data to the appropriate structure pointer */
  pc = (OP_CLASS *) pClass;
  
  /* Make sure the instance data index is bound and in range, then get a
   * pointer to the instance data for this operator */
  if ((pc->pod_i < 0) || (pc->pod_i >= pod_count)) {
    abort();
  }
  pod = &(pods[pc->pod_i]);
  
  /* If instance data t is -1 or greater, make sure that t is either one
   * greater than current t value or equal to it */
  if (pod->t >= -1) {
    if ((t != pod->t) && (t != pod->t + 1)) {
      abort();
    }
  }
  
  /* Determine what to do */
  if (pod->t < -1) {
    /* Operator is disabled so we should just generate zero */
    result = 0.0;
    
  } else if (pod->t == t) {
    /* We've already generated sample for this time, so return the
     * sample we generated */
    result = pod->current;
    
  } else {
    /* We need to generate a sample, so unless we have a NOISE function,
     * we need to figure out the frequency at the current time */
    if (pc->fop != GENERATOR_F_NOISE) {
      /* Multiply the frequency that is being rendered with the
       * frequency multiplier of the operator */
      f = pod->freq * pc->freq_mul;
      
      /* Add the frequency boost */
      f = f + pc->freq_boost;
    }
    
    /* Only proceed if we have a NOISE function, OR a finite frequency
     * that is greater than zero and less than the frequency limit for
     * this operator; else, disable the operator and result is zero */
    if ((pc->fop == GENERATOR_F_NOISE) || 
        (isfinite(f) && (f > 0.0) && (f < ((double) pc->ny_limit)))) {
      
      /* If we don't have the NOISE function, we need to update w
       * according to the frequency we just computed */
      if (pc->fop != GENERATOR_F_NOISE) {
        /* The w_adv value is waves per sample; we compute this by
         * taking the frequency (waves per second) and then dividing by
         * the sampling rate (samples per second) */
        w_adv = f / ((double) pc->samp_rate);

        /* If we have a frequency modulator, add that to w_adv */
        if (pc->pFM != NULL) {
          w_adv = w_adv + generator_invoke(pc->pFM, pods, pod_count, t);
        }
        
        /* Advance w by w_adv */
        pod->w = pod->w + w_adv;
        
        /* If the advanced w is not finite then there was a computation
         * problem, so clear it to zero */
        if (!isfinite(pod->w)) {
          pod->w = 0.0;
        }

        /* Drop the integer portion of w because the waves wrap */
        pod->w = modf(pod->w, &dummy);
        
        /* If w is negative, add 1.0 to it to get it in range */
        if (pod->w < 0.0) {
          pod->w += 1.0;
        }
        
        /* If w is not finite or not >= zero then there was a
         * computation problem, so clear it to zero */
        if ((!isfinite(pod->w)) || (!(pod->w >= 0.0))) {
          pod->w = 0.0;
        }
      }

      /* We now need to compute the function value at w, in range
       * [-1.0, 1.0] */
      if (pc->fop == GENERATOR_F_SINE) {
        nval = f_sine(pod->w);
      
      } else if (pc->fop == GENERATOR_F_SQUARE) {
        nval = f_square(
                  pod->w,
                  f,
                  pc->samp_rate,
                  pc->hlimit,
                  pc->ny_limit);
        
      } else if (pc->fop == GENERATOR_F_TRIANGLE) {
        nval = f_triangle(
                  pod->w,
                  f,
                  pc->samp_rate,
                  pc->hlimit,
                  pc->ny_limit);
        
      } else if (pc->fop == GENERATOR_F_SAWTOOTH) {
        nval = f_sawtooth(
                  pod->w,
                  f,
                  pc->samp_rate,
                  pc->hlimit,
                  pc->ny_limit);
        
      } else if (pc->fop == GENERATOR_F_NOISE) {
        nval = f_noise();
        
      } else {
        /* Unrecognized function */
        abort();
      }
      
      /* Next task is to compute amplitude; begin with the ADSR
       * envelope */
      amp = (((double) adsr_compute(pc->pAmp, t, pod->dur)) /
                ((double) MAX_FRAC));
      
      /* If there is amplitude modulation, add it in */
      if (pc->pAM != NULL) {
        amp = amp + generator_invoke(pc->pAM, pods, pod_count, t);
      }
      
      /* If amplitude is not finite, set to zero */
      if (!isfinite(amp)) {
        amp = 0.0;
      }
      
      /* Compute current sample by multiplying normalized function value
       * with amplitude */
      pod->current = amp * nval;
      
      /* If current sample is not finite, clear it to zero */
      if (!isfinite(pod->current)) {
        pod->current = 0.0;
      }
      
      /* Update t value */
      pod->t = t;
      
      /* Result is current */
      result = pod->current;
    
    } else {
      /* Frequency limit breached or modulation and feedback drove
       * frequency to zero or negative or numeric error computing the
       * frequency, so disable the operator and result is zero */
      pod->t = -2;
      result = 0.0;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Length routine for additive generators.
 * 
 * This matches the interface of fp_gen.
 */
static int32_t len_additive(
    void             * pClass,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {
  
  GENERATOR **ppg = NULL;
  int32_t result = 0;
  int32_t retval = 0;
  
  /* Check parameters */
  if ((pClass == NULL) ||
      (pods == NULL) || (pod_count < 1)) {
    abort();
  }
  
  /* Cast the class data to a NULL-terminated array of generator
   * pointers */
  ppg = (GENERATOR **) pClass;
  
  /* Invoke all the generators and get the maximum of all their
   * lengths */
  for( ; *ppg != NULL; ppg++) {
  
    /* Get length for current generator */
    retval = generator_length(*ppg, pods, pod_count);
    
    /* Update result */
    if (retval > result) {
      result = retval;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Length routine for scaling generators.
 * 
 * This matches the interface of fp_gen.
 */
static int32_t len_scale(
    void             * pClass,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {
  
  SCALE_CLASS *pc = NULL;
  
  /* Check parameters */
  if ((pClass == NULL) ||
      (pods == NULL) || (pod_count < 1)) {
    abort();
  }
  
  /* Cast the class data to the appropriate structure pointer */
  pc = (SCALE_CLASS *) pClass;
  
  /* Call through to underlying generator */
  return generator_length(pc->pBase, pods, pod_count);
}

/*
 * Length routine for operator generators.
 * 
 * This matches the interface of fp_gen.
 */
static int32_t len_op(
    void             * pClass,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {
  
  OP_CLASS *pc = NULL;
  GENERATOR_OPDATA *pod = NULL;

  /* Check parameters */
  if ((pClass == NULL) ||
      (pods == NULL) || (pod_count < 1)) {
    abort();
  }

  /* Cast the class data to the appropriate structure pointer */
  pc = (OP_CLASS *) pClass;

  /* Make sure the instance data index is bound and in range, then get a
   * pointer to the instance data for this operator */
  if ((pc->pod_i < 0) || (pc->pod_i >= pod_count)) {
    abort();
  }
  pod = &(pods[pc->pod_i]);

  /* Return the length from the ADSR envelope */
  return adsr_length(pc->pAmp, pod->dur);
}

/*
 * Bind routine for additive generators.
 * 
 * This matches the interface of fp_bind.
 */
static int32_t bind_additive(void *pClass, int32_t start) {
  
  GENERATOR **ppg = NULL;
  
  /* Check parameters */
  if ((pClass == NULL) || (start < 0)) {
    abort();
  }
  
  /* Cast the class data to a NULL-terminated array of generator
   * pointers */
  ppg = (GENERATOR **) pClass;
  
  /* Bind all generators */
  for( ; *ppg != NULL; ppg++) {
    start = generator_bind(*ppg, start);
  }
  
  /* Return updated count */
  return start;
}

/*
 * Bind routine for scaling generators.
 * 
 * This matches the interface of fp_bind.
 */
static int32_t bind_scale(void *pClass, int32_t start) {
  
  SCALE_CLASS *pc = NULL;
  
  /* Check parameters */
  if ((pClass == NULL) || (start < 0)) {
    abort();
  }
  
  /* Cast the class data to the appropriate structure pointer */
  pc = (SCALE_CLASS *) pClass;
  
  /* Call through to underlying generator */
  return generator_bind(pc->pBase, start);
}

/* 
 * Bind routine for operators.
 * 
 * This matches the interface of fp_bind.
 */
static int32_t bind_op(void *pClass, int32_t start) {
  
  OP_CLASS *pc = NULL;
  
  /* Check parameters */
  if ((pClass == NULL) || (start < 0)) {
    abort();
  }
  
  /* Cast the class data to the appropriate structure pointer */
  pc = (OP_CLASS *) pClass;
  
  /* Bind the index of this operator and update start, watching for
   * overflow */
  if (start < INT32_MAX) {
    pc->pod_i = start;
    start++;
    
  } else {
    /* Overflow */
    abort();
  }
  
  /* If there are modulators, bind them */
  if (pc->pFM != NULL) {
    start = generator_bind(pc->pFM, start);
  }
  if (pc->pAM != NULL) {
    start = generator_bind(pc->pAM, start);
  }
  
  /* Return updated count */
  return start;
}

/*
 * Destructor routine for class data of additive generators.
 * 
 * This matches the interface of fp_free.
 */
static void free_additive(void *pCustom) {
  
  GENERATOR **ppg = NULL;

  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }
  
  /* Cast class data pointer */
  ppg = (GENERATOR **) pCustom;
  
  /* Release references to all generators and replace them with NULL
   * pointers */
  for( ; *ppg != NULL; ppg++) {
    generator_release(*ppg);
    *ppg = NULL;
  }
  
  /* Now we can free the array memory */
  free(pCustom);
}

/*
 * Destructor routine for class data of scaling generators.
 * 
 * This matches the interface of fp_free.
 */
static void free_scale(void *pCustom) {
  
  SCALE_CLASS *pc = NULL;
  
  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }

  /* Cast the class data to the appropriate structure pointer */
  pc = (SCALE_CLASS *) pCustom;
  
  /* Release any references stored in the class data */
  generator_release(pc->pBase);
  pc->pBase = NULL;
  
  /* Now we can free the structure */
  free(pc);
}

/*
 * Destructor routine for class data of operator generators.
 * 
 * This matches the interface of fp_free.
 */
static void free_op(void *pCustom) {
  
  OP_CLASS *pc = NULL;
  
  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }

  /* Cast the class data to the appropriate structure pointer */
  pc = (OP_CLASS *) pCustom;
  
  /* Release any references stored in the class data */
  adsr_release(pc->pAmp);
  pc->pAmp = NULL;
  
  generator_release(pc->pFM);
  pc->pFM = NULL;
  
  generator_release(pc->pAM);
  pc->pAM = NULL;
  
  /* Now we can free the structure */
  free(pc);
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * generator_opdata_init function.
 */
void generator_opdata_init(
    GENERATOR_OPDATA * pod,
    double             freq,
    int32_t            dur) {
  
  /* Check parameters */
  if (pod == NULL) {
    abort();
  }
  if (!isfinite(freq)) {
    abort();
  }
  if (!(freq > 0.0)) {
    abort();
  }
  if (dur < 1) {
    abort();
  }
  
  /* Blank the structure */
  memset(pod, 0, sizeof(GENERATOR_OPDATA));
  
  /* Initialize the structure */
  pod->w = 0.0;
  pod->freq = freq;
  pod->current = 0.0;
  pod->t = -1;
  pod->dur = dur;
}

/*
 * generator_additive function.
 */
GENERATOR *generator_additive(GENERATOR **ppg, int32_t count) {
  
  GENERATOR **ppnew = NULL;
  GENERATOR *png = NULL;
  int32_t i = 0;
  
  /* Check parameters */
  if ((ppg == NULL) || (count < 1) || (count >= INT32_MAX)) {
    abort();
  }
  
  /* Allocate an array that has one more pointer that will be the null
   * termination pointer */
  ppnew = (GENERATOR **) calloc(
                          (size_t) (count + 1),
                          sizeof(GENERATOR *));
  if (ppnew == NULL) {
    abort();
  }
  
  /* Copy all the pointers */
  memcpy(ppnew, ppg, sizeof(GENERATOR *) * ((size_t) count));
  
  /* Set the pointer after the end to NULL */
  ppnew[count] = NULL;
  
  /* Add references to each of the generators */
  for(i = 0; i < count; i++) {
    generator_addref(ppnew[i]);
  }
  
  /* Allocate a generator structure */
  png = (GENERATOR *) malloc(sizeof(GENERATOR));
  if (png == NULL) {
    abort();
  }
  memset(png, 0, sizeof(GENERATOR));
  
  /* Initialize the generator structure */
  png->pClass = (void *) ppnew;
  png->fGen = &gen_additive;
  png->fLen = &len_additive;
  png->fBind = &bind_additive;
  png->fFree = &free_additive;
  png->refcount = 1;
  
  /* Return the new generator */
  return png;
}

/*
 * generator_scale function.
 */
GENERATOR *generator_scale(GENERATOR *pBase, double scale) {
  
  SCALE_CLASS *pc = NULL;
  GENERATOR *png = NULL;
  
  /* Check parameters */
  if ((pBase == NULL) || (!isfinite(scale))) {
    abort();
  }
  
  /* Add reference to base generator */
  generator_addref(pBase);
  
  /* Allocate scaling class data structure */
  pc = (SCALE_CLASS *) malloc(sizeof(SCALE_CLASS));
  if (pc == NULL) {
    abort();
  }
  memset(pc, 0, sizeof(SCALE_CLASS));
  
  /* Initialize class data */
  pc->pBase = pBase;
  pc->scale = scale;
  
  /* Allocate a generator structure */
  png = (GENERATOR *) malloc(sizeof(GENERATOR));
  if (png == NULL) {
    abort();
  }
  memset(png, 0, sizeof(GENERATOR));
  
  /* Initialize the generator structure */
  png->pClass = (void *) pc;
  png->fGen = &gen_scale;
  png->fLen = &len_scale;
  png->fBind = &bind_scale;
  png->fFree = &free_scale;
  png->refcount = 1;
  
  /* Return the new generator */
  return png;
}

/*
 * generator_op function.
 */
GENERATOR *generator_op(
    int         fop,
    double      freq_mul,
    double      freq_boost,
    ADSR_OBJ  * pAmp,
    GENERATOR * pFM,
    GENERATOR * pAM,
    int32_t     samp_rate,
    int32_t     ny_limit,
    int32_t     hlimit) {
  
  OP_CLASS *pc = NULL;
  GENERATOR *png = NULL;
  
  /* Check parameters */
  if ((fop < GENERATOR_F_MINVAL) || (fop > GENERATOR_F_MAXVAL)) {
    abort();
  }
  if ((!isfinite(freq_mul)) || (!(freq_mul >= 0.0))) {
    abort();
  }
  if (!isfinite(freq_boost)) {
    abort();
  }
  if (pAmp == NULL) {
    abort();
  }
  if ((samp_rate != RATE_CD) && (samp_rate != RATE_DVD)) {
    abort();
  }
  if ((ny_limit < 0) || (ny_limit > samp_rate)) {
    abort();
  }
  if (hlimit < 0) {
    abort();
  }
  
  /* Allocate operator class data structure */
  pc = (OP_CLASS *) malloc(sizeof(OP_CLASS));
  if (pc == NULL) {
    abort();
  }
  memset(pc, 0, sizeof(OP_CLASS));
  
  /* Store function parameters in class data */
  pc->fop = fop;
  pc->freq_mul = freq_mul;
  pc->freq_boost = freq_boost;
  pc->pAmp = pAmp;
  pc->pFM = pFM;
  pc->pAM = pAM;
  pc->samp_rate = samp_rate;
  pc->ny_limit = ny_limit;
  pc->hlimit = hlimit;
  
  /* Set to unbound state */
  pc->pod_i = -1;
  
  /* Add a reference to the ADSR envelope */
  adsr_addref(pc->pAmp);
  
  /* If modulating generators provided, add references to them */
  if (pc->pFM != NULL) {
    generator_addref(pc->pFM);
  }
  if (pc->pAM != NULL) {
    generator_addref(pc->pAM);
  }
  
  /* Allocate a generator structure */
  png = (GENERATOR *) malloc(sizeof(GENERATOR));
  if (png == NULL) {
    abort();
  }
  memset(png, 0, sizeof(GENERATOR));
  
  /* Initialize the generator structure */
  png->pClass = (void *) pc;
  png->fGen = &gen_op;
  png->fLen = &len_op;
  png->fBind = &bind_op;
  png->fFree = &free_op;
  png->refcount = 1;
  
  /* Return the new generator */
  return png;
}

/*
 * generator_addref function.
 */
void generator_addref(GENERATOR *pg) {
  
  /* Check parameter */
  if (pg == NULL) {
    abort();
  }
  
  /* Check that reference count is valid and won't overflow */
  if ((pg->refcount < 1) || (pg->refcount >= INT32_MAX)) {
    abort();
  }
  
  /* Increment reference count */
  (pg->refcount)++;
}

/*
 * generator_release function.
 */
void generator_release(GENERATOR *pg) {
  
  /* Only proceed if non-NULL parameter passed */
  if (pg != NULL) {
    
    /* Check that reference count is valid */
    if (pg->refcount < 1) {
      abort();
    }
    
    /* Decrement reference count */
    (pg->refcount)--;
    
    /* If reference count has dropped to zero, free the generator */
    if (pg->refcount < 1) {
      
      /* Check that there is a destructor routine and class data */
      if ((pg->fFree == NULL) || (pg->pClass == NULL)) {
        abort();
      }
      
      /* Invoke the destructor to release class data */
      (*(pg->fFree))(pg->pClass);
      
      /* Now we can release the generator structure */
      free(pg);
    }
  }
}

/*
 * generator_invoke function.
 */
double generator_invoke(
    GENERATOR        * pg,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count,
    int32_t            t) {
  
  /* Check parameters */
  if ((pg == NULL) || (pods == NULL) ||
      (pod_count < 1) || (t < 0)) {
    abort();
  }
  
  /* Check that class data and generator function are defined */
  if ((pg->pClass == NULL) || (pg->fGen == NULL)) {
    abort();
  }
  
  /* Call through to the generator function */
  return (*(pg->fGen))(pg->pClass, t, pods, pod_count);
}

/*
 * generator_length function.
 */
int32_t generator_length(
    GENERATOR        * pg,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count) {

  /* Check parameters */
  if ((pg == NULL) || (pods == NULL) ||
      (pod_count < 1)) {
    abort();
  }

  /* Check that class data and length function are defined */
  if ((pg->pClass == NULL) || (pg->fLen == NULL)) {
    abort();
  }

  /* Call through to the length function */
  return (*(pg->fLen))(pg->pClass, pods, pod_count);
}

/*
 * generator_bind function.
 */
int32_t generator_bind(GENERATOR *pg, int32_t start) {
  
  /* Check parameters */
  if ((pg == NULL) || (start < 0)) {
    abort();
  }
  
  /* Check that bind function exists */
  if (pg->fBind == NULL) {
    abort();
  }
  
  /* Call through to bind function implementation */
  return (*(pg->fBind))(pg->pClass, start);
}
