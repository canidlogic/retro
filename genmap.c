/*
 * genmap.c
 * ========
 * 
 * Implementation of genmap.h
 * 
 * See the header for further information.
 */

#include "genmap.h"
#include <stdlib.h>
#include <string.h>

/*
 * Public function implementations
 * -------------------------------
 * 
 * See header for specifications.
 */

/*
 * genmap_run function.
 */
void genmap_run(
    SNSOURCE      * pIn,
    GENMAP_RESULT * pResult,
    int32_t         samp_rate) {
  /* @@TODO: */
  ADSR_OBJ *adsr = NULL;
  adsr = adsr_alloc(
            25.0,   /* Attack duration in milliseconds */
            0.0,    /* Decay duration in milliseconds */
            1.0,    /* Sustain level multiplier */
            250.0,  /* Release duration in milliseconds */
            samp_rate);
  memset(pResult, 0, sizeof(GENMAP_RESULT));
  pResult->errcode = GENMAP_OK;
  pResult->pRoot = generator_op(
              GENERATOR_F_SINE,   /* function */
              1.0,                /* frequency multiplier */
              0.0,                /* frequency boost */
              20000.0,            /* base amplitude */
              adsr,               /* ADSR envelope */
              0.0,                /* FM feedback */
              0.0,                /* AM feedback */
              NULL,               /* FM modulator */
              NULL,               /* AM modulator */
              0.0,                /* FM modulator scale */
              0.0,                /* AM modulator scale */
              samp_rate,
              20000,              /* ny_limit */
              0,                  /* hlimit */
              0);                 /* instance data offset */
  pResult->icount = 1;
}

/*
 * genmap_errstr function.
 */
const char *genmap_errstr(int code) {
  
  const char *pResult = NULL;
  
  /* Handle error code */
  if (code < 0) {
    /* Shastina code, so pass through to Shastina */
    pResult = snerror_str(code);
    
  } else {
    /* Genmap error code */
    switch (code) {
      
      case GENMAP_OK:
        pResult = "No error";
        break;
      
      default:
        pResult = "Unknown error";
    }
  }
  
  /* Return result */
  return pResult;
}
