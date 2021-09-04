/*
 * sbuf.c
 * 
 * Implementation of sbuf.h
 */

#include "sbuf.h"
#include "wavwrite.h"

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
  /* @@TODO: */
}

/*
 * sbuf_close function.
 */
void sbuf_close(void) {
  /* @@TODO: */
}

/*
 * sbuf_sample function.
 */
void sbuf_sample(int32_t l, int32_t r) {
  /* @@TODO: */
  if (l < -(INT16_MAX)) {
    l = -(INT16_MAX);
  } else if (l > INT16_MAX) {
    l = INT16_MAX;
  }
  if (r < -(INT16_MAX)) {
    r = -(INT16_MAX);
  } else if (r > INT16_MAX) {
    r = INT16_MAX;
  }
  wavwrite_sample((int) l, (int) r);
}

/*
 * sbuf_stream function.
 */
void sbuf_stream(int32_t amp) {
  /* @@TODO: */
}
