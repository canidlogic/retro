/*
 * graph.c
 * 
 * Implementation of graph.h
 * 
 * See the header for further information.
 */

#include "graph.h"
#include <stdlib.h>
#include <string.h>

/*
 * Type declarations
 * =================
 */

/*
 * An element within the graph.
 */
typedef struct {
  
  /*
   * The time offset of this node.
   * 
   * If this is -1, then the element is currently undefined.
   * 
   * Otherwise, the range is zero or greater.
   */
  int32_t t;
  
  /*
   * The starting intensity of this node.
   * 
   * Only valid if the element is defined.
   * 
   * The range is [0, MAX_FRAC].
   */
  int16_t ra;
  
  /*
   * The ending intensity of this node, or -1 for a constant node.
   * 
   * Only valid if the element is defined.
   * 
   * The range is [0, MAX_FRAC], or -1.
   */
  int16_t rb;
  
} GRAPH_NODE;

/*
 * GRAPH_OBJ structure.
 * 
 * Prototype given in header.
 */
struct GRAPH_OBJ_TAG {
  
  /*
   * The reference count of this object.
   */
  int32_t refcount;
  
  /*
   * The number of elements in this graph.
   * 
   * Must be at least one.
   */
  int32_t ecount;
  
  /*
   * The array of graph nodes.
   * 
   * This extends beyond the end of the structure as necessary.
   */
  GRAPH_NODE n[1];
};

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * graph_alloc function.
 */
GRAPH_OBJ *graph_alloc(int32_t count) {
  
  GRAPH_OBJ *pg = NULL;
  GRAPH_NODE *pe = NULL;
  int32_t x = 0;
  
  /* Check parameter */
  if ((count < 1) || (count > GRAPH_MAXCOUNT)) {
    abort();
  }
  
  /* Allocate graph, with room for extra elements */
  pg = (GRAPH_OBJ *) malloc(sizeof(GRAPH_OBJ) +
                            ((count - 1) * sizeof(GRAPH_NODE)));
  if (pg == NULL) {
    abort();
  }
  memset(pg, 0, sizeof(GRAPH_OBJ) + ((count - 1) * sizeof(GRAPH_NODE)));
  
  /* Initialize the structure */
  pg->refcount = 1;
  pg->ecount = count;
  for(x = 0; x < count; x++) {
    pe = &((pg->n)[x]);
    pe->t = -1;
    pe->ra = -1;
    pe->rb = -1;
  }
  
  /* Return the object */
  return pg;
}

/*
 * graph_addref function.
 */
void graph_addref(GRAPH_OBJ *pg) {
  
  /* Check parameter */
  if (pg == NULL) {
    abort();
  }
  
  /* Increment reference count, watching for overflow */
  if (pg->refcount < INT32_MAX) {
    (pg->refcount)++;
  } else {
    abort();
  }
}

/*
 * graph_release function.
 */
void graph_release(GRAPH_OBJ *pg) {
  
  /* Only proceed if parameter is not NULL */
  if (pg != NULL) {
    /* Decrement reference count */
    (pg->refcount)--;
    
    /* If reference count is now zero, free the object */
    if (pg->refcount < 1) {
      free(pg);
    }
  }
}

/*
 * graph_set function.
 */
void graph_set(
    GRAPH_OBJ * pg,
    int32_t     i,
    int32_t     t,
    int32_t     ra,
    int32_t     rb) {
  
  GRAPH_NODE *pe = NULL;
  
  /* Check parameters */
  if (pg == NULL) {
    abort();
  }
  if ((i < 0) || (i >= pg->ecount)) {
    abort();
  }
  if (t < 0) {
    abort();
  }
  if ((ra < 0) || (ra > MAX_FRAC)) {
    abort();
  }
  if ((rb < -1) || (rb > MAX_FRAC)) {
    abort();
  }
  
  /* If ra and rb are equal, change rb to -1 because then the "ramp" is
   * actually constant */
  if (ra == rb) {
    rb = -1;
  }
  
  /* The element indicated must currently be undefined */
  if (((pg->n)[i]).t >= 0) {
    abort();  /* element already defined */
  }
  
  /* If this is not the first element, the previous element must be
   * defined and its t value must be less than this t */
  if (i > 0) {
    if (((pg->n)[i - 1]).t < 0) {
      abort();  /* previous element not defined */
    }
    if (((pg->n)[i - 1]).t >= t) {
      abort();  /* previous element t value not less than this t */
    }
  }
  
  /* If this is the first element, t must be zero */
  if ((i == 0) && (t != 0)) {
    abort();
  }
  
  /* If this is the last element, it must be constant */
  if ((i >= pg->ecount - 1) && (rb >= 0)) {
    abort();
  }
  
  /* Everything checks out, so set the element */
  pe = &((pg->n)[i]);
  pe->t = t;
  pe->ra = (int16_t) ra;
  pe->rb = (int16_t) rb;
}

/*
 * graph_get function.
 */
int16_t graph_get(GRAPH_OBJ *pg, int32_t t) {
  
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int32_t midt = 0;
  int32_t e_len = 0;
  int32_t result = 0;
  int32_t offset = 0;
  GRAPH_NODE *pe = NULL;
  
  /* Check parameters */
  if ((pg == NULL) || (t < 0)) {
    abort();
  }
  
  /* Make sure all elements are defined */
  if (((pg->n)[pg->ecount - 1]).t < 0) {
    abort();
  }
  
  /* We're looking for the element with the greatest t value that is
   * less than or equal to t */
  lo = 0;
  hi = pg->ecount - 1;
  while (lo < hi) {
    
    /* Figure out a midpoint that is greater than lo */
    mid = lo + ((hi - lo) / 2);
    if (mid <= lo) {
      mid = lo + 1;
    }
    
    /* Get midpoint t value */
    midt = ((pg->n)[mid]).t;
    
    /* Compare t to midpoint t */
    if (t > midt) {
      /* t greater than midpoint, so set low bound to midpoint */
      lo = mid;
      
    } else if (t < midt) {
      /* t less than midpoint, so set high bound to one less than
       * midpoint */
      hi = mid - 1;
      
    } else if (t == midt) {
      /* t equals midpoint, so zoom in on it */
      lo = mid;
      hi = mid;
      
    } else {
      /* Shouldn't happen */
      abort();
    }
  }
  
  /* Get a pointer to the element t is within */
  pe = &((pg->n)[lo]);
  
  /* If this element is a ramp, determine its length (there is a rule
   * that the last element may not be a ramp, so we know there is a next
   * element if there is a ramp) */
  if (pe->rb >= 0) {
    e_len = ((pg->n)[lo + 1]).t - pe->t;
  }
  
  /* Compute the result */
  if (pe->rb >= 0) {
    /* We have a ramp, so determine offset within it */
    offset = t - pe->t;
    
    /* Interpolate result */
    result = (int32_t) (((((int64_t) offset) *
                          (((int64_t) pe->rb) - ((int64_t) pe->ra))) /
                          ((int64_t) e_len)) +
                            ((int64_t) pe->ra));
    
    /* Clamp result */
    if (result < 0) {
      result = 0;
    } else if (result > MAX_FRAC) {
      result = MAX_FRAC;
    }
    
  } else {
    /* We have a constant element, so just return the element value */
    result = pe->ra;
  }
  
  /* Return result */
  return (int16_t) result;
}
