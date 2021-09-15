#ifndef GENERATOR_H_INCLUDED
#define GENERATOR_H_INCLUDED

/*
 * generator.h
 * ===========
 * 
 * Definition of the generator object class.
 * 
 * Compile with adsr module.
 * 
 * The math library may also be required, with -lm
 */

#include "retrodef.h"
#include "adsr.h"

/*
 * Constants
 * ---------
 */

/*
 * The different kinds of operator functions available.
 * 
 * GENERATOR_F_MINVAL and GENERATOR_F_MAXVAL are the minimum and maximum
 * valid values.  They should be kept updated appropriately.
 */
#define GENERATOR_F_SINE      (1)   /* Sine wave function     */
#define GENERATOR_F_NOISE     (2)   /* White noise function   */

#define GENERATOR_F_MINVAL (1)
#define GENERATOR_F_MAXVAL (2)

/*
 * Type declarations
 * -----------------
 */

/*
 * GENERATOR structure prototype.
 * 
 * Definition given in implementation file.
 */
struct GENERATOR_TAG;
typedef struct GENERATOR_TAG GENERATOR;

/*
 * Instance data for operators.
 * 
 * Clients should not directly access the internals of this structure.
 * Use generator_opdata_init to initialize.
 */
typedef struct {
  
  /*
   * The normalized location within the waveform for the wave-type
   * operator functions.
   * 
   * Zero is always at the start of the wave and 1.0 is always at the
   * end of a single wave cycle.
   * 
   * Ignored for NOISE functions.
   * 
   * Always set to zero if t has a value less than zero.
   */
  double w;
  
  /*
   * The frequency of the sound that is being rendered.
   * 
   * This is the value passed to generator_opdata_init().  It does NOT
   * take into account the frequency multiplier for the operator or any
   * frequency modulation effects.
   * 
   * Ignored for NOISE functions.
   */
  double freq;
  
  /*
   * The sample generated for the current time, if t >= 0.
   */
  double current;
  
  /*
   * The current time that has been generated.
   * 
   * This is -1 if no samples have been generated yet.
   * 
   * This is -2 if the operator frequency has exceeded the limit and the
   * operator is now disabled.
   */
  int32_t t;
  
  /*
   * The duration in samples of the event being rendered by this
   * instance.
   * 
   * This does NOT include release samples added by the ADSR envelope.
   */
  int32_t dur;
  
} GENERATOR_OPDATA;

/*
 * Public functions
 * ----------------
 */

/*
 * Initialize an operator instance data structure.
 * 
 * pod is the structure to initialize.  The current state of the
 * structure does not matter.
 * 
 * freq is the frequency in Hz that is being processed by this generator
 * map.  For NOISE type of operators, freq is ignored.  For the other
 * operator functions, the actual frequency of the operator is
 * determined by multiplying the giving freq parameter to the freq_mul
 * parameter and then adding to the frequency boost that was passed when
 * the operator was constructed.  This parameter must be finite and
 * greater than zero.
 * 
 * dur is the duration in samples of the event being rendered.  This is
 * necessary to use the ADSR envelope.  It must be greater than zero.
 * This does NOT include any release samples added by the ADSR envelope.
 * 
 * Parameters:
 * 
 *   pod - the operator instance data structure to initialize
 * 
 *   freq - the frequency that is being rendered, in Hz
 * 
 *   dur - the duration in samples of the event being rendered
 */
void generator_opdata_init(
    GENERATOR_OPDATA * pod,
    double             freq,
    int32_t            dur);

/*
 * Construct an additive generator, which mixes together the output of
 * multiple generators by adding their values together.
 * 
 * ppg points to an array of generator pointers, and count is the number
 * of generator pointers in this array.  count must be greater than
 * zero.  A copy of this array will be made and stored in the
 * constructed additive generator, and the additive generator will add
 * to the reference count of each of the referenced generators in the
 * array.  These references will be released when the additive generator
 * is released.
 * 
 * The reference count of the constructed generator object starts out at
 * one.
 * 
 * Additive generators do not have any "instance data."  All of their
 * data is "class data" that is shared across all instances of the
 * generator map.
 * 
 * Parameters:
 * 
 *   ppg - pointer to array of generator pointers
 * 
 *   count - the number of elements in the array
 * 
 * Return:
 * 
 *   the newly constructed additive generator
 */
