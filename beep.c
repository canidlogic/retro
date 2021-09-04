/*
 * beep.c
 */

#include "wavwrite.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  
  long scount = 0;
  
  /* Initialize WAV writer */
  if (!wavwrite_init("silence.wav",
                      WAVWRITE_INIT_44100 | WAVWRITE_INIT_MONO)) {
    abort();
  }
  
  /* Write five seconds of silence */
  for(scount = 0; scount < (44100 * 5); scount++) {
    wavwrite_sample(0, 0);
  }
  
  /* Close down */
  wavwrite_close(WAVWRITE_CLOSE_NORMAL);
  
  /* Return success */
  return 0;
}
