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
   * The reference count of the object.
   */
  int32_t refcount;
  
  /*
   * The number of samples for the attack.
   * 
   * This is in range [0, ADSR_MAXTIME].
   */
  int32_t attack;
  
  /*
   * The number of samples for the decay.
   * 
   * This is in range [0, ADSR_MAXTIME].
   * 
   * If sustain is MAX_FRAC, then this must be zero.
   */
  int32_t decay;
  
  /*
   * The sustain level.
   * 
   * This is in range [0, MAX_FRAC].
   */
  int32_t sustain;
  
  /*
   * The number of samples for the release.
   * 
   * This is in range [0, ADSR_MAXTIME].
   */
  int32_t release;
};

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
  if ((rate != RATE_DVD) && (rate != RATE_CD)) {
    abort();
  }
  
  /* If sustain is special value 1.0, then there is no decay */
  if (sustain == 1.0) {
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
  v_s = (int32_t) (sustain * ((double) MAX_FRAC));
  if (v_s < 0) {
    v_s = 0;
  } else if (v_s > MAX_FRAC) {
    v_s = MAX_FRAC;
  }
  
  /* Allocate object */
  pa = (ADSR_OBJ *) malloc(sizeof(ADSR_OBJ));
  if (pa == NULL) {
    abort();
  }
  memset(pa, 0, sizeof(ADSR_OBJ));
  
  /* Set variables */
  pa->refcount = 1;
  pa->attack = attack;
  pa->decay = decay;
  pa->sustain = v_s;
  pa->release = release;
  
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
  
  /* The duration is dur plus any release samples */
  d = ((int64_t) dur) + ((int64_t) pa->release);
  
  /* Truncate to signed 32-bit integer length if necessary */
  if (d > INT32_MAX) {
    d = INT32_MAX;
  }
  
  /* Return the computed duration */
  return (int32_t) d;
}

/*
 * adsr_compute function.
 */
int32_t adsr_compute(ADSR_OBJ *pa, int32_t t, int32_t dur) {
  
  int32_t mv = 0;
  int32_t offset = 0;
  int32_t scale = 0;
  
  /* Check parameters */
  if (pa == NULL) {
    abort();
  }
  if ((t < 0) || (dur < 1)) {
    abort();
  }
  
  /* Determine the envelope multiplier value depending on where in the
   * envelope we are */
  if (t >= dur) {
    /* Beyond the duration, so we are releasing -- compute offset */
    offset = t - dur;
    
    /* Check whether beyond envelope */
    if (offset >= pa->release) {
      /* Beyond the envelope, so multiplier is just zero */
      mv = 0;
    
    } else {
      /* Not beyond the envelope; recursively determine the scale of the
       * release from the envelope multiplier just before the release
       * period */
      scale = adsr_compute(pa, dur - 1, dur);
    
      /* Compute the envelope multiplier */
      mv = (int32_t)
            ((((int64_t) (pa->release - offset)) * ((int64_t) scale)) /
              ((int64_t) pa->release));
    }
  
  } else if (t < pa->attack) {
    /* During the attack -- offset is t */
    offset = t;
    
    /* Compute the envelope multiplier */
    mv = (int32_t) ((((int64_t) offset) * ((int64_t) MAX_FRAC)) /
                      ((int64_t) pa->attack));
    
  } else if (t < pa->attack + pa->decay) {
    /* During the decay -- compute the offset */
    offset = t - pa->attack;
    
    /* Compute the envelope multiplier */
    mv = (int32_t) (((((int64_t) (pa->decay - offset)) *
                      ((int64_t) (MAX_FRAC - pa->sustain))) /
                    ((int64_t) pa->decay)) + pa->sustain);
    
  } else {
    /* During the sustain -- just use sustain level */
    mv = pa->sustain;
  }
  
  /* Return the multiplier value */
  return mv;
}

/*
 * adsr_mul function.
 */
int16_t adsr_mul(
    ADSR_OBJ * pa,
    int32_t    t,
    int32_t    dur,
    int16_t    s) {
  
  int32_t mv = 0;
  int32_t result = 0;
  
  /* Compute the envelope multiplier */
  mv = adsr_compute(pa, t, dur);
  
  /* Multiply input sample by multiplier */
  result = (((int32_t) s) * mv) / MAX_FRAC;
  
  /* Clamp result */
  if (result < -(INT16_MAX)) {
    result = -(INT16_MAX);
  } else if (result > INT16_MAX) {
    result = INT16_MAX;
  }
  
  /* Return result */
  return (int16_t) result;
}