GENERATOR *generator_additive(GENERATOR **ppg, int32_t count);

/*
 * Construct a scaling generator, which multiplies the generated samples
 * of an underlying generator by a constant amount.
 * 
 * The scaling value may be any finite value.
 * 
 * The reference count of the underlying generator is incremented.  The
 * reference count of the constructed generator object starts out at
 * one.
 * 
 * Scaling generators do not have any "instance data."  All of their
 * data is "class data" that is shared across all instances of the
 * generator map.
 * 
 * Parameters:
 * 
 *   pBase - pointer to the underlying generator
 * 
 *   scale - the value to multiply to all samples
 * 
 * Return:
 * 
 *   the newly constructed scaling generator
 */
GENERATOR *generator_scale(GENERATOR *pBase, double scale);

/*
 * Construct a clip generator, which clips samples above a certain level
 * from an underlying generator.
 * 
 * The level value may be any finite value that is zero or greater.
 * 
 * The reference count of the underlying generator is incremented.  The
 * reference count of the constructed generator object starts out at
 * one.
 * 
 * Clip generators do not have any "instance data."  All of their data
 * is "class data" that is shared across all instances of the generator
 * map.
 * 
 * Parameters:
 * 
 *   pBase - pointer to the underlying generator
 * 
 *   level - the maximum level to clip to
 * 
 * Return:
 * 
 *   the newly constructed clip generator
 */
GENERATOR *generator_clip(GENERATOR *pBase, double level);

/*
 * Construct an operator generator.
 * 
 * fop is one of the GENERATOR_F constants, which determines the
 * function to use for the operator.  If NOISE is chosen, then the
 * freq_mul, fm_feedback, pFM, and fm_scale arguments are ignored,
 * because noise doesn't have any specific frequency.
 * 
 * freq_mul is a finite floating-point value that is zero or greater,
 * which determines the base frequency of the operator.  The actual
 * frequency of the operator is determined when constructing the
 * "instance data" of an operator with generator_opdata_init(), at which
 * point the frequency passed to that function will be multipled by this
 * freq_mul parameter and then added to freq_boost to determine the
 * actual frequency of the specific instance of this operator.
 * 
 * freq_boost is a finite floating-point value, which determines in
 * combination with freq_mul the frequency of the operator.
 * 
 * pAmp points to an ADSR envelope object that determines the amplitude
 * of this operator over time.  A reference to the ADSR object will be
 * added, and this reference will be released when the operator
 * generator is released.
 * 
 * pFM is either NULL or it points to a generator object that is used to
 * drive frequency modulation.  If provided, then a reference to the
 * FM generator is added, which will be released when this operator is
 * released.  If provided, the output of the FM generator will be added
 * to the frequency of the operator at each time unit.
 * 
 * pAM is either NULL or it points to a generator object that is used to
 * drive amplitude modulation.  If provided, then a reference to the AM
 * generator is added, which will be released when this operator is
 * released.  If provided, the output of the AM generator will be added
 * to the amplitude generated by the ADSR envelope at each time unit.
 * 
 * samp_rate is the sampling rate of the audio that is being generated.
 * It must be either RATE_CD or RATE_DVD.  It is ignored for NOISE type
 * operators.
 * 
 * Parameters:
 * 
 *   fop - the GENERATOR_F constant selecting the operator function
 * 
 *   freq_mul - the multiplier used to generate the frequency
 * 
 *   freq_boost - the frequency boost value
 * 
 *   pAmp - the ADSR envelope to use
 * 
 *   pFM - the frequency modulation generator, or NULL
 * 
 *   pAM - the amplitude modulation generator, or NULL
 * 
 *   samp_rate - the sampling rate, in Hz
 *
 * Return:
 * 
 *   the newly constructed generator object
 */
GENERATOR *generator_op(
    int         fop,
    double      freq_mul,
    double      freq_boost,
    ADSR_OBJ  * pAmp,
    GENERATOR * pFM,
    GENERATOR * pAM,
    int32_t     samp_rate);

/*
 * Increment the reference count of the given generator object.
 * 
 * A fault occurs if the reference count overflows.  This only happens
 * if there are billions of references.
 * 
 * Parameters:
 * 
 *   pg - the generator object to increase the reference count of
 */
