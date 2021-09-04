/*
 * adsr.c
 * 
 * Implementation of adsr.h
 * 
 * See the header for further information.
 */

#include "adsr.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * The maximum integer intensity value in an envelope.
 */
#define ADSR_MAXVAL (16384)

/*
 * Type declarations
 * =================
 */

/*
 * ADSR_OBJ structure.
 * 
 * Prototype given in the header.
 */
struct ADSR_OBJ_TAG {
  
  /*
   * The reference count of the object.
   */
  int32_t refcount;
  
  /*
   * The total number of envelope samples.
   * 
   * The sustain period always only has a single sample in the cached
   * envelope.  The total number of envelope samples must be at least
   * one.
   */
  int32_t env_samp;
  
  /*
   * The sample index of the sustain sample within the envelope.
   * 
   * This must be in range [0, env_samp - 1].  All samples before the
   * env_mid sample (if any) are the attack and decay samples.  All
   * samples after the env_mid sample (if any) are the release samples.
   */
  int32_t env_mid;
  
  /*
   * Pointer to the cached envelope.
   * 
   * This is a dynamically allocated array with length env_samp.  Each
   * array element is in range [0, ADSR_MAXVAL], where zero means a
   * value of 0.0 for the envelope and ADSR_MAXVAL means a value of 1.0
   * for the envelope.
   */
  uint16_t *penv;
};

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void adsr_ramp(
    int16_t * pv,
    int32_t   i,
    int32_t   len,
    int16_t   begin_val,
    int16_t   end_val);

/*
 * Create a ramp of integer values.
 * 
 * pv points to the array of integer values.
 * 
 * i is the index of the first array element in the ramp.  It must be
 * zero or greater.
 * 
 * len is the number of elements in the ramp.  It must be zero or
 * greater.
 * 
 * begin_val is the value at the start of the ramp, and end_val is the
 * value at the end of the ramp.  They may have any values.
 * 
 * If len is zero, the function does nothing.  If len is one, a value
 * halfway between begin and end is written to the index.  If len is
 * greater than one, the first value of the ramp is begin_val, and the
 * ramp ends just before end_val.
 * 
 * Parameters:
 * 
 *   pv - pointer to the array
 * 
 *   i - index of the first array element in the ramp
 * 
 *   len - length of the ramp
 * 
 *   begin_val - value at the beginning of the ramp
 * 
 *   end_val - value at the end of the ramp
 */
