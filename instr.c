/*
 * instr.c
 * 
 * Implementation of instr.h
 * 
 * See the header for further information.
 */

#include "instr.h"
#include "os.h"
#include "genmap.h"

#include "shastina.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * Instrument type constants.
 * 
 * The "NULL" instrument is used for instrument registers that are
 * cleared.
 */
#define ITYPE_NULL      (0)
#define ITYPE_SQUARE    (1)
#define ITYPE_FM        (2)

/*
 * The maximum number of entries that can be added to the search chain.
 * 
 * This does NOT include the default search entries.
 */
#define MAX_SEARCH_LINK (256)

/*
 * The maximum length, including terminating nul, of the buffer to use
 * for building paths to instrument files.
 */
#define MAX_SEARCH_BUF  (4096)

/*
 * Type declarations
 * =================
 */

/*
 * SEARCH_LINK structure for entries on the search path chain.
 * 
 * Preceded by a structure prototype so the structure can
 * self-reference.
 */
struct SEARCH_LINK_TAG;
typedef struct SEARCH_LINK_TAG SEARCH_LINK;
struct SEARCH_LINK_TAG {
  
  /*
   * Pointer to next entry on search path chain, or NULL if last entry.
   */
  SEARCH_LINK *pNext;
  
  /*
   * The directory path of this search entry.
   * 
   * The string is nul-terminated and extends beyond the end of the
   * structure.
   */
  char path[1];
};

/*
 * Structure storing instrument settings for FM instrument types.
 */
typedef struct {
  
  /*
   * Pointer to the root generator in the generator map.
   * 
   * The generator map should already be bound.  This structure will
   * hold a reference to this generator.
   */
  GENERATOR *pRoot;
  
  /*
   * The number of instance data structures required.
   * 
   * Must be one or greater.
   */
  int32_t icount;
  
} FM_PARAM;

/*
 * The instrument register structure.
 */
typedef struct {
  
  /*
   * The maximum and minimum intensities for this instrument.
   * 
   * If both are zero, the register is cleared.
   * 
   * Both must be in range [0, MAX_FRAC].  Furthermore, i_max must be
   * greater than or equal to i_min.
   */
  uint16_t i_max;
  uint16_t i_min;
  
  /*
   * The stereo position of this instrument.
   * 
   * This is only valid if the register is not cleared.
   */
  STEREO_POS sp;
  
  /*
   * The instrument type.
   * 
   * One of the ITYPE_ constants.
   */
  int itype;
  
  /*
   * The "val" union has data specific to the instance type.
   * 
   * The itype field determines what is stored here.
   */
  union {
  
    /*
     * Dummy value used for NULL instruments.
     */
    int dummy;
  
    /*
     * Pointer to the envelope object, used for SQUARE instruments.
     * 
     * The register must have a reference to the envelope object.
     */
    ADSR_OBJ *pa;
    
    /*
     * FM instrument parameters.
     */
    FM_PARAM fmp;
  
  } val;
  
} INSTR_REG;

/*
 * Static data
 * ===========
 */

/*
 * The search link chain.
 * 
 * Use instr_chaininit() to initialize this chain with default values if
 * it is empty.
 */
static SEARCH_LINK *m_instr_search = NULL;

/*
 * The sampling rate, or 0 if not set yet.
 */
static int32_t m_instr_rate = 0;

/*
 * Flag indicating whether the instrument register table has been
 * initialized yet.
 * 
 * Use instr_t_init() to initialize the table.
 */
static int m_instr_init = 0;

/*
 * The instrument register table.
 * 
 * Only valid if m_instr_init flag indicates the table has been
 * initialized.
 */
static INSTR_REG m_instr_t[INSTR_MAXCOUNT];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void instr_chaininit(void);

static void instr_t_init(void);
static INSTR_REG *instr_ptr(int32_t i);
static int instr_isclear(const INSTR_REG *pr);

static int instr_load(
    int32_t    i,
    SNSOURCE * pIn,
    int      * per,
    int      * per_src,
    long     * pline);

