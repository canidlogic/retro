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
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * adsr_init function.
 */
void adsr_init(
    ADSR_PARAM * pa,
    double       i_max,
    double       i_min,
    double       t_attack,
    double       t_decay,
    double       sustain,
    double       t_release,
    int32_t      rate) {
  
  int32_t attack = 0;
  int32_t decay = 0;
  int32_t release = 0;
  
  /* Check parameters */
  if (pa == NULL) {
    abort();
  }
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
  
  /* Clear the structure and write the values */
  memset(pa, 0, sizeof(ADSR_PARAM));
  
  pa->i_max = i_max;
  pa->i_min = i_min;
  pa->sustain = sustain;
  pa->attack = attack;
  pa->decay = decay;
  pa->release = release;
}

/*
 * adsr_extra function.
 */
int32_t adsr_extra(const ADSR_PARAM *pa) {
  
  if (pa == NULL) {
    abort();
  }
  return pa->release;
}

/*
 * adsr_mul function.
 */
int16_t adsr_mul(
    const ADSR_PARAM * pa,
          int32_t      t,
          int32_t      dur,
          double       intensity,
          int16_t      s) {
  /* @@TODO: */
  intensity = (intensity * 0.75) + 0.25;
  s = (int16_t) (((double) s) * intensity);
  return s;
}
