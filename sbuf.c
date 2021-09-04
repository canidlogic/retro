/*
 * sbuf.c
 * 
 * Implementation of sbuf.h
 */

#include "sbuf.h"
#include "wavwrite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/* 
 * The module state constants.
 */
#define SBUF_STATE_NONE   (0)   /* Nothing done yet */
#define SBUF_STATE_OPEN   (1)   /* Module opened but not yet streamed */
#define SBUF_STATE_STREAM (2)   /* Module has been streamed */
#define SBUF_STATE_CLOSED (3)   /* Module closed */

/*
 * Type declarations
 * =================
 */

/*
 * The sample structure in the buffer.
 */
typedef struct {
  
  int32_t l;
  int32_t r;
  
} SBUF_SAMP;

/*
 * Static data
 * ===========
 */

/*
 * The state of the module.
 * 
 * This is one of the SBUF_STATE constants.
 */
static int m_sbuf_state = SBUF_STATE_NONE;

/*
 * The temporary buffer file.
 * 
 * Only valid if m_sbuf_state is OPEN or STREAM.
 */
static FILE *m_sbuf_fp = NULL;

/*
 * The total number of samples that have been written to the buffer.
 */
static int32_t m_sbuf_count = 0;

/*
 * The maximum absolute sample value that has been written to the
 * buffer.
 */
static int32_t m_sbuf_maxval = 0;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * sbuf_init function.
 */
void sbuf_init(void) {
  
  /* Check state */
  if (m_sbuf_state != SBUF_STATE_NONE) {
    abort();
  }
  
  /* Open the temporary file */
  m_sbuf_fp = tmpfile();
  if (m_sbuf_fp == NULL) {
    abort();
  }
  
  /* Set the new state */
  m_sbuf_state = SBUF_STATE_OPEN;
}

/*
 * sbuf_close function.
 */
void sbuf_close(void) {
  
  /* Close the temporary file if open */
  if ((m_sbuf_state == SBUF_STATE_OPEN) ||
      (m_sbuf_state == SBUF_STATE_STREAM)) {
    fclose(m_sbuf_fp);
    m_sbuf_fp = NULL;
  }
  
  /* Set the new state */
  m_sbuf_state = SBUF_STATE_CLOSED;
}

/*
 * sbuf_sample function.
 */
void sbuf_sample(int32_t l, int32_t r) {
  
  int32_t av = 0;
  SBUF_SAMP sbs;
  
  /* Initialize structures */
  memset(&sbs, 0, sizeof(SBUF_SAMP));
  
  /* Check state */
  if (m_sbuf_state != SBUF_STATE_OPEN) {
    abort();
  }
  
  /* Get the greatest absolute value of the two samples */
  if (l >= 0) {
    av = l;
  } else {
    av = -(l);
  }
  if (r >= 0) {
    if (r > av) {
      av = r;
    }
  } else {
    if (-(r) > av) {
      av = -(r);
    }
  }
  
  /* Update maxval statistic */
  if (m_sbuf_maxval < av) {
    m_sbuf_maxval = av;
  }
  
  /* Update count, watching for overflow */
  if (m_sbuf_count < INT32_MAX) {
    m_sbuf_count++;
  } else {
    abort();  /* count overflow */
  }
  
  /* Write the sample */
  sbs.l = l;
  sbs.r = r;
  if (fwrite(&sbs, sizeof(SBUF_SAMP), 1, m_sbuf_fp) != 1) {
    abort();  /* I/O error */
  }
}

/*
 * sbuf_stream function.
 */
void sbuf_stream(int32_t amp) {
  
  int32_t x = 0;
  int32_t lq = 0;
  int32_t rq = 0;
  SBUF_SAMP sbs;
  
  /* Initialize structure */
  memset(&sbs, 0, sizeof(SBUF_SAMP));
  
  /* Check state */
  if (m_sbuf_state != SBUF_STATE_OPEN) {
    abort();
  }
  
  /* Check parameter */
  if ((amp < 1) || (amp > INT16_MAX)) {
    abort();
  }
  
  /* Only proceed if at least one sample recorded */
  if (m_sbuf_count > 0) {
  
    /* If the maxval value is zero, set it to one to avoid weird
     * cases */
    if (m_sbuf_maxval < 1) {
      m_sbuf_maxval = 1;
    }
  
    /* Rewind temporary file to the beginning */
    if (fseek(m_sbuf_fp, 0, SEEK_SET)) {
      abort();  /* I/O error */
    }
    
    /* Transfer each sample to output, scaling each appropriately */
    for(x = 0; x < m_sbuf_count; x++) {
    
      /* Read the next sample from the buffer */
      if (fread(&sbs, sizeof(SBUF_SAMP), 1, m_sbuf_fp) != 1) {
        abort();  /* I/O error */
      }
    
      /* Scale the left and right channel values */
      lq = (int32_t) ((((int64_t) sbs.l) * ((int64_t) amp)) /
                      ((int64_t) m_sbuf_maxval));
      rq = (int32_t) ((((int64_t) sbs.r) * ((int64_t) amp)) /
                      ((int64_t) m_sbuf_maxval));
    
      /* Clamp to range */
      if (lq > INT16_MAX) {
        lq = INT16_MAX;
      } else if (lq < -(INT16_MAX)) {
        lq = -(INT16_MAX);
      }
      if (rq > INT16_MAX) {
        rq = INT16_MAX;
      } else if (rq < -(INT16_MAX)) {
        rq = -(INT16_MAX);
      }
      
      /* Write scaled samples to output */
      wavwrite_sample((int) lq, (int) rq);
    }
  }
  
  /* Update state */
  m_sbuf_state = SBUF_STATE_STREAM;
}
