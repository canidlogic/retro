/*
 * retro.c
 * =======
 * 
 * The Retro synthesizer.
 * 
 * Compilation
 * -----------
 * 
 * May require the math library to be included (-lm).
 * 
 * Requires the other retro modules to be compiled with it.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POSIX headers for mapWAV() and unmapWAV() requirements */
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* Retro modules */
#include "adsr.h"
#include "graph.h"

/*
 * Local data
 * ==========
 */

/*
 * The name of this executable module, for use in diagnostic messages.
 * 
 * This is set at the start of the program entrypoint.
 */
static const char *pModule = NULL;

/*
 * The handle to the source wave file and the pointer to the mapped
 * data.
 * 
 * m_srcwav_h is -1 if nothing currently mapped, else the file
 * descriptor for the mapped file.
 * 
 * m_srcwav_p is a pointer to the read-only memory-mapped data only if
 * m_srcwav_h is not -1.
 * 
 * m_srcwav_len is the length in bytes of the memory-mapped data onlf if
 * m_srcwav_h is not -1.  If a file is mapped, this will always be
 * greater than zero.
 */
static int m_srcwav_h = -1;
static uint8_t *m_srcwav_p = NULL;
static int32_t m_srcwav_len = 0;

/*
 * Location information for samples in the mapped source wave file.
 * 
 * If m_src_head is -1 then location information is not loaded.
 * Location information can only be loaded when a source wave file is
 * mapped, and location information is automatically cleared when the
 * source wave file is unmapped.
 * 
 * Otherwise, m_src_head is the number of header bytes in the WAV file
 * before the raw sample data begins, and m_src_samp is the total number
 * of samples stored in the file, which will be greater than zero.
 * Also, m_src_rate is the sampling rate in the source, which will be
 * either 48000 or 44100.
 */
static int32_t m_src_head = -1;
static int32_t m_src_samp = -1;
static int32_t m_src_rate = -1;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int loadWAV(void);

static int32_t readInt32BE(uint8_t *pb);
static int32_t readInt32LE(uint8_t *pb);
static int readInt16LE(uint8_t *pb);

static int mapWAV(const char *pPath);
static void unmapWAV(void);

