/*
 * wavwrite.c
 * 
 * Implementation of wavwrite.h
 * 
 * See the header for further information.
 */

#include "wavwrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * The maximum number of bytes that may be written to the output file.
 * 
 * It is unwise to set this higher than 1GB.
 */
#define WAVWRITE_MAXFILE (1000000000UL)

/*
 * Limits of fixed integers.
 */
#define WAVWRITE_U32MAX (0xffffffffUL)  /* Max unsigned 32-bit value */

#define WAVWRITE_S16MIN (-32767)  /* Minimum signed 16-bit value */
#define WAVWRITE_S16MAX (32767)   /* Maximum signed 16-bit value */
#define WAVWRITE_U16MAX (65535U)  /* Maximum unsigned 16-bit value */

#define WAVWRITE_U8MAX  (255)     /* Maximum unsigned 8-bit value */

/*
 * Static data
 * ===========
 */

/*
 * Non-zero when module has been closed.
 * 
 * Once the module is closed, all other static data is invalid.
 */
static int m_wavwrite_closed = 0;

/*
 * Initialization flags.
 * 
 * This is only valid if m_wavwrite_closed is zero.
 * 
 * If this value is valid and zero, it means the module has not been
 * initialized yet.
 * 
 * If this value is valid and non-zero, it means the module has been
 * initialized and these are the flags that were passed to the
 * initialization function.
 */
static int m_wavwrite_flags = 0;

/*
 * Output file path copy.
 * 
 * This is only valid if m_wavwrite_closed is zero and m_wavwrite_flags
 * is non-zero.
 * 
 * This stores a dynamically-allocated copy of the path that was passed
 * to the initialization function.  This allows the output file to be
 * erased if necessary.
 */
static char *m_wavwrite_pPath = NULL;

/*
 * Output file handle.
 * 
 * This is only valid if m_wavwrite_closed is zero and m_wavwrite_flags
 * is non-zero.
 * 
 * This stores the handle to the output file.
 */
static FILE *m_wavwrite_pf = NULL;

/*
 * The total number of bytes that have been written to the output file.
 * 
 * This is only valid if m_wavwrite_closed is zero and m_wavwrite_flags
 * is non-zero.
 * 
 * This may not exceed WAVWRITE_MAXFILE.
 */
static unsigned long m_wavwrite_bytes = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void wavwrite_byte(int v);
static void wavwrite_uword(unsigned int v);
static void wavwrite_word(int v);
static void wavwrite_dword(unsigned long v);

/*
 * Write an unsigned 8-bit integer value to output.
 * 
 * The wavwrite module must be initialized but not yet closed.  This
 * function will update the m_wavwrite_bytes count of bytes written,
 * with a fault occuring if it would overflow WAVWRITE_MAXFILE.  The
 * data is written to the current file pointer.
 * 
 * The provided value may not exceed WAVWRITE_U8MAX.
 * 
 * Parameters:
 * 
 *   v - the unsigned byte value to write
 */
static void wavwrite_byte(int v) {
  
  /* Check state */
  if (m_wavwrite_closed || (m_wavwrite_flags == 0)) {
    abort();
  }
  
  /* Check parameter */
  if ((v < 0) || (v > WAVWRITE_U8MAX)) {
    abort();
  }
  
  /* Increment byte count, watching for overflow */
  if (m_wavwrite_bytes < WAVWRITE_MAXFILE) {
    m_wavwrite_bytes++;
  } else {
    abort();  /* File length overflow */
  }
  
  /* Write the byte */
  if (putc(v, m_wavwrite_pf) != v) {
    abort();  /* I/O error */
  }
}

/*
 * Write an unsigned 16-bit integer value to output.
 * 
 * The wavwrite module must be initialized but not yet closed.  This
 * function will update the m_wavwrite_bytes count of bytes written,
 * with a fault occuring if it would overflow WAVWRITE_MAXFILE.  The
 * data is written to the current file pointer.
 * 
 * The integer is written as two bytes in little endian order regardless
 * of what platform the program is compiled on.
 * 
 * This function is a wrapper around wavwrite_byte().  It converts
 * 16-bit values into two bytes in little endian order.
 * 
 * The provided value may not exceed WAVWRITE_U16MAX.
 * 
 * Parameters:
 * 
 *   v - the unsigned word value to write
 */
static void wavwrite_uword(unsigned int v) {
  
  /* Check range */
  if (v > WAVWRITE_U16MAX) {
    abort();
  }
  
  /* Write bytes in little endian order */
  wavwrite_byte((int) (v & 0xff));
  v >>= 8;
  
  wavwrite_byte((int) (v & 0xff));
}

