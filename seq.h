#ifndef SEQ_H_INCLUDED
#define SEQ_H_INCLUDED

/*
 * seq.h
 * 
 * Sequencer module of Retro synthesizer.
 */

#include "retrodef.h"
#include "instr.h"
#include "layer.h"
#include "sqwave.h"
#include "ttone.h"

/*
 * Add a note to the sequencer.
 * 
 * Notes can be added in any order, but the sequencer is optimized for
 * notes to be added in roughly sequential order.
 * 
 * The function fails if there are too many notes.
 * 
 * t is the time offset in samples from the beginning of the music.  It
 * must be zero or greater.
 * 
 * dur is the duration of the note in samples.  It must be greater than
 * zero.  Furthermore, (t+dur) must not exceed INT32_MAX.
 * 
 * pitch is the pitch of the note, in semitones from middle C.  That is,
 * a pitch of zero is middle C, -1 is one semitone below middle C, 2 is
 * two semitones above middle C, and so forth.  The range of the pitch
 * must be in [PITCH_MIN, PITCH_MAX].
 * 
 * instr is the instrument index to use to perform the note.  Note that
 * sequencing doesn't actually start when the note is defined, so the
 * instrument state will be read during sequencing, not during this
 * call.  The instrument index must be in [0, INSTR_MAXCOUNT - 1].
 * 
 * layer is the layer index to use to determine intensity during the
 * note.  As with the instrument index, the layer is queried during
 * sequencing, not during this call.  The layer index must be in range
 * [0, LAYER_MAXCOUNT - 1].
 * 
 * Parameters:
 * 
 *   t - the time offset of the note in samples
 * 
 *   dur - the duration of the note in samples
 * 
 *   pitch - the pitch in semitones from middle C
 * 
 *   instr - the instrument index
 * 
 *   layer - the layer index
 * 
 * Return:
 * 
 *   non-zero if successful, zero if too many notes
 */
int seq_note(
    int32_t t,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer);

/*
 * Perform the music according to the notes currently programmed in the
 * sequencer, using the current instrument and layer settings.
 * 
 * The square wave module must be initialized before using this
 * function.  The sbuf module must be initialized and open before using
 * this function.
 * 
 * The instrument and layer indices of each note select instruments and
 * layers according to the state of the instrument and layer modules
 * when this function is called, NOT when the note was defined with a
 * call to seq_note().
 * 
 * If no notes have been programmed yet, this call only outputs a single
 * silent sample.  Otherwise, it generates the appropriate samples and
 * sends them to the sbuf module.
 */
void seq_play(void);

#endif