/*
 * Initialize the search chain with default values if it is empty.
 * 
 * If m_instr_search has at least one element, this function does
 * nothing.  If the chain is empty, this function will initialize the
 * chain with the following, with (2) being at the end of the chain:
 * 
 *   1. "./retro_lib"
 *   2. "~/retro_lib" where ~ is the user home directory
 * 
 * If the user home directory can't be determined, element (2) is not
 * added to the chain.
 * 
 * This function does NOT check whether the directories actually exist.
 */
static void instr_chaininit(void) {
  
  static const char *pSubdir = "retro_lib";
  char *pHome = NULL;
  int32_t full_len = 0;
  int32_t new_len = 0;
  SEARCH_LINK *pl = NULL;
  char cb[2];
  
  /* Initialize buffer */
  memset(&(cb[0]), 0, 2);
  
  /* Only proceed if chain not initialized */
  if (m_instr_search == NULL) {
    
    /* Get the home directory, if possible */
    pHome = os_gethome();
    
    /* If we got the home directory, add an entry for the retro_lib
     * subdirectory */
    if (pHome != NULL) {
      
      /* Get the length of the home directory */
      full_len = (int32_t) strlen(pHome);
      
      /* Get the length of the subdirectory, including separator */
      new_len = (int32_t) (strlen(pSubdir) + 1);
      
      /* Only proceed if full length can be computed, including space
       * for the link structure, which includes the terminating nul */
      if (full_len <=
              INT32_MAX - new_len - ((int32_t) sizeof(SEARCH_LINK))) {
        
        /* Compute full length of path string, including nul */
        full_len = full_len + new_len + 1;
        
        /* Compute full length of link structure */
        new_len = (full_len - 1) + ((int32_t) sizeof(SEARCH_LINK));
        
        /* Allocate link structure */
        pl = (SEARCH_LINK *) malloc((size_t) new_len);
        if (pl == NULL) {
          abort();
        }
        memset(pl, 0, (size_t) new_len);
        
        /* Set next pointer to NULL */
        pl->pNext = NULL;
        
        /* Copy in the home directory */
        strcpy(&((pl->path)[0]), pHome);
        
        /* Append the directory separator character */
        cb[0] = (char) os_getsep();
        strcat(&((pl->path)[0]), &(cb[0]));
        
        /* Append the subdirectory */
        strcat(&((pl->path)[0]), pSubdir);
        
        /* Add the link to the start of the chain */
        m_instr_search = pl;
        pl = NULL;
      }
    }
    
    /* Compute full length of current directory version of retro_lib,
     * including terminating nul */
    full_len = ((int32_t) strlen(pSubdir)) + 3;
    
    /* Compute full length of link structure */
    new_len = (full_len - 1) + ((int32_t) sizeof(SEARCH_LINK));
    
    /* Allocate link structure */
    pl = (SEARCH_LINK *) malloc((size_t) new_len);
    if (pl == NULL) {
      abort();
    }
    memset(pl, 0, (size_t) new_len);
    
    /* Set next pointer to current value of search chain */
    pl->pNext = m_instr_search;
    
    /* Begin with the period character followed by separator */
    (pl->path)[0] = (char) '.';
    (pl->path)[1] = (char) os_getsep();
    
    /* Append subdirectory name */
    strcat(&((pl->path)[0]), pSubdir);
    
    /* Add the link to the start of the chain */
    m_instr_search = pl;
    pl= NULL;
    
    /* Free home path copy, if allocated */
    if (pHome != NULL) {
      free(pHome);
      pHome = NULL;
    }
  }
}

/*
 * Initialize the instrument register table if not already initialized.
 * 
 * If the table has already been initialized, this function has no
 * effect.
 */ 
static void instr_t_init(void) {
  
  int32_t x = 0;
  
  /* Only proceed if not initialized */
  if (!m_instr_init) {
    
    /* Clear table */
    memset(m_instr_t, 0, INSTR_MAXCOUNT * sizeof(INSTR_REG));
    
    /* Set all records to clear */
    for(x = 0; x < INSTR_MAXCOUNT; x++) {
      (m_instr_t[x]).i_max = 0;
      (m_instr_t[x]).i_min = 0;
      (m_instr_t[x]).itype = ITYPE_NULL;
      (m_instr_t[x]).val.dummy = 0;
    }
    
    /* Set initialized flag */
    m_instr_init = 1;
  }
}