/*
 * Write a signed 16-bit integer value to output.
 * 
 * The wavwrite module must be initialized but not yet closed.  This
 * function will update the m_wavwrite_bytes count of bytes written,
 * with a fault occuring if it would overflow WAVWRITE_MAXFILE.  The
 * data is written to the current file pointer.
 * 
 * The integer is written as two bytes in little endian order with
 * negative values represented in two's complement, regardless of what
 * platform the program is compiled on.
 * 
 * This function is a wrapper around wavwrite_uword().  It simply
 * converts negative values into unsigned values represented in two's
 * complement.
 * 
 * The provided value must be in range [-32767, 32767].  A fault occurs
 * if -32768 is passed, even though this has a two's complement
 * representation.
 * 
 * Parameters:
 * 
 *   v - the signed word value to write
 */
static void wavwrite_word(int v) {
  
  long lv = 0;
  
  /* Check range */
  if (!((v >= WAVWRITE_S16MIN) && (v <= WAVWRITE_S16MAX))) {
    abort();
  }
  
  /* If non-negative, set lv to v; else, convert negative value to
   * positive unsigned in two's complement */
  if (v >= 0) {
    lv = v;
  } else {
    lv = WAVWRITE_U16MAX + ((long) v) + 1;
  }
  
  /* Pass through */
  wavwrite_uword((unsigned int) lv);
}

/*
 * Write an unsigned 32-bit integer value to output.
 * 
 * The wavwrite module must be initialized but not yet closed.  This
 * function will update the m_wavwrite_bytes count of bytes written,
 * with a fault occuring if it would overflow WAVWRITE_MAXFILE.  The
 * data is written to the current file pointer.
 * 
 * The integer is written as four bytes in little endian order
 * regardless of what platform the program is compiled on.
 * 
 * This function is a wrapper around wavwrite_byte().  It converts
 * 32-bit values into four bytes in little endian order.
 * 
 * The provided value may not exceed WAVWRITE_U32MAX.
 * 
 * Parameters:
 * 
 *   v - the doubleword value to write
 */
static void wavwrite_dword(unsigned long v) {
  
  /* Check range */
  if (v > WAVWRITE_U32MAX) {
    abort();
  }
  
  /* Write bytes in little endian order */
  wavwrite_byte((int) (v & 0xff));
  v >>= 8;
  
  wavwrite_byte((int) (v & 0xff));
  v >>= 8;
  
  wavwrite_byte((int) (v & 0xff));
  v >>= 8;
  
  wavwrite_byte((int) (v & 0xff));
}

/*
 * Public functions
 * ================
 * 
 * See the header for specifications.
 */

/*
 * wavwrite_init function.
 */
int wavwrite_init(const char *pPath, int flags) {
  
  int status = 1;
  FILE *pf = NULL;
  size_t psz = 0;
  unsigned int chcount = 0;
  unsigned int blockalign = 0;
  unsigned long samprate = 0;
  unsigned long bpsec = 0;
  
  /* Check state */
  if ((m_wavwrite_closed) || (m_wavwrite_flags != 0)) {
    abort();
  }
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  if ((!(flags & WAVWRITE_INIT_44100)) && 
      (!(flags & WAVWRITE_INIT_48000))) {
    abort();
  }
  if ((!(flags & WAVWRITE_INIT_MONO)) &&
      (!(flags & WAVWRITE_INIT_STEREO))) {
    abort();
  }
  if ((flags & WAVWRITE_INIT_44100) && (flags & WAVWRITE_INIT_48000)) {
    abort();
  }
  if ((flags & WAVWRITE_INIT_MONO) && (flags & WAVWRITE_INIT_STEREO)) {
    abort();
  }
  
  /* Determine channel count and sample rate from flags */
  if (flags & WAVWRITE_INIT_STEREO) {
    chcount = 2;
  } else if (flags & WAVWRITE_INIT_MONO) {
    chcount = 1;
  } else {
    abort();  /* Unknown channel count */
  }
  
  if (flags & WAVWRITE_INIT_44100) {
    samprate = 44100UL;
  } else if (flags & WAVWRITE_INIT_48000) {
    samprate = 48000UL;
  } else {
    abort();  /* Unknown sample rate */
  }
  
  /* Compute block alignment and samples per second */
  blockalign = chcount * 2;
  bpsec = ((unsigned long) blockalign) * samprate;
  
  /* Create the output file */
  pf = fopen(pPath, "wb");
  if (pf == NULL) {
    status = 0;
  }
  
  /* Proceed with initialization if file open successful; else, move
   * directly to closed state */
  if (status) {
  
    /* Initialize the variables */
    m_wavwrite_flags = flags;
    m_wavwrite_pf = pf;
    m_wavwrite_bytes = 0;
    
    /* Make a copy of the path */
    psz = strlen(pPath);
    m_wavwrite_pPath = (char *) malloc(psz + 1);
    if (m_wavwrite_pPath == NULL) {
      abort();
    }
    strcpy(m_wavwrite_pPath, pPath);
  
    /* Write the WAV header, setting the file length and data length
     * fields to zero for now (they will be filled in when the module is
     * closed) */
    wavwrite_dword(0x46464952UL);   /* "RIFF" in little endian */
    wavwrite_dword(0);              /* (File length field) */
    wavwrite_dword(0x45564157UL);   /* "WAVE" in little endian */
    
    wavwrite_dword(0x20746d66UL);   /* "fmt " in little endian */
    wavwrite_dword(16);             /* Size of fmt chunk */
    wavwrite_uword(1);              /* PCM/uncompressed */
    wavwrite_uword(chcount);        /* Channel count */
    wavwrite_dword(samprate);       /* Sample rate */
    wavwrite_dword(bpsec);          /* Average bytes per second */
    wavwrite_uword(blockalign);     /* Block alignment */
    wavwrite_uword(16);             /* Bits per channel sample */
    
    wavwrite_dword(0x61746164UL);   /* "data" in little endian */
    wavwrite_dword(0);              /* (Data length field) */
  
  } else {
    /* Open failed, so move to closed state */
    m_wavwrite_closed = 1;
  }
  
  /* Return status */
  return status;
}

