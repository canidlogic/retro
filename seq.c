/*
 * seq.c
 * 
 * Implementation of seq.h
 * 
 * See the header for further information.
 */

#include "seq.h"
#include "sbuf.h"
#include "stereo.h"
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * The initial buffer capacity, in notes.
 */
#define SEQ_CAP_INIT (INT32_C(4096))

/*
 * The maximum buffer capacity, in notes.
 */
#define SEQ_CAP_MAX (INT32_C(1048576))

/*
 * Type declarations
 * =================
 */

/*
 * The note structure.
 */
typedef struct {
  
  /*
   * The time offset in samples.
   * 
   * Must be zero or greater.
   */
  int32_t t;
  
  /*
   * The duration in samples.
   * 
   * Must be greater than zero, and (t+dur) must not exceed INT32_MAX.
   */
  int32_t dur;
  
  /*
   * The pitch in semitones from middle C.
   * 
   * Must be in range [SQWAVE_PITCH_MIN, SQWAVE_PITCH_MAX].
   */
  int16_t pitch;
  
  /*
   * The instrument index.
   * 
   * Must be in range [0, INSTR_MAXCOUNT - 1].
   */
  uint16_t instr;
  
  /*
   * The layer index.
   * 
   * Must be in range [0, LAYER_MAXCOUNT - 1].
   */
  int32_t layer;
  
} SEQ_NOTE;

/*
 * The event structure.
 */
struct SEQ_EVENT_TAG;
typedef struct SEQ_EVENT_TAG SEQ_EVENT;
struct SEQ_EVENT_TAG {
  
  /*
   * The index of the note that is being played.
   */
  int32_t note_i;
  
  /*
   * The greatest t offset of the envelope for this event.
   */
  int32_t max_t;
  
  /*
   * Pointer to the previous event in the current event list, or NULL if
   * this is the first event.
   */
  SEQ_EVENT *pPrev;
  
  /*
   * Pointer to the next event in the current event list, or NULL if
   * this is the last event.
   */
  SEQ_EVENT *pNext;
};

/*
 * Static data
 * ===========
 */

/*
 * The current capacity of the note buffer in notes.
 * 
 * If this is zero, the note buffer hasn't been allocated yet.
 */
static int32_t m_seq_cap = 0;

/*
 * The total number of notes in the note buffer.
 */
static int32_t m_seq_count = 0;

/*
 * The note buffer.
 * 
 * m_seq_cap is the total size of the buffer in notes.
 * 
 * m_seq_count is the total number of notes actually used.
 * 
 * If m_seq_cap is zero, the buffer isn't allocated yet.
 */
static SEQ_NOTE *m_seq_buf = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void seq_shift(int32_t i);

/*
 * Shift all note entries right starting at index i.
 * 
 * i must be in range [0, m_seq_count - 1].
 * 
 * The shift sequence is all elements starting at i and proceeding to
 * the end of the buffer.
 * 
 * If the shift sequence only has one element, the element is just
 * cleared.
 * 
 * If the shift sequence has more than one element, the last element is
 * dropped, all other elements are shifted one to the right, and the
 * first element is blanked.
 * 
 * Parameters:
 * 
 *   i - the index to begin the shift at
 */
