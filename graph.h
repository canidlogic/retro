#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

/*
 * graph.h
 * 
 * The graph module of the Retro synthesizer.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum intensity value in a graph.
 */
#define GRAPH_MAXVAL (UINT16_C(1024))

/* @@TODO: */
struct GRAPH_OBJ_TAG;
typedef struct GRAPH_OBJ_TAG GRAPH_OBJ;

GRAPH_OBJ *graph_alloc(int32_t count);
void graph_addref(GRAPH_OBJ *pg);
void graph_release(GRAPH_OBJ *pg);
void graph_set(
  GRAPH_OBJ *pg, int32_t i, int32_t t, int32_t ra, int32_t rb);

#endif
