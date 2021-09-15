/*
 * sqwave.c
 * 
 * Implementation of sqwave.h
 * 
 * See the header for further information.
 */

#include "sqwave.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * Define the total number of piano keys by the pitch range.
 */
#define SQWAVE_KEY_COUNT (PITCH_MAX - PITCH_MIN + 1)

/*
 * Define the key bias, which is what to add to pitch numbers to get
 * zero-indexed piano key offsets.
 */
#define SQWAVE_KEY_BIAS (-(PITCH_MIN))

/*
 * The maximum number of harmonics that will be used to generate a
 * square wave additively.
 * 
 * The first harmonic is the fundamental frequency.
 */
#define SQWAVE_MAX_HARMONICS (256)

/*
 * The minimum number of samples used for wavetable records.
 */
#define SQWAVE_MIN_SAMPLES (1024)

/*
 * The frequency limit for sine waves for each sampling rate.
 */
#define SQWAVE_FLIMIT_CD  (21000.0)
#define SQWAVE_FLIMIT_DVD (23000.0)

/*
 * Type declarations
 * =================
 */

/*
 * Structure definition of a record in the wave table.
 */
typedef struct {
  
  /*
   * The total number of samples recorded for this record.
   */
  int32_t sampcount;
  
  /*
   * Pointer to the samples for this record.
   */
  int16_t *psamp;
  
} SQWAVE_WAVREC;

/*
 * Structure definition of wave parameters.
 */
typedef struct {
  
  /*
   * The total number of samples for the wave.
   */
  int32_t samp_count;
  
  /*
   * The total number of waves.
   */
  int32_t wave_count;
  
  /*
   * The total number of (odd-numbered) harmonics.
   * 
   * One means a sine wave with no additional harmonics.  Two means one
   * additional harmonic over the fundamental, and so forth.
   */
  int32_t harmonics;
  
} SQWAVE_WAVPARAM;

/*
 * Static data
 * ===========
 */

/*
 * The initialization flag.
 */
static int m_sqwave_init = 0;

/*
 * The wave table.
 * 
 * Only valid if m_sqwave_init is non-zero.
 */
static SQWAVE_WAVREC m_sqwave_table[SQWAVE_KEY_COUNT];

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void sqwave_sinewave(
    int16_t * ps,
    int32_t   samp_count,
    int32_t   wave_count,
    double    amp);
static void sqwave_squarewave(
    int16_t * ps,
    int32_t   samp_count,
    int32_t   wave_count,
    double    amp,
    int32_t   harmonics);

static void sqwave_param(
    SQWAVE_WAVPARAM * pwp,
    double            freq,
    double            rate,
    double            flimit,
    int32_t           max_hcount,
    int32_t           min_scount);

/*
 * Add a sine wave to the given sample array.
 * 
 * ps points to the sample array.  samp_count is the number of samples
 * in the array, which must be greater than one.  There is only one
 * channel in the sample array.
 * 
 * wave_count is the number of full sine wave periods to include in the
 * samples.  It must be greater than zero.
 * 
 * amp is what to multiply each normalized sine wave by to get the
 * integer value.  It must be finite.
 * 
 * The generated samples are added to the samples that are already 
 * the array.
 * 
 * Parameters:
 * 
 *   ps - pointer to the sample array
 * 
 *   samp_count - the number of samples in the sample array
 * 
 *   wave_count - the number of wave periods to write
 * 
 *   amp - the amplitude of the sine wave
 */
static void sqwave_sinewave(
    int16_t * ps,
    int32_t   samp_count,
    int32_t   wave_count,
    double    amp) {
  
  int32_t x = 0;
  int32_t i = 0;
  double sv = 0.0;
  double mult = 0.0;
  
  /* Check parameters */
  if (ps == NULL) {
    abort();
  }
  if ((samp_count < 2) || (wave_count < 1)) {
    abort();
  }
  if (!isfinite(amp)) {
    abort();
  }
  
  /* Precompute the multiplier */
  mult = (2.0 * M_PI * ((double) wave_count)) /
            ((double) samp_count);
  
  /* Generate the sine wave */
  for(x = 0; x < samp_count; x++) {
    
    /* Get the sine wave value */
    sv = sin(((double) x) * mult);
    
    /* Quantize */
    i = (int32_t) (sv * amp);
    
    /* Add in existing sample */
    i = i + ((int32_t) ps[x]);
    
    /* Clamp to range */
    if (i > INT16_MAX) {
      i = INT16_MAX;
    } else if (i < -(INT16_MAX)) {
      i = -(INT16_MAX);
    }
    
    /* Update sample */
    ps[x] = (int16_t) i;
  }
}