/*
 * wavwrite_close function.
 */
void wavwrite_close(int flags) {
  
  unsigned long total_len = 0;
  
  /* Determine what to do from the state (do nothing if already
   * closed) */
  if ((!m_wavwrite_closed) && (m_wavwrite_flags != 0) &&
      (flags & WAVWRITE_CLOSE_RMFILE)) {
    /* Close down, removing output file -- first, close output file */
    fclose(m_wavwrite_pf);
    m_wavwrite_pf = NULL;
    
    /* Remove the file */
    remove(m_wavwrite_pPath);
    
    /* Free the path and set the closed flag */
    free(m_wavwrite_pPath);
    m_wavwrite_pPath = NULL;
    
    m_wavwrite_closed = 1;
    
  } else if ((!m_wavwrite_closed) && (m_wavwrite_flags != 0)) {
    /* Close down normally -- first, get the total file length */
    total_len = m_wavwrite_bytes;
    
    /* Clear byte count to zero since we will now be performing random
     * access and we don't have to worry about overflow anymore */
    m_wavwrite_bytes = 0;
    
    /* Seek to the file length field */
    if (fseek(m_wavwrite_pf, 4, SEEK_SET)) {
      abort();  /* I/O error */
    }
    
    /* Write total file length minus eight to the file length field */
    wavwrite_dword(total_len - 8);
    
    /* Seek to the data length field */
    if (fseek(m_wavwrite_pf, 40, SEEK_SET)) {
      abort();  /* I/O error */
    }
    
    /* Write total file length minus the 44-byte header to the data
     * length field */
    wavwrite_dword(total_len - 44);
    
    /* WAV file is now complete, so close it */
    fclose(m_wavwrite_pf);
    m_wavwrite_pf = NULL;
    
    /* Free the path and set the closed flag */
    free(m_wavwrite_pPath);
    m_wavwrite_pPath = NULL;
    
    m_wavwrite_closed = 1;
    
  } else if (!m_wavwrite_closed) {
    /* Never was initialized, so just set closed flag */
    m_wavwrite_closed = 1;
  }
}

/*
 * wavwrite_sample function.
 */
void wavwrite_sample(int left, int right) {
  
  /* Check state */
  if (m_wavwrite_closed || (m_wavwrite_flags == 0)) {
    abort();
  }
  
  /* Clamp sample values to range */
  if (left < WAVWRITE_S16MIN) {
    left = WAVWRITE_S16MIN;
  } else if (left > WAVWRITE_S16MAX) {
    left = WAVWRITE_S16MAX;
  }
  
  if (right < WAVWRITE_S16MIN) {
    right = WAVWRITE_S16MIN;
  } else if (right > WAVWRITE_S16MAX) {
    right = WAVWRITE_S16MAX;
  }
  
  /* If in mono-aural mode, make sure left and right are equal */
  if (m_wavwrite_flags & WAVWRITE_INIT_MONO) {
    if (left != right) {
      abort();
    }
  }
  
  /* Write samples depending on mode */
  if (m_wavwrite_flags & WAVWRITE_INIT_STEREO) {
    /* Stereo mode, so write left then right channel */
    wavwrite_word(left);
    wavwrite_word(right);
    
  } else if (m_wavwrite_flags & WAVWRITE_INIT_MONO) {
    /* Mono-aural mode, so write the sample */
    wavwrite_word(left);
    
  } else {
    /* Invalid flag state */
    abort();
  }
}
