/*
 * adsr.c
 * 
 * Implementation of adsr.h
 * 
 * See the header for further information.
 */

#include "adsr.h"

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
  /* @@TODO: */
}

/*
 * adsr_extra function.
 */
int32_t adsr_extra(const ADSR_PARAM *pa) {
  /* @@TODO: */
  return 0;
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