/*
 * Add a square wave to the given sample array.
 * 
 * This is a wrapper around sqwave_sinewave.  See that function for a
 * description of the ps, samp_count, wave_count, and amp parameters.
 * 
 * The square wave is constructed as multiple calls to sinewave, each
 * adding a different harmonic of the square wave.  Square waves only
 * include odd-numbered harmonics.
 * 
 * harmonics indicates how many harmonics to add.  It must be greater
 * than zero.  If it is one, the result is the same as a sine wave.
 * Higher numbers get closer to a square wave.
 * 
 * This function does not check whether harmonics are below the Nyquist
 * limit.  Aliasing problems will occur if this limit is exceeded.
 * Undefined behavior occurs if the harmonics value is very large.
 * 
 * Parameters:
 * 
 *   ps - pointer to the sample array
 * 
 *   samp_count - the number of samples in the sample array
 * 
 *   wave_count - the number of wave periods to write
 * 
 *   amp - the amplitude of the sine wave
 * 
 *   harmonics - the number of harmonics to add
 */
static void sqwave_squarewave(
    int16_t * ps,
    int32_t   samp_count,
    int32_t   wave_count,
    double    amp,
    int32_t   harmonics) {
  
  int32_t h = 0;
  int32_t m = 0;
  
  /* Check harmonics, wave_count, and amp parameters */
  if ((harmonics < 1) || (wave_count < 1)) {
    abort();
  }
  if (!isfinite(amp)) {
    abort();
  }
  
  /* Add harmonics */
  for(h = 0; h < harmonics; h++) {
    
    /* Get multiplier */
    m = (h * 2) + 1;
    
    /* Call through */
    sqwave_sinewave(
        ps,
        samp_count,
        wave_count * m,
        (4.0 / (M_PI * ((double) m))) * amp);
  }
}

/*
 * Fill in a square wave parameter structure with properly computed
 * values.
 * 
 * pwp points to the square wave parameter structure to fill in.
 * 
 * freq is the frequency of the wave, in Hz.
 * 
 * rate is the sampling rate, in Hz.
 * 
 * flimit is the maximum frequency allowed for harmonic overtones, in
 * Hz.  It should be no greater than the Nyquist limit (half the
 * sampling rate).
 * 
 * freq, rate, and flimit must all be finite and greater than zero.
 * Additionally, freq must be less than or equal to flimit, and flimit
 * must be less than or equal to rate.
 * 
 * max_hcount is the maximum number of (odd-numbered) harmonics.  It
 * must be greater than zero.  If it is one, all generated waves will
 * just be sine waves.  If it is two, all generated waves will have at
 * most one additional harmonic sine wave added, and so forth.
 * 
 * min_scount is the minimum number of samples that should be generated.
 * It must be greater than zero.  Undefined behavior occurs if a huge
 * value is provided that is close to the limit of a signed 32-bit
 * integer.
 * 
 * The number of harmonics is determined by adding odd-numbered
 * harmonics until either the flimit frequency limit or the max_hcount
 * harmonic limit is reached (whichever occurs first).
 * 
 * To determine the sample count and the wave count, the period of the
 * wave is determined, in samples.  If this is less than min_scount,
 * additional waves are added using wave_count to bring the total number
 * of samples above min_scount.  Otherwise, the sample count is set to
 * the period and the wave_count is set to one.
 * 
 * Parameters:
 * 
 *   pwp - the parameter structure to fill in
 * 
 *   freq - the frequency of the square wave in Hz
 * 
 *   rate - the sampling rate in Hz
 * 
 *   flimit - the frequency limit in Hz
 * 
 *   max_hcount - the maximum number of harmonics
 * 
 *   min_scount - the minimum number of samples
 */
