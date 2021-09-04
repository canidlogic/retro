#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

/*
 * graph.h
 * 
 * The graph module of the Retro synthesizer.
 */

#include "retrodef.h"

/*
 * The maximum number of elements in a graph.
 * 
 * (This is also limited by the maximum stack height in the main retro
 * module, since all elements must be defined on the stack in the input
 * file.)
 */
#define GRAPH_MAXCOUNT (4096)

/* 
 * GRAPH_OBJ structure prototype.
 * 
 * Definition given in the implementation.
 */
struct GRAPH_OBJ_TAG;
typedef struct GRAPH_OBJ_TAG GRAPH_OBJ;

/*
 * Create a new graph object with the given number of elements.
 * 
 * count must be in range [1, GRAPH_MAXCOUNT].
 * 
 * The returned graph object has a reference count of one.  All
 * references should eventually be released with graph_release().
 * 
 * After allocation, each element in the graph is "undefined."  Each
 * element must be defined with graph_set() before the graph can be
 * used.
 * 
 * Parameters:
 * 
 *   count - the number of elements in the graph
 * 
 * Return:
 * 
 *   a new graph object
 */
GRAPH_OBJ *graph_alloc(int32_t count);

/*
 * Increment the reference count of the given graph object.
 * 
 * A fault occurs if the reference count overflows.  This only happens
 * if there are billions of references.
 * 
 * Parameters:
 * 
 *   pg - the graph object to add a reference to
 */
void graph_addref(GRAPH_OBJ *pg);

/*
 * Decrement the reference count of the given graph object.
 * 
 * If NULL is passed, the call is ignored.
 * 
 * If this call causes the reference count to drop to zero, the graph
 * object is freed.
 * 
 * Parameters:
 * 
 *   pg - the graph object to release, or NULL
 */
void graph_release(GRAPH_OBJ *pg);

/*
 * Set an element in the given graph object.
 * 
 * When a graph object is first defined, each element within it is in an
 * undefined state.  Before the graph object can be used, each element
 * must be defined using this function.
 * 
 * This function must be called in sequential order, beginning with the
 * first element of the graph and ending with the last element of the
 * graph.  Elements may only be set once.
 * 
 * pg is the graph object and i is the zero-based index of the element.
 * 
 * t is the time offset for the element.  The time offset of the first
 * element must be zero.  The time offset of any element elements after
 * the first must be greater than that of the previous element.
 * 
 * ra is the intensity value of constant graph elements, and the
 * starting intensity of ramp graph elements.  rb is -1 for constant
 * graph elements, or the ending intensity of ramp graph elements.
 * 
 * Both ra and rb must be in range [0, MAX_FRAC] (except rb may be -1).
 * If ra is equal to rb, then the call is equivalent to having rb equal
 * to -1.
 * 
 * The last element must be a constant element.  It may not be a ramp.
 * 
 * Parameters:
 * 
 *   pg - the graph object
 * 
 *   i - the index of the element to set
 * 
 *   t - the time offset of the element
 * 
 *   ra - the starting intensity of the element
 * 
 *   rb - the ending intensity of the element, or -1
 */
void graph_set(
    GRAPH_OBJ * pg,
    int32_t     i,
    int32_t     t,
    int32_t     ra,
    int32_t     rb);

/*
 * Get the graph value at a given t offset.
 * 
 * pg is the graph object.  All elements must have been defined already
 * using graph_set().
 * 
 * t is the time offset.  It must be zero or greater.
 * 
 * The return value is the computed intensity at the time t.  It will be
 * in range [0, MAX_FRAC].
 * 
 * Parameters:
 * 
 *   pg - the graph object
 * 
 *   t - the time offset
 * 
 * Return:
 * 
 *   the computed intensity according to the graph
 */
int16_t graph_get(GRAPH_OBJ *pg, int32_t t);

#endif
