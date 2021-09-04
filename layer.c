/*
 * layer.c
 * 
 * Implementation of layer.h
 * 
 * See the header for further information.
 */

#include "layer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Type declarations
 * =================
 */

/*
 * The layer register structure.
 */
typedef struct {
  
  /*
   * Pointer to the graph object, or NULL if this layer is undefined.
   */
  GRAPH_OBJ *pg;
  
  /*
   * The layer multiplier.
   * 
   * Only valid if pg is non-NULL.
   * 
   * This must be in range [0, MAX_FRAC].
   */
  int16_t m;
  
} LAYER_REG;

/*
 * Static data
 * ===========
 */

/*
 * The initialization flag.
 * 
 * Set to non-zero when the layer registers have been initialized.
 */
static int m_layer_init = 0;

/*
 * The layer register bank.
 * 
 * Only valid when m_layer_init is non-zero.
 */
static LAYER_REG m_layer_t[LAYER_MAXCOUNT];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void layer_init(void);
static LAYER_REG *layer_ptr(int32_t i);
static int16_t layer_qmul(double m);

/*
 * Initialize the layer register bank, if not already initialized.
 */
static void layer_init(void) {
  
  int32_t x = 0;
  
  if (!m_layer_init) {
    m_layer_init = 1;
    memset(m_layer_t, 0, sizeof(LAYER_REG) * LAYER_MAXCOUNT);
    for(x = 0; x < LAYER_MAXCOUNT; x++) {
      (m_layer_t[x]).pg = NULL;
    }
  }
}

/*
 * Get a pointer to the given layer register.
 * 
 * Initializes the layer register bank if necessary.
 * 
 * Parameters:
 * 
 *   i - the layer register
 * 
 * Return:
 * 
 *   a pointer to the layer register
 */
static LAYER_REG *layer_ptr(int32_t i) {
  
  /* Check parameter */
  if ((i < 0) || (i >= LAYER_MAXCOUNT)) {
    abort();
  }
  
  /* Initialize if necessary */
  layer_init();
  
  /* Return pointer */
  return &(m_layer_t[i]);
}

/*
 * Quantize the floating-point multiplier to an integer.
 * 
 * The multiplier must be greater than zero.  The result will be at
 * least one.
 * 
 * Parameters:
 * 
 *   m - the floating-point multiplier
 * 
 * Return:
 * 
 *   the integer multiplier
 */
static int16_t layer_qmul(double m) {
  
  int32_t result = 0;
  
  /* Check parameter */
  if (!isfinite(m)) {
    abort();
  }
  if (!((m > 0.0) && (m <= 1.0))) {
    abort();
  }
  
  /* Quantize */
  result = (int32_t) (m * ((double) MAX_FRAC));
  if (result < 1) {
    result = 1;
  } else if (result > MAX_FRAC) {
    result = MAX_FRAC;
  }
  
  /* Return result */
  return (int16_t) result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * layer_clear function.
 */
void layer_clear(int32_t layer) {
  
  LAYER_REG *pr = NULL;
  
  /* Get pointer to register */
  pr = layer_ptr(layer);
  
  /* Only proceed if register defined */
  if (pr->pg != NULL) {
    graph_release(pr->pg);
    memset(pr, 0, sizeof(LAYER_REG));
    pr->pg = NULL;
  }
}

/*
 * layer_define function.
 */
void layer_define(int32_t layer, double mul, GRAPH_OBJ *pg) {
  
  LAYER_REG *pr = NULL;
  
  /* Check parameters */
  if (!isfinite(mul)) {
    abort();
  }
  if (!((mul >= 0.0) && (mul <= 1.0))) {
    abort();
  }
  if (pg == NULL) {
    abort();
  }
  
  /* Clear register */
  layer_clear(layer);
  
  /* Only proceed if multiplier greater than zero */
  if (mul > 0.0) {
  
    /* Get pointer to register */
    pr = layer_ptr(layer);
    
    /* Copy in parameters */
    pr->pg = pg;
    graph_addref(pg);
    pr->m = layer_qmul(mul);
  }
}

/*
 * layer_derive function.
 */
void layer_derive(int32_t target, int32_t source, double mul) {
  
  /* Check parameters */
  if ((target < 0) || (source < 0) ||
      (target > LAYER_MAXCOUNT - 1) || (source > LAYER_MAXCOUNT - 1)) {
    abort();
  }
  if (!isfinite(mul)) {
    abort();
  }
  if (!((mul >= 0.0) && (mul <= 1.0))) {
    abort();
  }
  
  /* Initialize registers if necessary */
  layer_init();
  
  /* First, filter out special case of mul zero */
  if (mul > 0.0) {
    /* Non-zero multiplier, next see if source register is clear */
    if ((m_layer_t[source]).pg == NULL) {
      
      /* Source register clear, so just clear the target register */
      layer_clear(target);
      
    } else {
      /* Source register not clear, now check if source and target are
       * the same */
      if (source == target) {
        /* Registers are the same, so just change the multiplier */
        (m_layer_t[target]).m = layer_qmul(mul);
        
      } else {
        /* Registers are not the same, so copy in the graph and adjust
         * the multiplier */
        layer_clear(target);
        (m_layer_t[target]).m = layer_qmul(mul);
        (m_layer_t[target]).pg = (m_layer_t[source]).pg;
        graph_addref((m_layer_t[target]).pg);
      }
    }
    
  } else {
    /* Zero multiplier, so just clear target register */
    layer_clear(target);
  }
}

/*
 * layer_get function.
 */
int16_t layer_get(int32_t layer, int32_t t) {
  
  LAYER_REG *pr = NULL;
  int32_t result = 0;
  
  /* Check t parameter */
  if (t < 0) {
    abort();
  }
  
  /* Get pointer to register */
  pr = layer_ptr(layer);
  
  /* Check if register is clear */
  if (pr->pg == NULL) {
    
    /* Register is clear, so result is just zero */
    result = 0;
    
  } else {
    /* Register not clear, so first of all get the graph value */
    result = (int32_t) graph_get(pr->pg, t);
    
    /* Next, multiply by layer scaling rate */
    result = (result * ((int32_t) pr->m)) / MAX_FRAC;
    
    /* Clamp result */
    if (result < 0) {
      result = 0;
    } else if (result > MAX_FRAC) {
      result = MAX_FRAC;
    }
  }
  
  /* Return result */
  return (int16_t) result;
}
