/*
 * sqwave.c
 * 
 * Implementation of sqwave.h
 * 
 * See the header for further information.
 */

#include "sqwave.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * Static data
 * ===========
 */

/*
 * Flag indicated whether the module has been initialized.
 */
static int m_sqwave_init = 0;

/*
 * Half the window length.
 * 
 * Only valid if m_sqwave_init is non-zero.
 */
static int32_t m_sqwave_winhlen = 0;

/*
 * Pointer to the upper half of the windowed filter.
 * 
 * Only valid if m_sqwave_init is non-zero.
 * 
 * The length of this array is m_sqwave_winhlen.
 */
static int16_t *m_sqwave_pfilter = NULL;

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static int sqwave_raw(int32_t fnum, int32_t fdenom, int32_t t);
static double sqwave_sincf(int32_t t, double cutoff);
static double sqwave_filterf(int32_t n, int32_t nmax, double cutoff);

/*
 * Compute the raw, unfiltered square wave value for a given frequency
 * at a given time t.
 * 
 * The square wave module must be initialized before using this
 * function.
 * 
 * fnum and fdenom must both be greater than zero.  They indicate the
 * frequency *in samples* (not in seconds) as a rational number.
 * 
 * t is the time offset in samples.  It must be greater than or equal to
 * (-winhlen), where winhlen is half the window length.  This allows
 * negative values of t to be queried when computing the filtered value
 * at t=0.
 * 
 * The return value is either +1, zero, or -1.  If fnum is less than
 * floor(fdenom/2), then the value will be either +1 or -1, according to
 * the raw square wave.  Otherwise, the value will be zero because the
 * frequency is near or above the Nyquist limit.
 * 
 * To avoid aliasing interference, the raw square wave must be filtered
 * before it is written to a sound file.
 * 
 * Parameters:
 * 
 *   fnum - the numerator of the frequency in samples
 * 
 *   fdenom - the denominator of the frequency in samples
 * 
 *   t - the time offset in samples
 * 
 * Return:
 * 
 *   +1 or -1 according to the raw square wave, or zero if the frequency
 *   is near or above the Nyquist limit
 */