/*
 * Fill in location information for a currently mapped WAV file.
 * 
 * You must have a WAV file mapped with mapWAV() or a fault occurs.  If
 * this file has already been loaded, this call is ignored.
 * 
 * The m_src_ variables will be filled in properly.  If this function
 * fails, the m_src_ variables will be in their cleared state, but the
 * source WAV file remains mapped.
 * 
 * Unmapping the source WAV file with unmapWAV() automatically clears
 * the location information.
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int loadWAV(void) {
  
  int status = 1;
  int32_t wl = 0;
  int32_t ip = 0;
  int32_t ckt = 0;
  int32_t ckl = 0;
  int32_t fmt_offs = -1;
  int32_t fmt_size = -1;
  int32_t data_offs = -1;
  int32_t data_size = -1;
  int32_t samp_rate = 0;
  
  /* Check that WAV file is mapped */
  if (m_srcwav_h == -1) {
    abort();
  }
  
  /* Only proceed if not yet loaded */
  if (m_src_head == -1) {
    
    /* There must be at least twelve bytes to hold the RIFF chunk
     * header */
    if (m_srcwav_len < 12) {
      status = 0;
      fprintf(stderr, "%s: Source WAV missing RIFF header!\n", pModule);
    }
    
    /* Check the signature types */
    if (status) {
      if (readInt32BE(m_srcwav_p) != INT32_C(0x52494646)) {
        status = 0;
        fprintf(stderr, "%s: Source WAV not a RIFF file!\n", pModule);
      }
    }
    if (status) {
      if (readInt32BE(m_srcwav_p + 8) != INT32_C(0x57415645)) {
        status = 0;
        fprintf(stderr, "%s: Source WAV not a WAVE file!\n", pModule);
      }
    }
    
    /* Get the RIFF length from the RIFF header and check that it is at
     * least four (to include the WAVE word) and no more than eight less
     * than the full mapped file length (it does not count the RIFF and
     * length words at the start) */
    if (status) {
      wl = readInt32LE(m_srcwav_p + 4);
      if ((wl < 4) || (wl > m_srcwav_len - 8)) {
        status = 0;
        fprintf(stderr, "%s: Source declared RIFF size wrong!\n",
          pModule);
      }
    }
    
    /* Iterate through all the chunks within the RIFF block to get the
     * location of the "fmt " chunk and the "data" chunk */
    if (status) {
      /* Start immediately after the WAVE declaration in the RIFF
       * chunk */
      ip = 4;
      
      /* Keep iterating while we haven't reached the end of the RIFF
       * chunk */
      while (ip < wl) {
        /* We should have at least eight bytes left to read another
         * chunk header */
        if (wl - ip < 8) {
          status = 0;
          fprintf(stderr, "%s: Source chunk iteration failed!\n",
            pModule);
          break;
        }
        
        /* Read the chunk type and chunk size */
        ckt = readInt32BE(m_srcwav_p + 8 + ip);
        ckl = readInt32LE(m_srcwav_p + 8 + ip + 4);
        
        /* Chunk size must not be negative and must not exceed remaining
         * space in the RIFF chunk */
        if ((ckl < 0) || (ckl > wl - ip - 8)) {
          status = 0;
          fprintf(stderr, "%s: Source chunk iteration failed!\n",
            pModule);
          break;
        }
        
        /* If this is one of the desired chunks, update statistics */
        if (ckt == INT32_C(0x666d7420)) {
          /* fmt chunk found -- check that not already recorded */
          if (fmt_offs >= 0) {
            status = 0;
            fprintf(stderr, "%s: Multiple fmt chunks in source!\n",
              pModule);
            break;
          }
          
          /* Store format chunk information */
          fmt_offs = 8 + ip + 8;
          fmt_size = ckl;
          
        } else if (ckt == INT32_C(0x64617461)) {
          /* data chunk found -- check that not already recorded */
          if (data_offs >= 0) {
            status = 0;
            fprintf(stderr, "%s: Multiple data chunks in source!\n",
              pModule);
            break;
          }
          
          /* Store data chunk information */
          data_offs = 8 + ip + 8;
          data_size = ckl;
        }
        
        /* Chunks are 16-bit aligned, so if least significant bit of the
         * chunk size is set, increment chunk size */
        if (ckl & 0x1) {
          ckl++;
        }
        
        /* Move beyond this chunk */
        ip = ip + 8 + ckl;
      }
    }
    
    /* Check that we found both the fmt and data chunks */
    if (status && (fmt_offs < 0)) {
      status = 0;
      fprintf(stderr, "%s: Failed to find source fmt chunk!\n",
        pModule);
    }
    
    if (status && (data_offs < 0)) {
      status = 0;
      fprintf(stderr, "%s: Failed to find source data chunk!\n",
        pModule);
    }
    
    /* Must be at least 16 bytes in fmt chunk */
    if (status && (fmt_size < 16)) {
      status = 0;
      fprintf(stderr, "%s: Source WAV file is not uncompressed PCM!\n",
        pModule);
    }
    
    /* Check that uncompressed, 16-bit, single-channel */
    if (status) {
      if (readInt16LE(m_srcwav_p + fmt_offs) != 1) {
        status = 0;
        fprintf(stderr, "%s: Source WAV file is compressed!\n",
          pModule);
      }
    }
    
    if (status) {
      if (readInt16LE(m_srcwav_p + fmt_offs + 14) != 16) {
        status = 0;
        fprintf(stderr, "%s: Source WAV file is not 16-bit!\n",
          pModule);
      }
    }
    
    if (status) {
      if (readInt16LE(m_srcwav_p + fmt_offs + 2) != 1) {
        status = 0;
        fprintf(stderr, "%s: Source WAV file is not single-channel!\n",
          pModule);
      }
    }
    
    /* Get sampling rate and make sure either 48000 or 44100 */
    if (status) {
      samp_rate = readInt32LE(m_srcwav_p + fmt_offs + 4);
      if ((samp_rate != 48000) && (samp_rate != 44100)) {
        status = 0;
        fprintf(stderr,
          "%s: Source WAV file has unsupported sample rate: %ld!\n",
          pModule, (long) samp_rate);
      }
    }
    
    /* Make sure data size is divisible by two */
    if (status && ((data_size % 2) != 0)) {
      status = 0;
      fprintf(stderr, "%s: Source data chunk has invalid size!\n",
        pModule);
    }
    
    /* Update location information */
    if (status) {
      m_src_head = data_offs;
      m_src_samp = data_size / 2;
      m_src_rate = samp_rate;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Read four bytes in big endian order into a signed 32-bit integer.
 * 
 * Parameters:
 * 
 *   pb - pointer to the four bytes to interpret
 * 
 * Return:
 * 
 *   the signed 32-bit integer
 */
static int32_t readInt32BE(uint8_t *pb) {
  
  int64_t uiv = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Assemble the full unsigned value */
  uiv = (((int64_t) pb[0]) << 24) |
        (((int64_t) pb[1]) << 16) |
        (((int64_t) pb[2]) <<  8) |
         ((int64_t) pb[3]);
  
  /* If most significant bit of 32-bit unsigned value is set, then use
   * the two's-complement scheme to convert it to a signed value */
  if (uiv & INT64_C(0x80000000)) {
    uiv = uiv - INT64_C(0x100000000);
  }
  
  /* Return cast to 32-bit */
  return (int32_t) uiv;
}

/*
 * Read four bytes in little endian order into a signed 32-bit integer.
 * 
 * Parameters:
 * 
 *   pb - pointer to the four bytes to interpret
 * 
 * Return:
 * 
 *   the signed 32-bit integer
 */
static int32_t readInt32LE(uint8_t *pb) {
  
  int64_t uiv = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Assemble the full unsigned value */
  uiv = (((int64_t) pb[3]) << 24) |
        (((int64_t) pb[2]) << 16) |
        (((int64_t) pb[1]) <<  8) |
         ((int64_t) pb[0]);
  
  /* If most significant bit of 32-bit unsigned value is set, then use
   * the two's-complement scheme to convert it to a signed value */
  if (uiv & INT64_C(0x80000000)) {
    uiv = uiv - INT64_C(0x100000000);
  }
  
  /* Return cast to 32-bit */
  return (int32_t) uiv;
}

/*
 * Read two bytes in little endian order into a signed 16-bit integer.
 * 
 * Parameters:
 * 
 *   pb - pointer to the two bytes to interpret
 * 
 * Return:
 * 
 *   the signed 16-bit integer
 */
static int readInt16LE(uint8_t *pb) {
  
  int32_t uiv = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Assemble the full unsigned value */
  uiv = (((int32_t) pb[1]) << 8) | ((int32_t) pb[0]);
  
  /* If most significant bit of 16-bit unsigned value is set, then use
   * the two's-complement scheme to convert it to a signed value */
  if (uiv & INT32_C(0x8000)) {
    uiv = uiv - INT64_C(0x10000);
  }
  
  /* Return cast to 16-bit */
  return (int16_t) uiv;
}

/*
 * Map a WAV file entirely into memory for reading.
 * 
 * pPath is the path to the WAV file to map.
 * 
 * This call begins by an automatic call to unmapWAV() to unmap anything
 * that might be currently loaded.  Then, it attempts to map the given
 * file entirely into memory, setting the m_srcwv_ variables
 * appropriately.  If the function fails, the variables will be set to
 * unloaded state.
 * 
 * Parameters:
 * 
 *   pPath - the path to the file to map into memory
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int mapWAV(const char *pPath) {
  
  int status = 1;
  int fd = -1;
  void *pd = MAP_FAILED;
  int32_t fl = 0;
  struct stat fi;
  
  /* Initialize structures */
  memset(&fi, 0, sizeof(struct stat));
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  
  /* Unmap anything currently mapped */
  unmapWAV();
  
  /* Open the file for reading */
  fd = open(pPath, O_RDONLY);
  if (fd == -1) {
    status = 0;
    fprintf(stderr, "%s: Failed to open %s\n", pModule, pPath);
  }
  
  /* Get information about file from handle */
  if (status) {
    if (fstat(fd, &fi)) {
      status = 0;
      fprintf(stderr, "%s: Failed to stat %s\n", pModule, pPath);
    }
  }
  
  /* Check that size is within range and get it into integer */
  if (status) {
    if (fi.st_size < 1) {
      status = 0;
      fprintf(stderr, "%s: Source file is empty: %s\n", pModule, pPath);
    }
  }
  if (status) {
    if (fi.st_size > INT32_MAX) {
      status = 0;
      fprintf(stderr, "%s: Source file is too large: %s\n",
        pModule, pPath);
    }
  }
  if (status) {
    fl = (int32_t) fi.st_size;
  }
  
  /* Map file into memory */
  if (status) {
    pd = mmap(NULL, (size_t) fl, PROT_READ, MAP_PRIVATE, fd, 0);
    if (pd == MAP_FAILED) {
      status = 0;
      fprintf(stderr, "%s: Failed to memory-map %s\n", pModule, pPath);
    }
  }
  
  /* Update module variables */
  if (status) {
    m_srcwav_h = fd;
    m_srcwav_p = (uint8_t *) pd;
    m_srcwav_len = fl;
  }
  
  /* If we failed, unmap the view if mapped */
  if ((!status) && (pd != MAP_FAILED)) {
    if (munmap(pd, (size_t) fl)) {
      fprintf(stderr, "%s: Failed to unmap view!\n", pModule);
    }
  }
  
  /* If we failed, close file handle if open */
  if ((!status) && (fd != -1)) {
    if (close(fd)) {
      fprintf(stderr, "%s: Failed to close descriptor!\n", pModule);
    }
  }
  
  /* Return status */
  return status;
}

/*
 * If any source WAV file is mapped into memory, unmap and close it.
 */
static void unmapWAV(void) {
  
  /* Only proceed if something is mapped */
  if (m_srcwav_h != -1) {
    
    /* Clear location information */
    m_src_head = -1;
    m_src_samp = -1;
    m_src_rate = -1;
    
    /* Unmap view and clear length count */
    if (munmap(m_srcwav_p, (size_t) m_srcwav_len)) {
      fprintf(stderr, "%s: Failed to unmap view!\n", pModule);
    }
    m_srcwav_p = NULL;
    m_srcwav_len = 0;
    
    /* Close descriptor */
    if (close(m_srcwav_h)) {
      fprintf(stderr, "%s: Failed to close descriptor!\n", pModule);
    }
    m_srcwav_h = -1;
  }
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  /* @@TODO: */
  GRAPH_OBJ *pg = NULL;
  int32_t x = 0;
  
  /* Set executable name */
  pModule = NULL;
  if ((argc > 0) && (argv != NULL)) {
    pModule = argv[0];
  }
  if (pModule == NULL) {
    pModule = "retro";
  }
  
  /* Check that no additional parameters received */
  if (argc > 1) {
    status = 0;
    fprintf(stderr, "%s: Not expecting program arguments!\n", pModule);
  }
  
  /* @@TODO: */
  if (status) {
    mapWAV("test/triwav_first.wav");
    loadWAV();
    unmapWAV();
    
    pg = graph_alloc(5);
    graph_pushRough(pg, 7, 10, 50);
    graph_pushSmooth(pg, 15, -12);
    graph_pushSmooth(pg, 5, -12);
    graph_pushRough(pg, 10, 0, 5);
    
    for(x = 0; x < 40; x++) {
      printf("%d\n", (int) graph_get(pg, x));
    }
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
