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

/*
 * Constants
 * ---------
 */

/*
 * Generator object types.
 */
#define GEN_TYPE_OPERATOR   (1)
#define GEN_TYPE_ADDITIVE   (2)

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
   * Pointer to the destructor function for this generator object.
   */
  fp_free fFree;
  
  /*
   * The reference count of this generator object.
   */
  int32_t refcount;
  
  /*
   * The generator object type.
   * 
   * Must be one of the GEN_TYPE constants.
   */
  int gtype;
};

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
  double base_amp;
  double fm_feedback;
  double am_feedback;
  double fm_scale;
  double am_scale;
  
  /*
   * Integer parameters.
   */
  int32_t pod_i;
  int fop;
  
} OP_CLASS;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static double f_sine(double w);
static double f_square(double w);
static double f_triangle(double w);
static double f_sawtooth(double w);
static double f_noise(void);

static double gen_additive(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

static double gen_op(
    void             * pClass,
    int32_t            t,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

/* @@TODO: */
static double f_sine(double w);
static double f_square(double w);
static double f_triangle(double w);
static double f_sawtooth(double w);
static double f_noise(void);

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
  
  /* Make sure the instance data index is in range, then get a pointer
   * to the instance data for this operator */
  if ((pc->pod_i < 0) || (pc->pod_i >= pod_count)) {
    abort();
  }
  pod = &(pods[pc->pod_i]);
  
  /* If t is -1 or greater, make sure that t is either one greater than
   * current t value or equal to it */
  if (t >= -1) {
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
      /* Start by multiplying the frequency that is being rendered with
       * the frequency multiplier of the operator */
      f = pod->freq * pc->freq_mul;
      
      /* If we have FM feedback, add that in, properly scaled */
      if (pc->fm_feedback != 0.0) {
        f = f + (pc->fm_feedback * pod->last);
      }
      
      /* If we have a frequency modulator, add that in, properly
       * scaled */
      if ((pc->pFM != NULL) && (fm_scale != 0.0)) {
        f = f + (pc->fm_scale *
                    generator_invoke(pc->pFM, pods, pod_count, t));
      }
    }
    
    /* Only proceed if we have a NOISE function, OR a finite frequency
     * that is greater than zero and less than the frequency limit for
     * this operator; else, disable the operator and result is zero */
    if ((pc->fop == GENERATOR_F_NOISE) || 
        (isfinite(f) && (f > 0.0) && (f < ((double) pod->ny_limit)))) {
      
      /* If we don't have the NOISE function, we need to update w
       * according to the frequency we just computed */
      if (pc->fop != GENERATOR_F_NOISE) {
        /* The w_adv value is waves per sample; we compute this by
         * taking the frequency (waves per second) and then dividing by
         * the sampling rate (samples per second) */
        w_adv = f / ((double) pod->samp_rate);
        
        /* Advance w by w_adv */
        pod->w = pod->w + w_adv;
        
        /* If the advanced w is not finite or not >= zero then there was
         * a computation problem, so clear it to zero */
        if ((!isfinite(pod->w)) || (!(pod->w >= 0.0))) {
          pod->w = 0.0;
        }
        
        /* Drop the integer portion of w because the waves wrap */
        pod->w = modf(pod->w, &dummy);
        
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
        nval = f_square(pod->w);
        
      } else if (pc->fop == GENERATOR_F_TRIANGLE) {
        nval = f_triangle(pod->w);
        
      } else if (pc->fop == GENERATOR_F_SAWTOOTH) {
        nval = f_sawtooth(pod->w);
        
      } else if (pc->fop == GENERATOR_F_NOISE) {
        nval = f_noise();
        
      } else {
        /* Unrecognized function */
        abort();
      }
      
      /* Next task is to compute amplitude; begin by multiplying the
       * base amplitude by the ADSR envelope */
      amp = pc->base_amp * 
              (((double) adsr_compute(pc->pAmp, t, pod->dur)) /
                ((double) MAX_FRAC));
      
      /* If there is amplitude feedback, add it in, properly scaled */
      if (pc->am_feedback != 0.0) {
        amp = amp + (pc->am_feedback * pod->last);
      }
      
      /* If there is amplitude modulation, add it in, properly scaled */
      if ((pc->pAM != NULL) && (pc->am_scale != 0.0)) {
        amp = amp + (pc->am_scale *
                      generator_invoke(pc->pAM, pods, pod_count, t));
      }
      
      /* If amplitude is not finite, set to zero */
      if (!isfinite(amp)) {
        amp = 0.0;
      }
      
      /* Store current sample in last, and then compute current sample
       * by multiplying normalized function value with amplitude */
      pod->last = pod->current;
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
      /* Frequency limit breached or numeric error computing the
       * frequency, so disable the operator and result is zero */
      pod->t = -2;
      result = 0.0;
    }
  }
  
  /* Return result */
  return result;
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
    int32_t            dur,
    int32_t            samp_rate,
    int32_t            ny_limit) {
  
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
  if ((samp_rate != RATE_CD) && (samp_rate != RATE_DVD)) {
    abort();
  }
  if ((ny_limit < 0) || (ny_limit > samp_rate)) {
    abort();
  }
  
  /* Blank the structure */
  memset(pod, 0, sizeof(GENERATOR_OPDATA));
  
  /* Initialize the structure */
  pod->w = 0.0;
  pod->freq = freq;
  pod->current = 0.0;
  pod->last = 0.0;
  pod->t = -1;
  pod->dur = dur;
  pod->samp_rate = samp_rate;
  pod->ny_limit = ny_limit;
}

/*
 * generator_additive function.
 */
GENERATOR *generator_additive(GENERATOR **ppg, int32_t count) {
  
  /* Check parameters */
  if ((ppg == NULL) || (count < 1)) {
    abort();
  }
  
  /* @@TODO: */
  
}
