/*
 * ttone.c
 * =======
 * 
 * Implementation of ttone.h
 * 
 * See the header for further information.
 */

#include "ttone.h"
#include <stdlib.h>

/*
 * The total number of piano keys.
 */
#define KEY_COUNT (PITCH_MAX - PITCH_MIN + 1)

/*
 * Define the key bias, which is what to add to pitch numbers to get
 * zero-indexed piano key offsets.
 */
#define KEY_BIAS (-(PITCH_MIN))

/*
 * The frequencies of each piano key.
 * 
 * These are from the Wikipedia article "Piano key frequencies" accessed
 * on 2019-12-28.  They are generated according to 12-tone equal
 * temperament.
 */
static double M_FREQ[KEY_COUNT] = {
  
  /* A''' Bb''' B''' */
  27.50000, 29.13524, 30.86771,
  
  /* C''-B'' octave */
  32.70320, 34.64783, 36.70810, 38.89087,
  41.20344, 43.65353, 46.24930, 48.99943,
  51.91309, 55.00000, 58.27047, 61.73541,
  
  /* C'-B' octave */
  65.40639, 69.29566, 73.41619, 77.78175,
  82.40689, 87.30706, 92.49861, 97.99886,
  103.8262, 110.0000, 116.5409, 123.4708,
  
  /* C-B octave */
  130.8128, 138.5913, 146.8324, 155.5635,
  164.8138, 174.6141, 184.9972, 195.9977,
  207.6523, 220.0000, 233.0819, 246.9417,
  
  /* c-b octave (middle C octave) */
  261.6256, 277.1826, 293.6648, 311.1270,
  329.6276, 349.2282, 369.9944, 391.9954,
  415.3047, 440.0000, 466.1638, 493.8833,
  
  /* c'-b' octave */
  523.2511, 554.3653, 587.3295, 622.2540,
  659.2551, 698.4565, 739.9888, 783.9909,
  830.6094, 880.0000, 932.3275, 987.7666,
  
  /* c''-b'' octave */
  1046.502, 1108.731, 1174.659, 1244.508,
  1318.510, 1396.913, 1479.978, 1567.982,
  1661.219, 1760.000, 1864.555, 1975.533,
  
  /* c'''-b''' octave and c'''' */
  2093.005, 2217.461, 2349.318, 2489.016,
  2637.020, 2793.826, 2959.955, 3135.963,
  3322.438, 3520.000, 3729.310, 3951.066, 4186.009
};

/*
 * pitchfreq function.
 * 
 * See the header for specification.
 */
double pitchfreq(int32_t pitch) {
  
  /* Check parameter */
  if ((pitch < PITCH_MIN) || (pitch > PITCH_MAX)) {
    abort();
  }

  /* Return the frequency by table lookup */
  return M_FREQ[pitch + KEY_BIAS];
}