void generator_addref(GENERATOR *pg);

/*
 * Decrement the reference count of the given generator object.
 * 
 * If NULL is passed, this call is ignored.
 * 
 * If decrementing the reference count causes the reference count to
 * drop to zero, then the generator object is released.
 * 
 * Parameters:
 * 
 *   pg - the generator object to decrease the reference count of
 */
void generator_release(GENERATOR *pg);

/*
 * Invoke a generator object.
 * 
 * Before using this function, you must bind all generators by using
 * generator_bind() on the generator object or a fault occurs.
 * 
 * pg is the generator object to invoke.  pods points to an array of
 * operator data structures, and pod_count determines how many operator
 * data structures are present in this array.  The operator data
 * structures must have previously been initialized with the function
 * generator_opdata_init().  The operator data structures are the
 * "instance data" for the generator map that store data specific to the
 * particular sound being generated, while the data stored within the
 * generator data structure is "class data" that is shared across all
 * instances of the generator map.
 * 
 * t is the sample offset, which must be zero or greater.  Furthermore,
 * generators must always be invoked in a sequence t=0 t=1 t=2 etc.,
 * with the only exception being that each t value may be repeated
 * multiple times in a row (which happens if a generator has its output
 * going to multiple places).
 * 
 * Parameters:
 * 
 *   pg - the generator object to invoke
 * 
 *   pods - the instance data structures
 * 
 *   pod_count - the number of instance data structures
 * 
 *   t - the sample time offset
 * 
 * Return:
 * 
 *   the generated value
 */
double generator_invoke(
    GENERATOR        * pg,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count,
    int32_t            t);

/*
 * Determine the total length in samples of the sound that is being
 * rendered by a specific generator instance.
 * 
 * Before using this function, you must bind all generators by using
 * generator_bind() on the generator object or a fault occurs.
 * 
 * pg is the generator object to invoke.  pods points to an array of
 * operator data structures, and pod_count determines how many operator
 * data structures are present in this array.  The operator data
 * structures must have previously been initialized with the function
 * generator_opdata_init().  The operator data structures are the
 * "instance data" for the generator map that store data specific to the
 * particular sound being generated, while the data stored within the
 * generator data structure is "class data" that is shared across all
 * instances of the generator map.
 * 
 * The "instance data" for the generators already has the duration
 * information of the rendered sound, so the duration does not need to
 * be provided to this function.
 * 
 * If the provided generator is an operator, this function will query
 * the ADSR envelope of the operator to find out how long the rendered
 * sound actually is in samples and return that value.
 * 
 * If the provided generator is additive, this function will recursively
 * query all component generators until it reaches operators, and then
 * return the maximum length among the ADSR envelopes for all reached
 * operators.
 * 
 * If the provided generator is a scaling generator, the call is passed
 * through to the underlying generator.
 * 
 * Parameters:
 * 
 *   pg - the generator object to invoke
 * 
 *   pods - the instance data structures
 * 
 *   pod_count - the number of instance data structures
 * 
 * Return:
 * 
 *   the total length in samples
 */
int32_t generator_length(
    GENERATOR        * pg,
    GENERATOR_OPDATA * pods,
    int32_t            pod_count);

/*
 * Recursively bind a generator object and all generator objects that
 * can be reached from the generator object.
 * 
 * Binding an additive generator simply forwards the binding call to
 * each of the component generators.  Binding a scaling generator calls
 * through to the underlying generator.  Binding an operator assigns a
 * unique index within the instance data array and forwards the call to
 * any modulator generator objects.
 * 
 * start is the number of instance data structures that have been
 * assigned so far.  The top-level bind call should set this parameter
 * to zero.
 * 
 * The return value is the total number of instance data structures that
 * were assigned during the bind operation.
 * 
 * You must bind generators before you can use generator_invoke() or
 * generator_length().
 * 
 * Parameters:
 * 
 *   pg - the generator to recursively bind
 * 
 *   start - the number of bindings so far
 * 
 * Return:
 * 
 *   the total number of instance data structures that have been bound
 */
int32_t generator_bind(GENERATOR *pg, int32_t start);

#endif
