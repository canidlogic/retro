/*
 * beep.c
 */

#include "wavwrite.h"
#include <math.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  
  long scount = 0;
  double f = 0.0;
  int v = 0;
  
  /* Initialize WAV writer */
  if (!wavwrite_init("a440.wav",
                      WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO)) {
    abort();
  }
  
  /* Write five seconds of 440 Hz tone */
  for(scount = 0; scount < (44100 * 5); scount++) {
    f = sin(2 * M_PI * ((double) scount) * (440.0 / 44100.0));
    v = (int) (f * 20000.0);
    wavwrite_sample(v, v);
  }
  
  /* Close down */
  wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  
  /* Return success */
  return 0;
}
