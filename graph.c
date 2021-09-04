/*
 * graph.c
 * 
 * Implementation of graph.h
 * 
 * See the header for further information.
 */

/* @@TODO: */
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>

struct GRAPH_OBJ_TAG {
  int dummy;
};

GRAPH_OBJ *graph_alloc(int32_t count) {
  printf("graph_alloc %d\n", (int) count);
  return (GRAPH_OBJ *) malloc(sizeof(GRAPH_OBJ));
}
void graph_addref(GRAPH_OBJ *pg) {
  
}
void graph_release(GRAPH_OBJ *pg) {
  
}
void graph_set(
  GRAPH_OBJ *pg, int32_t i, int32_t t, int32_t ra, int32_t rb) {
  printf("graph_set %d %d %d %d\n", (int) i, (int) t, (int) ra,
    (int) rb);
}
