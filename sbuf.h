#ifndef SBUF_H_INCLUDED
#define SBUF_H_INCLUDED

/*
 * sbuf.h
 * 
 * Sample buffer module of the Retro synthesizer.
 */

#include "retrodef.h"

/*
 * Initialize the sample buffer module.
 * 
 * This must be called before any other function in this module, except
 * for sbuf_close().  The sbuf_close() function should be called before
 * the program ends.
 * 
 * A fault occurs if this is called a second time, or it is called after
 * the sbuf_close() function has been called.
 */
void sbuf_init(void);

/*
 * Close down the sample buffer module.
 * 
 * This function may be called at any time.  If it is called when the
 * module has already been closed down, it is ignored.  If it is called
 * before the module has been initialized, the module is immediately
 * closed.
 * 
 * This should be called before the program finishes to make sure
 * everything is cleaned up.
 */
void sbuf_close(void);

/*
 * Record a 32-bit sample in the sample buffer.
 * 
 * The module must be initialized before calling this function, but the
 * module may not be closed down yet.  This function also may not be
 * called after sbuf_stream() has been called.
 * 
 * Parameters:
 * 
 *   l - the left channel value
 * 
 *   r - the right channel value
 */
void sbuf_sample(int32_t l, int32_t r);

/*
 * Stream the buffered samples to output.
 * 
 * The module must be initialized before calling this function, and at
 * least one sample must have been recorded with sbuf_sample().  The
 * module may not be closed down yet.  This function may only be called
 * once.
 * 
 * amp is the target maximum amplitude in the output file.  This is in
 * range [1, INT16_MAX].  All 32-bit samples will be scaled to range
 * [-amp, amp] before being output.
 * 
 * The wavwrite_sample() function of the WAV writer module will be used
 * to record each sample to output.  The WAV writer module must be
 * initialized but not yet closed when sbuf_stream() is called.
 * 
 * Parameters:
 * 
 *   amp - the output amplitude
 */
void sbuf_stream(int32_t amp);

#endif