static int sqwave_raw(int32_t fnum, int32_t fdenom, int32_t t) {
  
  int result = 0;
  int64_t hp = 0;
  
  /* Check state */
  if (!m_sqwave_init) {
    abort();
  }
  
  /* Check parameters */
  if ((fnum < 1) || (fdenom < 1) || (t < -(m_sqwave_winhlen))) {
    abort();
  }
  
  /* Only proceed if below the Nyquist limit */
  if (fnum < fdenom / 2) {
    
    /* Add half the window length to t so that t is always zero or
     * greater */
    t += m_sqwave_winhlen;
    
    /* Compute the number of elapsed half-periods */
    hp = (((int64_t) fnum) * 2 * ((int64_t) t)) / ((int64_t) fdenom);
    
    /* Result according to least significant bit */
    if (hp & 0x1) {
      result = 1;
    } else {
      result = -1;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Compute the non-windowed sinc filter function at time t with a given
 * cutoff value.
 * 
 * cutoff must be finite and in range (0, 1.0).  t may be any value.
 * 
 * A fault occurs if the computed result is non-finite (indicating
 * numerical problems).
 * 
 * Parameters:
 * 
 *   t - the time value
 * 
 *   cutoff - the cutoff as a fraction of the Nyquist limit
 * 
 * Return:
 * 
 *   the non-windowed since filter value
 */
static double sqwave_sincf(int32_t t, double cutoff) {
  
  double result = 0.0;
  double tv = 0.0;
  
  /* Check parameters */
  if (!isfinite(cutoff)) {
    abort();
  }
  if (!((cutoff > 0.0) && (cutoff < 1.0))) {
    abort();
  }

  /* Compute the value */
  if (t != 0) {
    tv = (double) t;
    result = (sin(M_PI * tv * cutoff) / (M_PI * tv * cutoff)) * cutoff;
  } else {
    result = 1.0;
  }
  
  /* Check if finite */
  if (!isfinite(result)) {
    abort();  /* numerical problems */
  }
  
  /* Return result */
  return result;
}

/*
 * Computed the Hann-windowed sinc filter at a given filter point for a
 * given cutoff.
 * 
 * nmax must be greater than one and even.  n must be greater than or
 * equal to zero and less than or equal to nmax.  (nmax + 1) is the
 * total number of filter taps, and n is the tap number.
 * 
 * cutoff is passed through to sqwave_sincf to compute the underlying
 * sinc filter function.  It must be finite and in range (0, 1.0).
 * 
 * The function is symmetric about the halfway point.
 * 
 * Parameters:
 * 
 *   n - the tap number
 * 
 *   nmax - the maximum tap number
 * 
 *   cutoff - the cutoff as a fraction of the Nyquist limit
 * 
 * Return:
 * 
 *   the windowed filter value
 */
static double sqwave_filterf(int32_t n, int32_t nmax, double cutoff) {
  
  double result = 0.0;
  double hannv = 0.0;
  int32_t t = 0;

  /* Check integer parameters */
  if ((nmax <= 1) || (n < 0) || (n > nmax) || (nmax & 0x1 == 1)) {
    abort();
  }
  
  /* Determine the Hann window value at n and verify that it is
   * finite */
  hannv = sin((M_PI * ((double) n)) / ((double) nmax));
  hannv = hannv * hannv;
  if (!isfinite(hannv)) {
    abort();  /* numerical problems */
  }
  
  /* Determine the t value for the underlying sinc filter, which has its
   * t=0 value centered in the middle of the window */
  t = n - (nmax / 2.0);
  
  /* The result is the Hann window value multiplied to the underlying
   * sinc filter value */
  result = hannv * sqwave_sincf(t, cutoff);
  
  /* Verify result is finite */
  if (!isfinite(result)) {
    abort();
  }
  
  /* Return result */
  return result;
}

/*
 * Public functions
 * ================
 * 
 * See header for specifications.
 */

/*
 * sqwave_init function.
 */
void sqwave_init(
    int32_t winhlen,
    double  cutoff,
    int32_t sampnorm) {
  
  int32_t x = 0;
  int32_t i = 0;
  double fv = 0.0;
  
  /* Check state */
  if (m_sqwave_init) {
    abort();
  }
  
  /* Check parameters */
  if ((winhlen < SQWAVE_HLEN_MIN) || (winhlen > SQWAVE_HLEN_MAX)) {
    abort();
  }
  if ((sampnorm < SQWAVE_SNORM_MIN) || (sampnorm > SQWAVE_SNORM_MAX)) {
    abort();
  }
  if (!isfinite(cutoff)) {
    abort();
  }

  /* Clamp cutoff value */
  if (cutoff < SQWAVE_CUTOFF_MIN) {
    cutoff = SQWAVE_CUTOFF_MIN;
  } else if (cutoff > SQWAVE_CUTOFF_MAX) {
    cutoff = SQWAVE_CUTOFF_MAX;
  }
  
  /* Set initialization flag and copy in winhlen */
  m_sqwave_init = 1;
  m_sqwave_winhlen = winhlen;
  
  /* Allocate a buffer for the filter values */
  m_sqwave_pfilter = (int16_t *) malloc(
                        ((size_t) (winhlen + 1)) * sizeof(int16_t));
  if (m_sqwave_pfilter == NULL) {
    abort();
  }
  memset(m_sqwave_pfilter, 0,
          ((size_t) (winhlen + 1)) * sizeof(int16_t));
  
  /* Compute all the filter values */
  for(x = 0; x <= winhlen; x++) {
    
    /* Get the filter value */
    fv = sqwave_filterf(x + winhlen, (winhlen * 2), cutoff);
    
    /* Quantize */
    i = (int32_t) (fv * ((double) sampnorm));
    if (i > INT16_MAX) {
      i = INT16_MAX;
    } else if (i < -(INT16_MAX)) {
      i = -(INT16_MAX);
    }
    
    /* Write to array */
    m_sqwave_pfilter[x] = (int16_t) i;
  }
}

/*
 * sqwave_get function.
 */
int16_t sqwave_get(int32_t fnum, int32_t fdenom, int32_t t) {
  
  int32_t result = 0;
  int32_t x = 0;
  int32_t fv = 0;
  int v = 0;
  
  /* Check state */
  if (!m_sqwave_init) {
    abort();
  }
  
  /* Check parameters */
  if ((fnum < 1) || (fdenom < 1) ||
      (t < 0) || (t > INT32_MAX - SQWAVE_HLEN_MAX)) {
    abort();
  }
  
  /* Start with the value right at t */
  v = sqwave_raw(fnum, fdenom, t);
  if (v > 0) {
    result = result + ((int32_t) m_sqwave_pfilter[0]);
  } else if (v < 0) {
    result = result - ((int32_t) m_sqwave_pfilter[0]);
  } else if (v == 0) {
    result = 0;
  } else {
    abort();  /* shouldn't happen */
  }
  
  /* Only compute the rest of the terms if the v return was non-zero */
  if (v != 0) {
    
    /* Add in the rest of the filter values symmetrically */
    for(x = 1; x <= m_sqwave_winhlen; x++) {
      
      /* Get filter value */
      fv = (int32_t) m_sqwave_pfilter[x];
      
      /* Add the upper value */
      v = sqwave_raw(fnum, fdenom, (t + x));
      if (v > 0) {
        result = result + fv;
      } else if (v < 0) {
        result = result - fv;
      } else {
        abort();  /* shouldn't happen */
      }
      
      /* Add the lower value */
      v = sqwave_raw(fnum, fdenom, (t - x));
      if (v > 0) {
        result = result + fv;
      } else if (v < 0) {
        result = result - fv;
      } else {
        abort();  /* shouldn't happen */
      }
    }
  }
  
  /* Clamp the result to 16-bit range */
  if (result > INT16_MAX) {
    result = INT16_MAX;
  } else if (result < -(INT16_MAX)) {
    result = -(INT16_MAX);
  }
  
  /* Return the result */
  return (int16_t) result;
}
