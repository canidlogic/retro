#ifndef WAVWRITE_H_INCLUDED
#define WAVWRITE_H_INCLUDED

/*
 * wavwrite.h
 * 
 * WAV writer module of the retro synthesizer.
 */

#include <stddef.h>

/*
 * Flags for wavwrite_init().
 */
#define WAVWRITE_INIT_44100     (0x1)
#define WAVWRITE_INIT_48000     (0x2)
#define WAVWRITE_INIT_MONO      (0x4)
#define WAVWRITE_INIT_STEREO    (0x8)

/*
 * Flags for wavwrite_close().
 */
#define WAVWRITE_CLOSE_NORMAL   (0)
#define WAVWRITE_CLOSE_RMFILE   (0x1)

/*
 * Initialize the WAV writer module.
 * 
 * This must be called before any of the other WAV writer functions,
 * except for wavwrite_close().  This function may only be called once.
 * 
 * Call wavwrite_close() before the program finishes.  If this function
 * is not explicitly called, an incomplete or invalid WAV file may be
 * left on disk.
 * 
 * pPath is the path to the WAV output file.  If a file already exists
 * at this path, it will be overwritten.
 * 
 * flags is a combination of WAVWRITE_INIT flags.  Unrecognized flags
 * are ignored.  Exactly one sample rate flag and exactly one channel
 * configuration flag must be specified:
 * 
 *   Sample rate flags:
 *
 *     WAVWRITE_INIT_44100 -> 44,100 Hz (CD sample rate)
 *     WAVWRITE_INIT_48000 -> 48,000 Hz (DVD sample rate)
 * 
 *   Channel configuration flags:
 * 
 *     WAVWRITE_INIT_MONO   -> one channel
 *     WAVWRITE_INIT_STEREO -> two channels
 * 
 * The function fails if the output file can't be created.  In this
 * case, the WAV writer module will transition to closed state.
 * 
 * Parameters:
 * 
 *   pPath - path to the output WAV file to create
 * 
 *   flags - initialization flags
 * 
 * Return:
 * 
 *   non-zero if successful, zero if output file could not be created
 */
int wavwrite_init(const char *pPath, int flags);

/*
 * Close down the WAV writer module.
 * 
 * This function may be called at any time, even before wavwrite_init().
 * The WAV writer module always transitions to closed state after this
 * function.  If this function is called when the WAV writer is already
 * closed, the function has no effect.
 * 
 * flags is a combination of WAVWRITE_CLOSE flags.  Use zero or
 * WAVWRITE_CLOSE_NORMAL if no special flags are required.
 * 
 * If the WAVWRITE_CLOSE_RMFILE flag is specified, then if the WAV
 * writer module is currently initialized, the output WAV file will be
 * erased during closure.  Otherwise, the normal operation is to flush
 * the write buffer and complete the WAV file.  The RMFILE flag is
 * ignored if the WAV writer module is not currently initialized.
 * 
 * Unrecognized flags are ignored.
 * 
 * If this function is not called before the end of the program, an
 * incomplete or invalid WAV file may be left on disk.
 * 
 * Parameters:
 * 
 *   flags - closure flags
 */
void wavwrite_close(int flags);

/*
 * Write a sample to output.
 * 
 * The WAV writer module must be initialized with wavwrite_init() before
 * calling this function, and it may not be closed.
 * 
 * left is the left channel value and right is the right channel value.
 * Both are clamped to the range [-32,767, +32,767] by this function.
 * 
 * If the WAV writer was initialized with WAVWRITE_INIT_MONO, then left
 * and right must be the same value or a fault occurs.
 * 
 * Parameters:
 * 
 *   left - the left channel value
 * 
 *   right - the right channel value
 */
void wavwrite_sample(int left, int right);

#endif