/*
 * Get a pointer to the given instrument register.
 * 
 * The instrument register table is initialized if necessary by this
 * function.
 * 
 * This function range-checks i, faulting if out of range.
 * 
 * Parameters:
 * 
 *   i - the instrument register to retrieve
 * 
 * Return:
 * 
 *   a pointer to the instrument register
 */
static INSTR_REG *instr_ptr(int32_t i) {
  
  /* Initialize instrument register table if necessary */
  instr_t_init();
  
  /* Range-check parameter */
  if ((i < 0) || (i >= INSTR_MAXCOUNT)) {
    abort();
  }
  
  /* Return register */
  return &((m_instr_t)[i]);
}

/*
 * Check whether the given instrument register is clear.
 * 
 * Parameters:
 * 
 *   pr - pointer to the instrument register structure
 * 
 * Return:
 * 
 *   non-zero if clear, zero otherwise
 */
static int instr_isclear(const INSTR_REG *pr) {
  
  int result = 0;
  
  /* Check parameter */
  if (pr == NULL) {
    abort();
  }
  
  /* Check if clear */
  if ((pr->i_max == 0) && (pr->i_min == 0)) {
    result = 1;
  } else {
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * Load an instrument from a given Shastina source.
 * 
 * i must be in range [0, INSTR_MAXCOUNT - 1].  instr_clear() is run
 * automatically on the indicated register before the new instrument is
 * defined.
 * 
 * pIn is a Shastina source that contains the whole script for defining
 * the instrument.  It must support multipass.
 * 
 * The minimum intensity will be set to half and the maximum intensity
 * will be set to maximum possible value.  The stereo position will be
 * set to center.
 * 
 * per and pline are pointers to variables to receive an error code and
 * a line number if there is an error.  per_src points to an integer
 * to receive one of the INSTR_ERRMOD_ constants that indicates what
 * Retro module the error code comes from, which determines the meaning
 * of the error code.
 * 
 * Parameters:
 * 
 *   i - the instrument register
 * 
 *   pIn - the source for the instrument script
 * 
 *   per - pointer to variable to receive genmap error code
 * 
 *   per_src - the module from which the error number comes
 * 
 *   pline - pointer to variable to receive line number within script
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int instr_load(
    int32_t    i,
    SNSOURCE * pIn,
    int      * per,
    int      * per_src,
    long     * pline) {
  
  int status = 1;
  INSTR_REG *pr = NULL;
  GENMAP_RESULT gmr;
  
  /* Initialize structures */
  memset(&gmr, 0, sizeof(GENMAP_RESULT));
  gmr.pRoot = NULL;
  
  /* Check parameters */
  if ((i < 0) || (i >= INSTR_MAXCOUNT) ||
      (pIn == NULL) || (per == NULL) ||
      (per_src == NULL) || (pline == NULL)) {
    abort();
  }
  if (!snsource_ismulti(pIn)) {
    abort();
  }
  
  /* Check state */
  if (m_instr_rate == 0) {
    abort();
  }
  
  /* We currently only support genmap instruments, so call through */
  genmap_run(pIn, &gmr, m_instr_rate);
  
  /* Handle errors */
  if (gmr.errcode != GENMAP_OK) {
    status = 0;
    if (gmr.errcode < 0) {
      *per_src = INSTR_ERRMOD_SHASTINA;
    } else {
      *per_src = INSTR_ERRMOD_GENMAP;
    }
    *per = gmr.errcode;
    *pline = gmr.linenum;
  }
  
  /* If successful, set up the instrument */
  if (status) {
    /* Clear instrument register */
    instr_clear(i);
    
    /* Get instrument register */
    pr = instr_ptr(i);
   
    /* Setup default intensity and stereo position */
    pr->i_min = (MAX_FRAC / 2);
    pr->i_max = MAX_FRAC;
    stereo_setPos(&(pr->sp), 0);
    
    /* Store FM instrument definition */
    pr->itype = ITYPE_FM;
    (pr->val).fmp.pRoot = gmr.pRoot;
    generator_addref(gmr.pRoot);
    (pr->val).fmp.icount = gmr.icount;
  }
  
  /* Release object references */
  generator_release(gmr.pRoot);
  gmr.pRoot = NULL;
  
  /* Return status */
  return status;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * instr_addsearch function.
 */
int instr_addsearch(const char *pDir) {
  
  static int32_t el_count = 0;
  
  int status = 1;
  SEARCH_LINK *pl = NULL;
  int32_t full_len = 0;
  
  /* Initialize search chain with default values if needed */
  instr_chaininit();
  
  /* Only proceed if not too many elements */
  if (el_count < MAX_SEARCH_LINK) {
    /* Not too many search links, so increment count */
    el_count++;
    
    /* Compute full length of new link */
    full_len = (int32_t) strlen(pDir);
    if (full_len <= INT32_MAX - ((int32_t) sizeof(SEARCH_LINK))) {
      full_len = full_len + ((int32_t) sizeof(SEARCH_LINK));
    } else {
      abort();  /* overflow */
    }
    
    /* Allocate new link structure */
    pl = (SEARCH_LINK *) malloc((size_t) full_len);
    if (pl == NULL) {
      abort();
    }
    memset(pl, 0, (size_t) full_len);
    
    /* Set next pointer to current chain */
    pl->pNext = m_instr_search;
    
    /* Copy in directory */
    strcpy(&((pl->path)[0]), pDir);
    
    /* Prefix to chain */
    m_instr_search = pl;
    pl = NULL;
    
  } else {
    /* Too many search links */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * instr_setsamp function.
 */
void instr_setsamp(int32_t rate) {
  
  /* Check parameter */
  if ((rate != RATE_CD) && (rate != RATE_DVD)) {
    abort();
  }
  
  /* Check state */
  if (m_instr_rate != 0) {
    abort();
  }
  
  /* Store rate */
  m_instr_rate = rate;
}

/*
 * instr_clear function.
 */
void instr_clear(int32_t i) {
  
  INSTR_REG *pr = NULL;
  
  /* Get instrument register */
  pr = instr_ptr(i);
  
  /* Only proceed if instrument register not clear */
  if (!instr_isclear(pr)) {
    /* Release the instrument data */
    if (pr->itype == ITYPE_SQUARE) {
      /* Release square wave instrument */
      adsr_release((pr->val).pa);
      (pr->val).pa = NULL;
    
    } else if (pr->itype == ITYPE_FM) {
      /* Release FM instrument */
      generator_release((pr->val).fmp.pRoot);
      (pr->val).fmp.pRoot = NULL;
    
    } else {
      /* Shouldn't happen */
      abort();
    }
    
    /* Clear the record */
    memset(pr, 0, sizeof(INSTR_REG));
    
    /* Set the clear */
    pr->i_max = 0;
    pr->i_min = 0;
    pr->itype = ITYPE_NULL;
    (pr->val).dummy = 0;
  }
}

/*
 * instr_define function.
 */
void instr_define(
          int32_t      i,
          int32_t      i_max,
          int32_t      i_min,
          ADSR_OBJ   * pa,
    const STEREO_POS * psp) {
  
  INSTR_REG *pr = NULL;
  
  /* Get instrument register */
  pr = instr_ptr(i);
  
  /* Check parameters */
  if ((i_max < 0) || (i_min < 0) ||
      (i_max > MAX_FRAC) || (i_min > MAX_FRAC) ||
      (i_max < i_min)) {
    abort();
  }
  if ((pa == NULL) || (psp == NULL)) {
    abort();
  }
  
  /* Check for special case of both intensities zero */
  if ((i_max == 0) && (i_min == 0)) {
    /* Both intensities zero, so just clear the register */
    instr_clear(i);
    
  } else {
    /* At least one intensity non-zero, so begin by clearing the
     * register */
    instr_clear(i);
    
    /* Copy values in */
    pr->i_max = (uint16_t) i_max;
    pr->i_min = (uint16_t) i_min;
    pr->itype = ITYPE_SQUARE;
    (pr->val).pa = pa;
    adsr_addref(pa);
    memcpy(&(pr->sp), psp, sizeof(STEREO_POS));
  }
}

/*
 * instr_embedded function.
 */
int instr_embedded(
          int32_t   i,
    const char    * pText,
          int     * per,
          int     * per_src,
          long    * pline) {
  
  int status = 1;
  SNSOURCE *pIn = NULL;
  
  /* Check parameters */
  if ((i < 0) || (i >= INSTR_MAXCOUNT) ||
      (pText == NULL) || (per == NULL) ||
      (per_src == NULL) || (pline == NULL)) {
    abort();
  }
  
  /* Wrap string in source */
  pIn = snsource_string(pText);
  
  /* Load the instrument */
  if (!instr_load(i, pIn, per, per_src, pline)) {
    status = 0;
  }
  
  /* Release source if allocated */
  snsource_free(pIn);
  pIn = NULL;
  
  /* Return status */
  return status;
}

/*
 * instr_external function.
 */
int instr_external(
          int32_t   i,
    const char    * pCall,
          int     * per,
          int     * per_src,
          long    * pline) {
  
  const char *pExt = ".iretro";
  
  int status = 1;
  int file_found = 0;
  int32_t full_len = 0;
  int32_t x = 0;
  char *pc = NULL;
  char *pbuf = NULL;
  char *pt = NULL;
  SEARCH_LINK *pl = NULL;
  FILE *pHIn = NULL;
  SNSOURCE *pIn = NULL;
  char cb[2];
  
  /* Initialize buffers */
  memset(cb, 0, 2);
  
  /* Check parameters */
  if ((i < 0) || (i >= INSTR_MAXCOUNT) ||
      (pCall == NULL) || (per == NULL) ||
      (per_src == NULL) || (pline == NULL)) {
    abort();
  }
  
  /* Reset error information */
  *per = INSTR_ERR_OK;
  *per_src = INSTR_ERRMOD_INSTR;
  *pline = 0;
  
  /* Initialize search chain if necessary */
  instr_chaininit();
  
  /* Allocate path buffer */
  pbuf = (char *) malloc((size_t) MAX_SEARCH_BUF);
  if (pbuf == NULL) {
    abort();
  }
  memset(pbuf, 0, (size_t) MAX_SEARCH_BUF);
  
  /* Make a copy of the call number string */
  pc = (char *) malloc(strlen(pCall) + 1);
  if (pc == NULL) {
    abort();
  }
  strcpy(pc, pCall);
  
  /* Make sure call number string has at least one character and that
   * the first character is not a dot */
  if ((*pc == 0) || (*pc == '.')) {
    status = 0;
    *per = INSTR_ERR_BADCALL;
    *per_src = INSTR_ERRMOD_INSTR;
    *pline = 0;
  }
  
  /* Make sure only lowercase ASCII letters, decimal digits,
   * underscores, and periods are used, that no period appears
   * immediately before another period, and that the period is not the
   * last character */
  if (status) {
    for(pt = pc; *pt != 0; pt++) {
      if (((*pt < 'a') || (*pt > 'z')) &&
          ((*pt < '0') || (*pt > '9')) &&
          (*pt != '_') && (*pt != '.')) {
        status = 0;
      }
      if (*pt == '.') {
        if ((pt[1] == '.') || (pt[1] == 0)) {
          status = 0;
        }
      }
      if (!status) {
        break;
      }
    }
    if (!status) {
      *per = INSTR_ERR_BADCALL;
      *per_src = INSTR_ERRMOD_INSTR;
      *pline = 0;
    }
  }
  
  /* Change all periods to platform-specific separator */
  if (status) {
    for(pt = pc; *pt != 0; pt++) {
      if (*pt == '.') {
        *pt = (char) os_getsep();
      }
    }
  }
  
  /* Go through all base directories in the search path looking for the
   * instrument file */
  if (status) {
    for(pl = m_instr_search; pl != NULL; pl = pl->pNext) {
      
      /* Compute the full length of the path that will be constructed */
      full_len = (int32_t) strlen(pl->path);
      if (full_len <= INT32_MAX - 2) {
        full_len += 2;
      } else {
        abort();  /* overflow */
      }
      
      x = (int32_t) strlen(pc);
      if (x <= INT32_MAX - full_len) {
        full_len += x;
      }
      
      x = (int32_t) strlen(pExt);
      if (x <= INT32_MAX - full_len) {
        full_len += x;
      }
      
      /* Check that full length fits in path buffer */
      if (full_len >= MAX_SEARCH_BUF) {
        status = 0;
        *per = INSTR_ERR_HUGEPATH;
        *per_src = INSTR_ERRMOD_INSTR;
        *pline = 0;
      }
      
      /* Clear the path buffer */
      if (status) {
        memset(pbuf, 0, MAX_SEARCH_BUF);
      }
      
      /* Build the full path in the buffer */
      if (status) {
        strcpy(pbuf, pl->path);
        
        cb[0] = (char) os_getsep();
        strcat(pbuf, cb);
        
        strcat(pbuf, pc);
        strcat(pbuf, pExt);
      }
      
      /* Check if file exists; leave loop if it does and set
       * file_found flag */
      if (status) {
        if (os_isfile(pbuf)) {
          file_found = 1;
          break;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
    
    /* Fail if no instrument file found */
    if (status && (!file_found)) {
      status = 0;
      *per = INSTR_ERR_NOTFOUND;
      *per_src = INSTR_ERRMOD_INSTR;
      *pline = 0;
    }
  }
  
  /* If we got here, then we found an instrument file and its path is in
   * the pbuf buffer -- open a file handle to read it */
  if (status) {
    pHIn = fopen(pbuf, "rb");
    if (pHIn == NULL) {
      status = 0;
      *per = INSTR_ERR_OPEN;
      *per_src = INSTR_ERRMOD_INSTR;
      *pline = 0;
    }
  }
  
  /* Transfer the file handle into a Shastina source */
  if (status) {
    pIn = snsource_stream(pHIn, SNSTREAM_OWNER | SNSTREAM_RANDOM);
    pHIn = NULL;
  }
  
  /* Load instrument */
  if (status) {
    if (!instr_load(i, pIn, per, per_src, pline)) {
      status = 0;
    }
  }
  
  /* Close Shastina source if open */
  snsource_free(pIn);
  pIn = NULL;
  
  /* Close file if open */
  if (pHIn != NULL) {
    fclose(pHIn);
    pHIn = NULL;
  }
  
  /* Release path buffer if allocated */
  if (pbuf != NULL) {
    free(pbuf);
    pbuf = NULL;
  }
  
  /* Release copy of call number if allocated */
  if (pc != NULL) {
    free(pc);
    pc = NULL;
  }
  
  /* Return status */
  return status;
}

/*
 * instr_dup function.
 */
void instr_dup(int32_t i_target, int32_t i_src) {
  
  INSTR_REG *ps = NULL;
  INSTR_REG *pt = NULL;

  /* Check parameters */
  if ((i_target < 0) || (i_src < 0) ||
      (i_target >= INSTR_MAXCOUNT) || (i_src >= INSTR_MAXCOUNT)) {
    abort();
  }
  
  /* Only proceed if source and target are different */
  if (i_target != i_src) {
    
    /* Get pointers to source and target */
    ps = instr_ptr(i_src);
    pt = instr_ptr(i_target);

    /* Check if source is cleared */
    if (instr_isclear(ps)) {
      /* Source is cleared, so just clear target */
      instr_clear(i_target);
    
    } else {
      /* Source not cleared, so clear target and copy to it */
      instr_clear(i_target);
      memcpy(pt, ps, sizeof(INSTR_REG));
      
      /* Add any references */
      if (pt->itype == ITYPE_SQUARE) {
        /* Add references for square wave instrument */
        adsr_addref((pt->val).pa);      
      
      } else if (pt->itype == ITYPE_FM) {
        /* Add references for FM instrument */
        generator_addref((pt->val).fmp.pRoot);
      
      } else {
        /* Shouldn't happen */
        abort();
      }
    }
  }
}

/*
 * instr_setMaxMin function.
 */
void instr_setMaxMin(int32_t i, int32_t i_max, int32_t i_min) {
  
  INSTR_REG *pr = NULL;
  
  /* Get pointer to instrument register */
  pr = instr_ptr(i);
  
  /* Check parameters */
  if ((i_max < 0) || (i_min < 0) ||
      (i_max > MAX_FRAC) || (i_min > MAX_FRAC) ||
      (i_max < i_min)) {
    abort();
  }
  
  /* Check whether both intensities are zero */
  if ((i_max == 0) && (i_min == 0)) {
    /* Both intensities zero, so just clear register */
    instr_clear(i);
  
  } else {
    /* At least one intensity non-zero -- proceed only if register is
     * not clear */
    if (!instr_isclear(pr)) {
      /* Set the new intensity values */
      pr->i_max = (uint16_t) i_max;
      pr->i_min = (uint16_t) i_min;
    }
  }
  
}

/*
 * instr_setStereo function.
 */
void instr_setStereo(int32_t i, const STEREO_POS *psp) {
  
  INSTR_REG *pr = NULL;
  
  /* Get pointer to instrument register */
  pr = instr_ptr(i);
  
  /* Check parameter */
  if (psp == NULL) {
    abort();
  }
  
  /* Only proceed if register not cleared */
  if (!instr_isclear(pr)) {
    /* Copy in the new stereo position */
    memcpy(&(pr->sp), psp, sizeof(STEREO_POS));
  }
}

/*
 * instr_prepare function.
 */
void *instr_prepare(int32_t i, int32_t dur, int32_t pitch) {
  
  INSTR_REG *pr = NULL;
  GENERATOR_OPDATA *pod = NULL;
  int32_t x = 0;
  int32_t icount = 0;
  double f = 0.0;
  
  /* Get pointer to instrument register */
  pr = instr_ptr(i);
  
  /* Check parameters */
  if ((dur < 1) || (pitch < PITCH_MIN) || (pitch > PITCH_MAX)) {
    abort();
  }
  
  /* Only proceed if register not cleared */
  if (!instr_isclear(pr)) {
    /* Only proceed if FM instrument */
    if (pr->itype == ITYPE_FM) {
      
      /* Look up the frequency for this pitch */
      f = pitchfreq(pitch);
      
      /* Get the count of instance data structures */
      icount = (pr->val).fmp.icount;
      
      /* Allocate the necessary set of generator instance data */
      pod = (GENERATOR_OPDATA *) calloc(
                                    (size_t) icount,
                                    sizeof(GENERATOR_OPDATA));
      if (pod == NULL) {
        abort();
      }
      
      /* Initialize all the instance data */
      for(x = 0; x < icount; x++) {
        generator_opdata_init(&(pod[x]), f, dur);
      }
    }
  }
  
  /* Return instance data or NULL */
  return pod;
}

/*
 * instr_length function.
 */
int32_t instr_length(int32_t i, int32_t dur, void *pod) {
  
  INSTR_REG *pr = NULL;
  int32_t result = 0;
  
  /* Get pointer to instrument register */
  pr = instr_ptr(i);
  
  /* Check parameter */
  if (dur < 1) {
    abort();
  }
  
  /* Check if register cleared */
  if (instr_isclear(pr)) {
    /* Register is cleared, so result is always one */
    result = 1;

  } else {
    /* Register is not cleared, so call through */
    if (pr->itype == ITYPE_SQUARE) {
      /* For square waves, call through to ADSR envelope, after making
       * sure no instance data was provided */
      if (pod != NULL) {
        abort();
      }
      result = adsr_length((pr->val).pa, dur);
      
    } else if (pr->itype == ITYPE_FM) {
      /* For FM instruments, query generator map, after making sure
       * instance data was provided */
      if (pod == NULL) {
        abort();
      }
      result = generator_length(
                  (pr->val).fmp.pRoot,
                  pod,
                  (pr->val).fmp.icount);
    
    } else {
      /* Shouldn't happen */
      abort();
    }
  }
  
  /* Return result */
  return result;
}

/*
 * instr_get function.
 */
void instr_get(
    int32_t       i,
    int32_t       t,
    int32_t       dur,
    int32_t       pitch,
    int16_t       amp,
    STEREO_SAMP * pss,
    void        * pod) {
  
  INSTR_REG *pr = NULL;
  double sf = 0.0;
  double af = 0.0;
  int16_t s = 0;
  int32_t s32 = 0;
  int32_t intensity = 0;

  /* Get pointer to instrument register */
  pr = instr_ptr(i);
  
  /* Check parameters */
  if (t < 0) {
    abort();
  }
  if (dur < 1) {
    abort();
  }
  if ((pitch < PITCH_MIN) || (pitch > PITCH_MAX)) {
    abort();
  }
  if ((amp < 0) || (amp > MAX_FRAC)) {
    abort();
  }
  if (pss == NULL) {
    abort();
  }
  
  /* Only proceed if instrument register is not clear; otherwise, just
   * generate a zero result */
  if (!instr_isclear(pr)) {
  
    /* Handle instrument types */
    if (pr->itype == ITYPE_SQUARE) {
      /* Square wave instrument, verify that no instance data */
      if (pod != NULL) {
        abort();
      }
  
      /* First of all, get the sample from the square wave generator */
      s = sqwave_get(pitch, t);
    
      /* Second, compute the intensity from the amplitude and the i_max
       * & i_min parameters */
      intensity =
        ((((int32_t) amp) * ((int32_t) (pr->i_max - pr->i_min))) /
                  ((int32_t) MAX_FRAC)) + ((int32_t) pr->i_min);
    
      /* Next, multiply the sample by the intensity */
      s = (int16_t) ((intensity * ((int32_t) s)) / 
              ((int32_t) MAX_FRAC));
    
      /* Then comes the envelope */
      s = adsr_mul((pr->val).pa, t, dur, s);
    
      /* Finally, stereo-image the sample */
      stereo_image(s, pitch, &(pr->sp), pss);
    
    } else if (pr->itype == ITYPE_FM) {
      /* FM instrument, verify that instance data */
      if (pod == NULL) {
        abort();
      }
    
      /* First of all, get the generated floating-point sample */
      sf = generator_invoke(
                (pr->val).fmp.pRoot,
                pod,
                (pr->val).fmp.icount,
                t);
      
      /* Second, compute floating-point intensity from the amplitude and
       * the i_max & i_min parameters */
      af = 
        ((((double) amp) * ((double) (pr->i_max - pr->i_min))) /
                  ((double) MAX_FRAC)) + ((double) pr->i_min);

      /* Next, multiply the floating-point sample by the intensity */
      sf = (sf * af) / ((double) MAX_FRAC);
      
      /* If floating-point sample not finite, set to zero */
      if (!isfinite(sf)) {
        sf = 0.0;
      }
      
      /* Clamp floating-point sample to 16-bit signed range */
      if (sf > ((double) INT16_MAX)) {
        sf = (double) INT16_MAX;
      } else if (sf < (double) INT16_MIN) {
        sf = (double) INT16_MIN;
      }
      
      /* Convert to 32-bit integer and clamp to 16-bit range */
      s32 = (int32_t) floor(sf);
      if (s32 > INT16_MAX) {
        s32 = INT16_MAX;
      } else if (s32 < INT16_MIN) {
        s32 = INT16_MIN;
      }
      
      /* Finally, stereo-image the sample */
      stereo_image((int16_t) s32, pitch, &(pr->sp), pss);
    
    } else {
      /* Shouldn't happen */
      abort();
    }
  
  } else {
    /* Instrument register clear */
    pss->left = 0;
    pss->right = 0;
  }
}

/*
 * instr_errstr function.
 */
const char *instr_errstr(int code) {
  
  const char *pResult = NULL;
  
  switch (code) {
    case INSTR_ERR_OK:
      pResult = "No error";
      break;
    
    case INSTR_ERR_NOTFOUND:
      pResult = "Can't find external instrument file";
      break;
    
    case INSTR_ERR_BADCALL:
      pResult = "Invalid instrument call number";
      break;
    
    case INSTR_ERR_HUGEPATH:
      pResult = "External instrument path too long";
      break;
    
    case INSTR_ERR_OPEN:
      pResult = "Can't open instrument file";
      break;
    
    default:
      pResult = "Unknown error";
  }
  
  return pResult;
}
