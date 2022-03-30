/*
 * graph.c
 * =======
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
 * A block element within the graph.
 */
typedef struct {
  
  /*
   * The width of this block and mode of the block.
   * 
   * If this is zero, this block is the special termination node.
   * 
   * If this is greater than zero, this is a smooth block and this field
   * is the width of the block.
   * 
   * If this is less than zero, this is a rough block and the absolute
   * value of this field is the width of the block.
   */
  int32_t w;
  
  /*
   * The graph value immediately before the start of this block.
   * 
   * Ignored except for rough blocks.
   * 
   * Valid range is [-32767, 32767].
   */
  int16_t a;
  
  /*
   * The graph value of the last position in this block.
   * 
   * Present except for the termination node.
   * 
   * Valid range is [-32767, 32767].
   */
  int16_t b;
  
} GRAPH_BLOCK;

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
   * The capacity of this graph.
   * 
   * Must be at least one.
   */
  int32_t cap;
  
  /*
   * The number of blocks that have been pushed onto this graph.
   * 
   * Must be zero or greater, and less than cap.
   */
  int32_t count;
  
  /*
   * The array of graph blocks.
   * 
   * This extends beyond the end of the structure as necessary.
   */
  GRAPH_BLOCK n[1];
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
  int32_t sz = 0;
  
  /* Check parameter */
  if (count < 1) {
    abort();
  }
  
  /* Compute size of the graph object, watching for overflow */
  sz = count - 1;
  
  if (sz <= INT32_MAX / sizeof(GRAPH_BLOCK)) {
    sz *= (int32_t) sizeof(GRAPH_BLOCK);
  } else {
    abort();
  }
  
  if (sz <= INT32_MAX - sizeof(GRAPH_OBJ)) {
    sz += (int32_t) sizeof(GRAPH_OBJ);
  } else {
    abort();
  }
  
  /* Allocate graph, with room for extra elements */
  pg = (GRAPH_OBJ *) malloc((size_t) sz);
  if (pg == NULL) {
    abort();
  }
  memset(pg, 0, (size_t) sz);
  
  /* Initialize the structure */
  pg->refcount = 1;
  pg->cap = count;
  pg->count = 0;
  
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
 * graph_pushSmooth function.
 */
void graph_pushSmooth(GRAPH_OBJ *pg, int32_t w, int32_t b) {
  
  /* Check parameters */
  if ((pg == NULL) || (w < 1) || (b < -32767) || (b > 32767)) {
    abort();
  }
  
  /* Check that we have room in graph */
  if (pg->count >= pg->cap - 1) {
    abort();
  }
  
  /* Store the smooth block */
  ((pg->n)[pg->count]).w = w;
  ((pg->n)[pg->count]).b = (int16_t) b;
  
  (pg->count)++;
}

/*
 * graph_pushRough function.
 */
void graph_pushRough(GRAPH_OBJ *pg, int32_t w, int32_t a, int32_t b) {
  
  /* Check parameters */
  if ((pg == NULL) || (w < 1) ||
      (a < -32767) || (a > 32767) || (b < -32767) || (b > 32767)) {
    abort();
  }
  
  /* Check that we have room in graph */
  if (pg->count >= pg->cap - 1) {
    abort();
  }
  
  /* Store the rough block */
  ((pg->n)[pg->count]).w = 0 - w;
  ((pg->n)[pg->count]).a = (int16_t) a;
  ((pg->n)[pg->count]).b = (int16_t) b;
  
  (pg->count)++;
}

/*
 * graph_get function.
 */
int16_t graph_get(GRAPH_OBJ *pg, int32_t t) {
  
  const GRAPH_BLOCK *pe = NULL;
  int32_t i = 0;
  int32_t a = 0;
  int32_t b = 0;
  int32_t w = 0;
  int64_t result = 0;
  
  /* Check parameters */
  if ((pg == NULL) || (t < 0)) {
    abort();
  }
  
  /* Iterate through graph and find the block that this t value belongs
   * to, or stop on the terminator block if t is beyond end of graph */
  b = 0;
  for(i = 0; i <= pg->count; i++) {
    /* Get current block */
    pe = &((pg->n)[i]);

    /* Check width/mode of current block */
    if (pe->w > 0) {
      /* Smooth block, get width of block */
      w = pe->w;
      
    } else if (pe->w < 0) {
      /* Rough block, get width of block */
      w = 0 - pe->w;
      
    } else if (pe->w == 0) {
      /* Terminator block, so stop */
      break;
      
    } else {
      /* Shouldn't happen */
      abort();
    }
    
    /* If we are inside current block, stop */
    if ((t >= b) && (t - b < w)) {
      break;
    }
    
    /* Update base; in case of overflow, set to maximum int value */
    if (b <= INT32_MAX - w) {
      b += w;
    } else {
      b = INT32_MAX;
    }
  }
  
  /* Get the block */
  pe = &((pg->n)[i]);
  
  /* Compute based on kind of block */
  if (pe->w == 0) {
    /* Terminator block -- check if first block */
    if (i < 1) {
      /* First block, so result is zero */
      a = 0;
      b = 0;
      i = 0;
      w = 1;
      
    } else {
      /* Not first block, so result is last sample in previous block */
      a = ((pg->n)[i - 1]).b;
      b = a;
      i = 0;
      w = 1;
    }
    
  } else if (pe->w < 0) {
    /* Inside a rough block, so get parameters from block */
    i = t - b;
    a = pe->a;
    b = pe->b;
    w = 0 - pe->w;
  
  } else if (pe->w > 0) {
    /* Inside a smooth block, starting parameter is from previous block
     * if exists, else zero */
    if (i < 1) {
      /* First block, so zero */
      a = 0;
    } else {
      /* Not first block, so from last of previous */
      a = ((pg->n)[i - 1]).b;
    }
    
    /* Set other parameters */
    i = t - b;
    b = pe->b;
    w = pe->w;
    
  } else {
    /* Shouldn't happen */
    abort();
  }

  /* Linearly interpolate result */
  result = ((int64_t) a)
            + (((((int64_t) b) - ((int64_t) a))
                * (((int64_t) i) + 1)))
              / ((int64_t) w);
  
  /* Clamp to range */
  if (result < -32767) {
    result = -32767;
  } else if (result > 32767) {
    result = 32767;
  }
  
  /* Return casted result */
  return (int16_t) result;
}