static void adsr_ramp(
    int16_t * pv,
    int32_t   i,
    int32_t   len,
    int16_t   begin_val,
    int16_t   end_val) {
  
  double vf = 0.0;
  int32_t r = 0;
  int32_t x = 0;
  
  /* Check parameters */
  if (pv == NULL) {
    abort();
  }
  if ((i < 0) || (len < 0)) {
    abort();
  }
  
  /* Different ramps depending on length */
  if (len == 1) {
    /* Only one element -- compute halfway point */
    vf = (((double) begin_val) + ((double) end_val)) / 2.0;
    r = (int32_t) vf;
    if (r > INT16_MAX) {
      r = INT16_MAX;
    } else if (r < -(INT16_MAX)) {
      r = -(INT16_MAX);
    }
    
    /* Set the element */
    pv[i] = (int16_t) r;
    
  } else if (len > 1) {
    /* More than one element -- compute the full ramp */
    for(x = 0; x < len; x++) {
      /* Compute progress */
      vf = ((double) x) / ((double) len);
      
      /* Compute value */
      vf = vf * (((double) end_val) - ((double) begin_val)) +
                ((double) begin_val);
      r = (int32_t) vf;
      if (r > INT16_MAX) {
        r = INT16_MAX;
      } else if (r < -(INT16_MAX)) {
        r = -(INT16_MAX);
      }
      
      /* Set the element */
      pv[i + x] = (int16_t) r;
    }
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * adsr_alloc function.
 */
ADSR_OBJ *adsr_alloc(
    double       t_attack,
    double       t_decay,
    double       sustain,
    double       t_release,
    int32_t      rate) {
  
  int32_t v_s = 0;
  int32_t attack = 0;
  int32_t decay = 0;
  int32_t release = 0;
  ADSR_OBJ *pa = NULL;
  
  /* Check parameters */
  if ((!isfinite(t_attack)) || (!isfinite(t_decay)) ||
      (!isfinite(sustain)) || (!isfinite(t_release))) {
    abort();
  }
  if (!((sustain >= 0.0) && (sustain <= 1.0))) {
    abort();
  }
  if ((!(t_attack >= 0.0)) || (!(t_decay >= 0.0)) ||
      (!(t_release >= 0.0))) {
    abort();
  }
  if ((rate != 48000) && (rate != 44100)) {
    abort();
  }
  
  /* If sustain is special value 0.0 or 1.0, then adjust other
   * parameters appropriately */
  if (sustain == 0.0) {
    /* If sustain level is zero, then there is no release */
    t_release = 0.0;
  
  } else if (sustain == 1.0) {
    /* If sustain level is one, there is no decay */
    t_decay = 0.0;
  }
  
  /* Compute the number of samples for the attack, release, and decay */
  t_attack = ((double) rate) * (t_attack / 1000.0);
  t_decay = ((double) rate) * (t_decay / 1000.0);
  t_release = ((double) rate) * (t_release / 1000.0);
  
  /* If any time computation is non-finite or greater than the maximum
   * time, set to max time */
  if (!isfinite(t_attack)) {
    t_attack = (double) ADSR_MAXTIME;
  }
  if (!isfinite(t_decay)) {
    t_decay = (double) ADSR_MAXTIME;
  }
  if (!isfinite(t_release)) {
    t_release = (double) ADSR_MAXTIME;
  }
  
  if (t_attack > (double) ADSR_MAXTIME) {
    t_attack = (double) ADSR_MAXTIME;
  }
  if (t_decay > (double) ADSR_MAXTIME) {
    t_decay = (double) ADSR_MAXTIME;
  }
  if (t_release > (double) ADSR_MAXTIME) {
    t_release = (double) ADSR_MAXTIME;
  }
  
  /* Get the integer sample counts and clamp to range */
  attack = (int32_t) t_attack;
  decay = (int32_t) t_decay;
  release = (int32_t) t_release;
  
  if (attack < 0) {
    attack = 0;
  } else if (attack > ADSR_MAXTIME) {
    attack = ADSR_MAXTIME;
  }
  
  if (decay < 0) {
    decay = 0;
  } else if (decay > ADSR_MAXTIME) {
    decay = ADSR_MAXTIME;
  }
  
  if (release < 0) {
    release = 0;
  } else if (release > ADSR_MAXTIME) {
    release = ADSR_MAXTIME;
  }
  
  /* Compute integer sustain value */
  v_s = (int32_t) (sustain * ((double) ADSR_MAXVAL));
  if (v_s < 0) {
    v_s = 0;
  } else if (v_s > ADSR_MAXVAL) {
    v_s = ADSR_MAXVAL;
  }
  
  /* Allocate object */
  pa = (ADSR_OBJ *) malloc(sizeof(ADSR_OBJ));
  if (pa == NULL) {
    abort();
  }
  memset(pa, 0, sizeof(ADSR_OBJ));
  
  /* Set variables */
  pa->refcount = 1;
  pa->env_samp = attack + decay + release + 1;
  pa->env_mid = attack + decay;
  
  /* Allocate buffer for cached envelope */
  pa->penv = (int16_t *) malloc(pa->env_samp * sizeof(int16_t));
  if (pa->penv == NULL) {
    abort();
  }
  memset(pa->penv, 0, pa->env_samp * sizeof(int16_t));
  
  /* Set the envelope */
  adsr_ramp(pa->penv, 0, attack,
            0, (int16_t) ADSR_MAXVAL);
  
  adsr_ramp(pa->penv, attack, decay,
            (int16_t) ADSR_MAXVAL, (int16_t) v_s);
  
  (pa->penv)[pa->env_mid] = (int16_t) v_s;
  
  adsr_ramp(pa->penv, pa->env_mid + 1, release,
            (int16_t) v_s, 0);
  
  /* Return object */
  return pa;
}

/*
 * adsr_addref function.
 */
void adsr_addref(ADSR_OBJ *pa) {
  if (pa != NULL) {
    if (pa->refcount < INT32_MAX) {
      (pa->refcount)++;
    } else {
      abort();  /* reference count overflow */
    }
  }
}

/*
 * adsr_release function.
 */
void adsr_release(ADSR_OBJ *pa) {
  if (pa != NULL) {
    (pa->refcount)--;
    if (pa->refcount < 1) {
      free(pa->penv);
      free(pa);
    }
  }
}

/*
 * adsr_length function.
 */
int32_t adsr_length(ADSR_OBJ *pa, int32_t dur) {
  
  int64_t d = 0;
  
  /* Check parameters */
  if ((pa == NULL) || (dur < 1)) {
    abort();
  }
  
  /* The duration must include at least one sustain sample, so extend it
   * if necessary */
  if (dur < pa->env_mid + 1) {
    dur = pa->env_mid + 1;
  }
  
  /* Add any release samples to the duration */
  d = ((int64_t) dur) + ((int64_t) (pa->env_samp - pa->env_mid - 1));
  
  /* Truncate to signed 32-bit integer length if necessary */
  if (d > INT32_MAX) {
    d = INT32_MAX;
  }
  
  /* Return the computed duration */
  return (int32_t) d;
}

/*
 * adsr_mul function.
 */
int16_t adsr_mul(
    ADSR_OBJ * pa,
    int32_t    t,
    int32_t    dur,
    int16_t    s) {
  
  int32_t result = 0;
  int32_t el = 0;
  int32_t ie = 0;
  
  /* Check parameters */
  if (pa == NULL) {
    abort();
  }
  if ((t < 0) || (dur < 1) ||
      (intensity < 0) || (intensity > ADSR_MAXVAL)) {
    abort();
  }
  
  /* First of all, set result to input sample */
  result = (int32_t) s;
  
  /* If the t value is less than or equal to env_mid, then t is the
   * index within the cached envelope; else, we need further computation
   * to determine the index */
  if (t <= pa->env_mid) {
    ie = t;
  
  } else {
    /* Further computation required to determine envelope index --
     * first, get the full length of the envelope */
    el = adsr_length(pa, dur);
    
    /* Flip t so that t=0 is the last sample of the envelope and t moves
     * backwards */
    t = el - t - 1;
    
    /* If t is greater than the cached envelope length minus one, set it
     * to the cached envelope length minus one */
    if (t > (pa->env_samp - 1)) {
      t = pa->env_samp - 1;
    }
    
    /* Flip t again so that it is an offset within the cached
     * envelope */
    t = pa->env_samp - t - 1;
    
    /* If t is less than the sustain sample, set it to the sustain
     * sample */
    if (t < pa->env_mid) {
      t = pa->env_mid;
    }
    
    /* t is now the index within the cached envelope */
    ie = t;
  }
  
  /* Apply the envelope value */
  result = (result * ((int32_t) (pa->penv)[ie])) / ADSR_MAXVAL;
  
  if (result > INT16_MAX) {
    result = INT16_MAX;
  } else if (result < -(INT16_MAX)) {
    result = -(INT16_MAX);
  }
  
  /* Return the transformed sample */
  return (int16_t) result;
}
