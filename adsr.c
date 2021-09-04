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
   * The maximum and minimum intensity values, in integer range.
   * 
   * Both must be in range [0, ADSR_MAXVAL], i_max may not be zero, and
   * and i_max must be greater than or equal to i_min.
   */
  int16_t i_max;
  int16_t i_min;
  
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
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * adsr_init function.
 */
ADSR_OBJ *adsr_alloc(
    double       i_max,
    double       i_min,
    double       t_attack,
    double       t_decay,
    double       sustain,
    double       t_release,
    int32_t      rate) {
  
  int32_t i_x = 0;
  int32_t i_n = 0;
  int32_t attack = 0;
  int32_t decay = 0;
  int32_t release = 0;
  ADSR_OBJ *pa = NULL;
  
  /* Check parameters */
  if ((!isfinite(i_max)) || (!isfinite(i_min)) ||
      (!isfinite(t_attack)) || (!isfinite(t_decay)) ||
      (!isfinite(sustain)) || (!isfinite(t_release))) {
    abort();
  }
  if (!((i_max > 0.0) && (i_max <= 1.0))) {
    abort();
  }
  if (!((i_min >= 0.0) && (i_min <= 1.0))) {
    abort();
  }
  if (!(i_min <= i_max)) {
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
  
  /* Compute integer i_max and i_min and clamp to range */
  i_x = (int32_t) (i_max * ((double) ADSR_MAXVAL));
  i_n = (int32_t) (i_min * ((double) ADSR_MAXVAL));
  
  if (i_x < 0) {
    i_x = 0;
  } else if (i_x > ADSR_MAXVAL) {
    i_x = ADSR_MAXVAL;
  }
  
  if (i_n < 0) {
    i_n = 0;
  } else if (i_n > ADSR_MAXVAL) {
    i_n = ADSR_MAXVAL;
  }
  
  /* Allocate object */
  pa = (ADSR_OBJ *) malloc(sizeof(ADSR_OBJ));
  if (pa == NULL) {
    abort();
  }
  memset(pa, 0, sizeof(ADSR_OBJ));
  
  /* Set variables */
  pa->i_max = (int16_t) i_x;
  pa->i_min = (int16_t) i_n;
  
  pa->env_samp = attack + decay + release + 1;
  pa->env_mid = attack + decay;
  
  /* Allocate buffer for cached envelope */
  pa->penv = (int16_t *) malloc(pa->env_samp * sizeof(int16_t));
  if (pa->penv == NULL) {
    abort();
  }
  memset(pa->penv, 0, pa->env_samp * sizeof(int16_t));
  
  /* @@TODO: */
  for(i_n = 0; i_n < pa->env_samp; i_n++) {
    (pa->penv)[i_n] = (int16_t) ADSR_MAXVAL;
  }
  
  /* Return object */
  return pa;
}

/*
 * adsr_free function.
 */
void adsr_free(ADSR_OBJ *pa) {
  if (pa != NULL) {
    free(pa->penv);
    free(pa);
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
    int32_t    intensity,
    int16_t    s) {
  
  int32_t result = 0;
  int32_t ival = 0;
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
  
  /* First of all, apply intensity */
  ival = ((intensity * ((int32_t) (pa->i_max - pa->i_min))) / 
              ADSR_MAXVAL) + ((int32_t) pa->i_min);
  result = (((int32_t) s) * ival) / ADSR_MAXVAL;
  
  if (result > INT16_MAX) {
    result = INT16_MAX;
  } else if (result < -(INT16_MAX)) {
    result = -(INT16_MAX);
  }
  
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
