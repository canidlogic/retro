/*
 * instr.c
 * 
 * Implementation of instr.h
 * 
 * See the header for further information.
 */

#include "instr.h"

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
 * Type declarations
 * =================
 */

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
          long    * pline) {
  /* @@TODO: */
  fprintf(stderr, "TODO: embedded %s\n", pText);
  *per = SNERR_BADCR;
  *pline = 1;
  return 0;
}

/*
 * instr_external function.
 */
int instr_external(
          int32_t   i,
    const char    * pCall,
          int     * per,
          long    * pline) {
  /* @@TODO: */
  fprintf(stderr, "TODO: external %s\n", pCall);
  *per = SNERR_BADCR;
  *pline = 1;
  return 0;
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