static void seq_shift(int32_t i) {
  
  int32_t x = 0;
  
  /* Check parameter */
  if ((i < 0) || (i >= m_seq_count)) {
    abort();
  }
  
  /* Starting from second-to-last element (if any), shift elements
   * right */
  for(x = m_seq_count - 2; x >= i; x--) {
    memcpy(&(m_seq_buf[x + 1]), &(m_seq_buf[x]), sizeof(SEQ_NOTE));
  }
  
  /* Clear the element at the given index */
  memset(&(m_seq_buf[i]), 0, sizeof(SEQ_NOTE));
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * seq_note function.
 */
int seq_note(
    int32_t t,
    int32_t dur,
    int32_t pitch,
    int32_t instr,
    int32_t layer) {

  int status = 1;
  int32_t newcap = 0;
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int32_t mt = 0;
  SEQ_NOTE *pn = NULL;

  /* Check parameters */
  if ((t < 0) || (dur < 1)) {
    abort();
  }
  if (dur > INT32_MAX - t) {
    abort();
  }
  if ((pitch < SQWAVE_PITCH_MIN) || (pitch > SQWAVE_PITCH_MAX)) {
    abort();
  }
  if ((instr < 0) || (instr >= INSTR_MAXCOUNT)) {
    abort();
  }
  if ((layer < 0) || (layer >= LAYER_MAXCOUNT)) {
    abort();
  }
  
  /* Only proceed if we aren't at maximum capacity; else, fail */
  if (m_seq_count < SEQ_CAP_MAX) {
    
    /* Not at maximum capacity yet, check first if we need to make
     * initial allocation */
    if (m_seq_cap < 1) {
      /* We need to make initial allocation */
      m_seq_buf = (SEQ_NOTE *) malloc(SEQ_CAP_INIT * sizeof(SEQ_NOTE));
      if (m_seq_buf == NULL) {
        abort();
      }
      memset(m_seq_buf, 0, SEQ_CAP_INIT * sizeof(SEQ_NOTE));
      m_seq_cap = SEQ_CAP_INIT;
    }
    
    /* Next, check whether we need to expand capacity */
    if (m_seq_count >= m_seq_cap) {
      /* Expansion needed -- expanded capacity is double current
       * capacity or maximum capacity, whichever is lesser */
      newcap = m_seq_cap * 2;
      if (newcap > SEQ_CAP_MAX) {
        newcap = SEQ_CAP_MAX;
      }
      
      /* Expand buffer */
      m_seq_buf = (SEQ_NOTE *) realloc(
                    m_seq_buf, newcap * sizeof(SEQ_NOTE));
      if (m_seq_buf == NULL) {
        abort();
      }
      memset(
        &(m_seq_buf[m_seq_cap]),
        0,
        (newcap - m_seq_cap) * sizeof(SEQ_NOTE));
      m_seq_cap = newcap;
    }
    
    /* First special case to filter out is the first note being added */
    if (m_seq_count > 0) {
      /* Not first note, next special case to filter out is that the
       * note should go first in the buffer */
      if ((m_seq_buf[0]).t <= t) {
        
        /* General case -- find a note that has greatest t value that is
         * less than or equal to t value of new note */
        lo = 0;
        hi = m_seq_count - 1;
        while(lo < hi) {
          
          /* Find the midpoint, at least one greater than low bound */
          mid = lo + ((hi - lo) / 2);
          if (mid <= lo) {
            mid = lo + 1;
          }
          
          /* Get midpoint t value */
          mt = (m_seq_buf[mid]).t;
          
          /* Compare new t to midpoint t */
          if (t > mt) {
            /* t greater than midpoint, so midpoint becomes new low
             * boundary */
            lo = mid;
            
          } else if (t < mt) {
            /* t less than midpoint, so new high boundary is one less
             * than midpoint */
            hi = mid - 1;
            
          } else if (t == mt) {
            /* t equals midpoint t, so we can just zero in on that
             * record */
            lo = mid;
            hi = mid;
            
          } else {
            /* Shouldn't happen */
            abort();
          }
        }
      
        /* Low bound is the index we insert the new note after */
        m_seq_count++;
        seq_shift(lo + 1);
        pn = &(m_seq_buf[lo + 1]);
      
      } else {
        /* Note added at very beginning */
        m_seq_count++;
        seq_shift(0);
        pn = &(m_seq_buf[0]);
      }
      
    } else {
      /* This is the first note, so just add it */
      m_seq_count++;
      pn = &(m_seq_buf[0]);
    }
  
    /* Fill in note structure */
    pn->t = t;
    pn->dur = dur;
    pn->pitch = (int16_t) pitch;
    pn->instr = (uint16_t) instr;
    pn->layer = layer;
  
  } else {
    /* Already at maximum capacity */
    status = 0;
  }

  /* Return status */
  return status;
}

/*
 * seq_play function.
 */
void seq_play(void) {
  
  int32_t t = 0;
  int32_t x = 0;
  int32_t notes_read = 0;
  int64_t mt = 0;
  int16_t amp = 0;
  SEQ_EVENT *pl = NULL;
  SEQ_EVENT *pse = NULL;
  SEQ_EVENT *psr = NULL;
  SEQ_NOTE *pn = NULL;
  
  int32_t samp_left = 0;
  int32_t samp_right = 0;
  
  STEREO_SAMP ssp;
  
  /* Initialize structures */
  memset(&ssp, 0, sizeof(STEREO_SAMP));
  
  /* If no notes, then output silent sample */
  if (m_seq_count < 1) {
    sbuf_sample(0, 0);
  }
  
  /* Keep sequencing until we've read all the notes and the event list
   * is empty */
  while ((notes_read < m_seq_count) || (pl != NULL)) {
    
    /* Reset the current sample */
    samp_left = 0;
    samp_right = 0;
    
    /* Remove finished notes from the event list */
    pse = pl;
    while (pse != NULL) {
      /* Check if current event is finished */
      if (pse->max_t < t) {
        /* Current event finished, remove it */
        if (pse->pPrev != NULL) {
          (pse->pPrev)->pNext = pse->pNext;
        } else {
          pl = pse->pNext;
        }
        
        if (pse->pNext != NULL) {
          (pse->pNext)->pPrev = pse->pPrev;
        }
        
        psr = pse;
        pse = pse->pNext;
        free(psr);
        psr = NULL;
        
      } else {
        /* Current event not finished, move to next */
        pse = pse->pNext;
      }
    }
    
    /* Add any new notes to the event list */
    for(x = notes_read; x < m_seq_count; x++) {
      if ((m_seq_buf[x]).t <= t) {
        
        /* Add another note to the list */
        pse = (SEQ_EVENT *) malloc(sizeof(SEQ_EVENT));
        if (pse == NULL) {
          abort();
        }
        memset(pse, 0, sizeof(SEQ_EVENT));
        
        /* Link the note in */
        pse->pPrev = NULL;
        pse->pNext = pl;
        if (pl != NULL) {
          pl->pPrev = pse;
        }
        pl = pse;
        
        /* Compute the max_t */
        mt = ((int64_t) (m_seq_buf[x]).t) - 1 +
              ((int64_t) instr_length(
                            (m_seq_buf[x]).instr,
                            (m_seq_buf[x]).dur
              ));
        if (mt > INT32_MAX) {
          mt = INT32_MAX;
        }
        pse->max_t = (int32_t) mt;
        
        /* Copy the note index and update the notes_read count */
        pse->note_i = x;
        notes_read++;
      
      } else {
        /* No more notes to add */
        break;
      }
    }
    
    /* Compute the current sample by going through all notes in the
     * event list */
    for(pse = pl; pse != NULL; pse = pse->pNext) {
      
      /* Get a pointer to the note */
      pn = &(m_seq_buf[pse->note_i]);
      
      /* Get the amplitude of the layer at t */
      amp = layer_get(pn->layer, t);
      
      /* Compute the stereo sample */
      instr_get(pn->instr, t - pn->t, pn->dur, pn->pitch, amp, &ssp);
      
      /* Mix the stereo sample in */
      mt = ((int64_t) samp_left) + ((int64_t) ssp.left);
      if (mt > INT32_MAX) {
        mt = INT32_MAX;
      } else if (mt < -(INT32_MAX)) {
        mt = -(INT32_MAX);
      }
      samp_left = (int32_t) mt;
      
      mt = ((int64_t) samp_right) + ((int64_t) ssp.right);
      if (mt > INT32_MAX) {
        mt = INT32_MAX;
      } else if (mt < -(INT32_MAX)) {
        mt = -(INT32_MAX);
      }
      samp_right = (int32_t) mt;
    }
    
    /* Output the current sample */
    sbuf_sample(samp_left, samp_right);
    
    /* Proceed to next t value */
    if (t < INT32_MAX) {
      t++;
    } else {
      abort();
    }
  }
}
