#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

/*
 * graph.h
 * =======
 * 
 * The graph module of the Retro synthesizer.
 */

#include <stddef.h>
#include <stdint.h>

/* 
 * GRAPH_OBJ structure prototype.
 * 
 * Definition given in the implementation.
 */
struct GRAPH_OBJ_TAG;
typedef struct GRAPH_OBJ_TAG GRAPH_OBJ;

/*
 * Create a new graph object with the given capacity.
 * 
 * count must be greater than zero.  In addition to any blocks that will
 * be added, there must also be one additional element to serve as the
 * terminator block.  Therefore, an empty graph will require a count of
 * one, a graph with one block requires a count of two, etc.
 * 
 * Although this module can work with large count values, note that the
 * algorithms and data structures are optimized for relatively small
 * graphs (measured in block counts) and become inefficient for large
 * counts of blocks.
 * 
 * The returned graph object has a reference count of one.  All
 * references should eventually be released with graph_release().
 * 
 * After allocation, the graph will be an empty graph that returns zero
 * for all times.
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
 * Push a smooth block to the end of the given graph object.
 * 
 * The graph must have sufficient capacity to add the new block or a
 * fault occurs.
 * 
 * w is the width of the block in control units.  It must be greater
 * than zero.
 * 
 * b is the output value at the last sample of the block.  It must be
 * in range [-32767, 32767].  A smooth block connects smoothly with the
 * value at the end of the previous block, or with a value of zero if
 * this is the first block.
 * 
 * Parameters:
 * 
 *   pg - the graph object
 * 
 *   w - the width of the new block
 * 
 *   b - the last value in the new block
 */
void graph_pushSmooth(GRAPH_OBJ *pg, int32_t w, int32_t b);

/*
 * Push a rough block to the end of the given graph object.
 * 
 * The graph must have sufficient capacity to add the new block or a
 * fault occurs.
 * 
 * w is the width of the block in control units.  It must be greater
 * than zero.
 * 
 * a is the output value immediately before the start of the block,
 * while b is the output value at the last sample of the block.  Both
 * must be in range [-32767, 32767].  Rough blocks don't have any
 * implicit connection to the previous block, so a discontinuity may
 * occur at the start of the rough block.
 * 
 * Parameters:
 * 
 *   pg - the graph object
 * 
 *   w - the width of the new block
 * 
 *   a - the value immediately before the start of the new block
 * 
 *   b - the last value in the new block
 */
void graph_pushRough(GRAPH_OBJ *pg, int32_t w, int32_t a, int32_t b);

/*
 * Get the graph value at a given t offset.
 * 
 * t is the time offset.  It must be zero or greater.  If it goes beyond
 * the end of the last defined block in the graph, the last output value
 * of the last block is repeated.  If there are no defined blocks in the
 * graph, zero is returned.
 * 
 * Time units for graphs are control units.
 * 
 * The return value is the computed intensity at the time t.  It will be
 * in range [-32767, 32767].
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
