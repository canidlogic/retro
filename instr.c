/*
 * instr.c
 * 
 * Implementation of instr.h
 * 
 * See the header for further information.
 */

#include "instr.h"
#include <stdlib.h>
#include <string.h>

/*
 * Type declarations
 * =================
 */

/*
 * The instrument register structure.
 */
typedef struct {
  
  /*
   * The maximum and minimum intensities for this instrument.
   * 
   * If both are zero, the register is cleared.
   * 
   * Both must be in range [0, INSTR_MAXINTENSITY].  Furthermore, i_max
   * must be greater than or equal to i_min.
   */
  uint16_t i_max;
  uint16_t i_min;
  
  /*
   * Pointer to the envelope object.
   * 
   * If the register is cleared, this must be NULL.  Otherwise, this
   * must be non-NULL.
   * 
   * The register must have a reference to the envelope object.
   */
  ADSR_OBJ *pa;
  
  /*
   * The stereo position of this instrument.
   * 
   * This is only valid if the register is not cleared.
   */
  STEREO_POS sp;
  
} INSTR_REG;

/*
 * Static data
 * ===========
 */

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
static void instr_t_init(void);
static INSTR_REG *instr_ptr(int32_t i);
static int instr_isclear(const INSTR_REG *pr);

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
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * instr_clear function.
 */
void instr_clear(int32_t i) {
  
  INSTR_REG *pr = NULL;
  
  /* Get instrument register */
  pr = instr_ptr(i);
  
  /* Only proceed if instrument register not clear */
  if (!instr_isclear(pr)) {
    /* Release the ADSR object */
    adsr_release(pr->pa);
    pr->pa = NULL;
    
    /* Clear the record */
    memset(pr, 0, sizeof(INSTR_REG));
    
    /* Set the clear */
    pr->i_max = 0;
    pr->i_min = 0;
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
      (i_max > INSTR_MAXINTENSITY) || (i_min > INSTR_MAXINTENSITY) ||
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
    pr->pa = pa;
    adsr_addref(pa);
    memcpy(&(pr->sp), psp, sizeof(STEREO_POS));
  }
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
      adsr_addref(pt->pa);
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
      (i_max > INSTR_MAXINTENSITY) || (i_min > INSTR_MAXINTENSITY) ||
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
 * instr_setADSR function.
 */
void instr_setADSR(int32_t i, ADSR_OBJ *pa) {
  
  INSTR_REG *pr = NULL;
  
  /* Get pointer to instrument register */
  pr = instr_ptr(i);
  
  /* Check parameter */
  if (pa == NULL) {
    abort();
  }
  
  /* Only proceed if register not cleared */
  if (!instr_isclear(pr)) {
    /* Release the current envelope */
    adsr_release(pr->pa);
    pr->pa = NULL;
    
    /* Copy in the new envelope */
    pr->pa = pa;
    adsr_addref(pa);
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
 * instr_length function.
 */
int32_t instr_length(int32_t i, int32_t dur) {
  
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
    /* Register is not cleared, so call through to envelope */
    result = adsr_length(pr->pa, dur);
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
    STEREO_SAMP * pss) {
  /* @@TODO: */
}