static void sqwave_param(
    SQWAVE_WAVPARAM * pwp,
    double            freq,
    double            rate,
    double            flimit,
    int32_t           max_hcount,
    int32_t           min_scount) {
  
  int32_t h = 0;
  double f = 0.0;
  double p = 0.0;
  double wc = 0.0;
  
  /* Check parameters */
  if (pwp == NULL) {
    abort();
  }
  if ((!isfinite(freq)) || (!isfinite(rate)) || (!isfinite(flimit))) {
    abort();
  }
  if ((!(freq > 0.0)) || (!(rate > 0.0)) || (!(flimit > 0.0))) {
    abort();
  }
  if ((!(freq <= flimit)) || (!(flimit <= rate))) {
    abort();
  }
  if ((max_hcount < 1) || (min_scount < 1)) {
    abort();
  }
  
  /* Reset the structure */
  memset(pwp, 0, sizeof(SQWAVE_WAVPARAM));
  
  /* Determine number of harmonics */
  for(h = 1; h <= max_hcount; h++) {
    
    /* Compute frequency */
    f = freq * ((double) (((h - 1) * 2) + 1));
    if (!isfinite(f)) {
      abort();
    }
    
    /* If frequency is above the limit, leave loop */
    if (f > flimit) {
      h--;
      break;
    }
  }
  
  /* Clamp h to valid range */
  if (h > max_hcount) {
    h = max_hcount;
  } else if (h < 1) {
    h = 1;
  }
  
  /* Write the number of harmonics */
  pwp->harmonics = h;
  
  /* Compute the period in samples */
  p = rate / freq;
  
  /* If period is less than minimum number of samples, compute wave
   * count; else, set wave count to 1.0 */
  if (p < (double) min_scount) {
    /* Compute number of periods and add one */
    wc = floor(((double) min_scount) / p) + 1.0;
    
  } else {
    /* Single period is long enough */
    wc = 1.0;
  }
  
  /* Set the sample count and wave count */
  pwp->samp_count = (int32_t) (p * wc);
  pwp->wave_count = (int32_t) wc;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * sqwave_init function.
 */
void sqwave_init(double amp, int32_t samprate) {
  
  int32_t i = 0;
  SQWAVE_WAVPARAM wp;
  double flim = 0.0;
  
  /* Initialize structures */
  memset(&wp, 0, sizeof(SQWAVE_WAVPARAM));
  
  /* Check state */
  if (m_sqwave_init) {
    abort();
  }
  
  /* Check parameters */
  if (!isfinite(amp)) {
    abort();
  }
  if (!(amp > 0.0)) {
    abort();
  }
  if ((samprate != RATE_CD) && (samprate != RATE_DVD)) {
    abort();
  }
  
  /* Clamp amplitude */
  if (amp < SQWAVE_AMP_MIN) {
    amp = SQWAVE_AMP_MIN;
  } else if (amp > SQWAVE_AMP_MAX) {
    amp = SQWAVE_AMP_MAX;
  }
  
  /* Set initialization flag */
  m_sqwave_init = 1;
  
  /* Clear wave table */
  memset(m_sqwave_table, 0, SQWAVE_KEY_COUNT * sizeof(SQWAVE_WAVREC));
  
  /* Determine frequency limit depending on sample rate */
  if (samprate == RATE_CD) {
    flim = SQWAVE_FLIMIT_CD;
  } else if (samprate == RATE_DVD) {
    flim = SQWAVE_FLIMIT_DVD;
  } else {
    /* Unrecognized sample rate */
    abort();
  }
  
  /* Initialize wave table */
  for(i = 0; i < SQWAVE_KEY_COUNT; i++) {
    
    /* Determine wave parameters for this entry */
    sqwave_param(
        &wp,
        pitchfreq(i + PITCH_MIN),
        samprate,
        flim,
        SQWAVE_MAX_HARMONICS,
        SQWAVE_MIN_SAMPLES);
    
    /* Set the sample count in the wave table */
    (m_sqwave_table[i]).sampcount = wp.samp_count;
    
    /* Allocate memory for the wave and clear the samples */
    (m_sqwave_table[i]).psamp = (int16_t *) malloc(
                                  wp.samp_count * sizeof(int16_t));
    if ((m_sqwave_table[i]).psamp == NULL) {
      abort();
    }
    memset(
      (m_sqwave_table[i]).psamp,
      0,
      wp.samp_count * sizeof(int16_t));
    
    /* Write the square wave */
    sqwave_squarewave(
      (m_sqwave_table[i]).psamp,
      wp.samp_count,
      wp.wave_count,
      amp,
      wp.harmonics);
  }
}

/*
 * sqwave_get function.
 */
int16_t sqwave_get(int32_t pitch, int32_t t) {
  
  int32_t key = 0;
  
  /* Check state */
  if (!m_sqwave_init) {
    abort();
  }
  
  /* Check parameters */
  if ((pitch < PITCH_MIN) || (pitch > PITCH_MAX) ||
      (t < 0)) {
    abort();
  }
  
  /* Get the key offset from the pitch */
  key = pitch + SQWAVE_KEY_BIAS;

  /* Adjust t with modulus so the sample loops if necessary */
  t = t % (m_sqwave_table[key]).sampcount;
  
  /* Get the requested sample */
  return ((m_sqwave_table[key]).psamp)[t];
}
