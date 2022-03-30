/*
 * adsr.c
 * ======
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
   * This is zero or greater.
   */
  int32_t attack;
  
  /*
   * The number of samples for the decay.
   * 
   * This is zero or greater.
   */
  int32_t decay;
  
  /*
   * The number of samples for the release.
   * 
   * This is zero or greater.
   */
  int32_t release;
  
  /*
   * The number of samples for the sustain fade limit.
   * 
   * This is zero or greater.
   */
  int32_t limit;
  
  /*
   * The peak multiplier.
   * 
   * This is finite and greater than zero.
   */
  double peak;
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
    int32_t t_attack,
    int32_t t_decay,
    int32_t t_release,
    int32_t t_limit,
    double  peak) {
  
  ADSR_OBJ *pa = NULL;
  
  /* Check parameters */
  if ((t_attack < 0) || (t_decay < 0) ||
      (t_release < 0) || (t_limit < 0)) {
    abort();
  }
  if (!isfinite(peak)) {
    abort();
  }
  if (!(peak > 0.0)) {
    abort();
  }
  
  /* Allocate object */
  pa = (ADSR_OBJ *) malloc(sizeof(ADSR_OBJ));
  if (pa == NULL) {
    abort();
  }
  memset(pa, 0, sizeof(ADSR_OBJ));
  
  /* Set variables */
  pa->refcount = 1;
  pa->attack   = t_attack;
  pa->decay    = t_decay;
  pa->release  = t_release;
  pa->limit    = t_limit;
  pa->peak     = peak;
  
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
  
  int32_t x = 0;
  int32_t r = 0;
  
  /* Check parameters */
  if ((pa == NULL) || (dur < 1)) {
    abort();
  }
  
  /* Get a local copy of the release time */
  r = pa->release;
  
  /* If there is a fade limit, then clamp dur to a maximum of the attack
   * plus decay plus limit and clear the local copy of the release
   * time to zero */
  if (pa->limit > 0) {
    x = dur;
    if (x > pa->attack) {
      x = x - pa->attack;
      if (x > pa->decay) {
        x = x - pa->decay;
        if (x > pa->limit) {
          dur = pa->attack + pa->decay + pa->limit;
          r = 0;
        }
      }
    }
  }
  
  /* Add release time to duration, clamping to maximum of INT32_MAX */
  if (dur <= INT32_MAX - r) {
    dur += r;
  } else {
    dur = INT32_MAX;
  }
  
  /* Return the adjusted duration */
  return dur;
}

/*
 * adsr_compute function.
 */
double adsr_compute(ADSR_OBJ *pa, int32_t t, int32_t dur) {
  
  double lv = 0.0;
  double result = 0.0;
  
  /* Check parameters */
  if (pa == NULL) {
    abort();
  }
  if ((t < 0) || (dur < 1)) {
    abort();
  }
  
  /* Check which phase we are in */
  if (t >= dur) {
    /* Release phase, so first compute the last envelope value before
     * the release phase */
    lv = adsr_compute(pa, dur - 1, dur);
    
    /* Scale last value appropriately */
    result = lv * (1.0 - (((double) (t - dur + 1))
                            / (((double) pa->release) + 1.0)));
    
  } else if (t < pa->attack) {
    /* t is in attack phase */
    result = (((double) (t + 1)) / ((double) pa->attack)) * pa->peak;
    
  } else if (t - pa->attack < pa->decay) {
    /* t is in decay phase (attack <= t < attack + decay) */
    result = 1.0
              + ((pa->peak - 1.0)
                  * (1.0
                      - (((double) (t - pa->attack + 1))
                        / (((double) pa->decay) + 1.0))));
    
  } else if (pa->limit > 0) {
    /* t is in sustain phase and there is a fading limit */
    result = 1.0
              - (((double) (t - pa->attack - pa->decay))
                  / ((double) pa->limit));
    
  } else {
    /* If we got here, we are in the sustain phase and there is no
     * fading limit, so just return 1.0 */
    result = 1.0;
  }
  
  /* Return the computed result */
  return result;
}
