/*
 * beep.c
 */

#include "wavwrite.h"
#include "sqwave.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  
  int32_t scount = 0;
  int v = 0;
  
  /* Initialize square wave module */
  sqwave_init(1024, 0.3, 20000);
  
  /* Initialize WAV writer */
  if (!wavwrite_init("a440_filter.wav",
                      WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO)) {
    abort();
  }
  
  /* Write five seconds of 440 Hz square wave */
  for(scount = 0; scount < (44100 * 5); scount++) {
    v = (int) sqwave_get(440, 44100, (int32_t) scount);
    wavwrite_sample(v, v);
  }
  
  /* Close down */
  wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  
  /* Return success */
  return 0;
}
