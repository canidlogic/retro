#ifndef RETRODEF_H_INCLUDED
#define RETRODEF_H_INCLUDED

/*
 * retrodef.h
 * 
 * Core definitions and includes used across the Retro synthesizer.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum integer value used for representing fractions.
 */
#define MAX_FRAC (1024)

/*
 * The two supported sampling rates.
 */
#define RATE_CD  (44100)  /* 44,100 Hz (Audio CDs) */
#define RATE_DVD (48000)  /* 48,000 Hz (DVDs) */

#endif
