#ifndef ADSR_H_INCLUDED
#define ADSR_H_INCLUDED

/*
 * adsr.h
 * 
 * The ADSR (Attack-Decay-Sustain-Release) module of the Retro synth.
 * 
 * Compilation
 * ===========
 * 
 * May require the math library to be included (-lm).
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum value for ADSR time durations.
 */
#define ADSR_MAXTIME (INT32_C(100000000))

/*
 * The ADSR parameters structure.
 * 
 * Clients should not access the internal fields directly.  Use the
 * functions provided by this module instead.
 */
typedef struct {
  
  /*
   * The maximum intensity multiplier for the instrument.
   * 
   * This is a finite value in range (0.0, 1.0].  It must be greater
   * than or equal to i_min.  If i_max and i_min are the same, then the
   * instrument will have constant intensity regardless of the input
   * intensity.
   */
  double i_max;
  
  /*
   * The minimum intensity multiplier for the instrument.
   * 
   * This is a finite value in range [0.0, 1.0].  It must be less than
   * or equal to i_max.  If i_max and i_min are the same, then the
   * instrument will have constant intensity regardless of the input
   * intensity.
   */
  double i_min;
  
  /*
   * The sustain multiplier for the instrument.
   * 
   * This is a finite value in range [0.0, 1.0].
   * 
   * If the sustain is 1.0, then the decay must be zero, since there is
   * no difference between the attack intensity and the sustain
   * intensity in this case.
   * 
   * If the sustain is 0.0, then the release must be zero, since the
   * sound has already finished by the time the release begins.
   */
  double sustain;
  
  /*
   * The attack duration, in samples.
   * 
   * This must be in range [0, ADSR_MAXTIME].
   */
  int32_t attack;
  
  /*
   * The decay duration, in samples.
   * 
   * This must be in range [0, ADSR_MAXTIME].
   * 
   * If the sustain value is 1.0, then the decay duration must be zero.
   */
  int32_t decay;
  
  /*
   * The release duration, in samples.
   * 
   * This must be in range [0, ADSR_MAXTIME].
   * 
   * If the sustain value is 0.0, then the release duration must be
   * zero.
   */
  int32_t release;
  
} ADSR_PARAM;

/*
 * Initialize an ADSR parameter structure.
 * 
 * This must be called before an ADSR_PARAM structure can be used with
 * the other functions of this module.  The structure need not be
 * cleaned up in any way before being released, and it can be re-used
 * by simply re-initializing it.
 * 
 * pa points to the structure to initialize.
 * 
 * i_max and i_min are the maximum and minimum intensity multipliers for
 * the instrument.  Both must be finite values in range [0.0, 1.0].
 * Additionally, i_max must be greater than or equal to i_min, and i_max
 * may not be 0.0.
 * 
 * When a specific intensity is requested for the ADSR envelope, it will
 * first be mapped to the range defined by i_max and i_min, and then it
 * will be multiplied against the input sample value to transform the
 * intensity.  If i_max and i_min are equal, then the input sample will
 * always be transformed by a constant multiplier, regardless of what
 * the given intensity for a particular note is.
 * 
 * The t_attack, t_decay, and t_release parameters are the times, in
 * milliseconds, of the attack, decay, and release periods.  They must
 * be finite and greater than or equal to zero.  If when they are
 * transformed into a sample count this value would exceed ADSR_MAXTIME,
 * the duration is shortened to ADSR_MAXTIME.
 * 
 * sustain is the sustain level multiplier.  It must be finite and in
 * range [0.0, 1.0].  If it is 1.0, then t_decay is ignored and the
 * decay is set to zero.  If it is 0.0, then t_release is ignored and
 * the release is set to zero.
 * 
 * rate is the sampling rate, in Hz.  It must be either 44100 or 48000.
 * 
 * Parameters:
 * 
 *   pa - pointer to the structure to initialize
 * 
 *   i_max - the maximum intensity multiplier
 * 
 *   i_min - the minimum intensity multiplier
 * 
 *   t_attack - the attack duration, in milliseconds
 * 
 *   t_decay - the decay duration, in milliseconds
 * 
 *   sustain - the sustain level multiplier
 * 
 *   t_release - the release duration, in milliseconds
 * 
 *   rate - the sampling rate, in Hz
 */
void adsr_init(
    ADSR_PARAM * pa,
    double       i_max,
    double       i_min,
    double       t_attack,
    double       t_decay,
    double       sustain,
    double       t_release,
    int32_t      rate);

/*
 * Get the number of extra samples that should be added to each note
 * duration to account for the release time.
 * 
 * The return value is in range [0, ADSR_MAXTIME].
 * 
 * Parameters:
 * 
 *   pa - pointer to the initialized ADSR structure
 * 
 * Return:
 * 
 *   the number of extra samples
 */
int32_t adsr_extra(const ADSR_PARAM *pa);

/*
 * Transform a given sample according to an ADSR envelope.
 * 
 * pa points to the initialized ADSR structure.
 * 
 * t is the time offset in samples from the beginning of the envelope.
 * It must be zero or greater.
 * 
 * dur is the duration of the event in samples, not including the extra
 * samples for the release.  It must be greater than zero.
 * 
 * The ADSR envelope is defined for t in [0, (dur + extra - 1)], where
 * extra is the return value of adsr_extra() for this envelope.  Any t
 * value greater than this range will result in a return value of zero.
 * 
 * intensity is the intensity of the event at time offset t.  It must be
 * in range [0.0, 1.0], where 0.0 maps to the minimum intensity
 * multiplier for this instrument and 1.0 maps to the maximum intensity
 * multiplier for this instrument.
 * 
 * s is the input sample to transform by the ADSR envelope.  The return
 * value is the transformed sample.
 * 
 * Parameters:
 * 
 *   pa - the initialized ADSR structure
 * 
 *   t - the time offset in samples from the start of the envelope
 * 
 *   dur - the duration of the event in samples, not including the extra
 *   time for release
 * 
 *   intensity - the intensity at time t
 * 
 *   s - the sample to transform
 * 
 * Return:
 * 
 *   the transformed sample
 */
int16_t adsr_mul(
    const ADSR_PARAM * pa,
          int32_t      t,
          int32_t      dur,
          double       intensity,
          int16_t      s);

#endif
