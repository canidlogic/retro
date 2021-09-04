/*
 * layer.c
 * 
 * Implementation of layer.h
 * 
 * See the header for further information.
 */

/* @@TODO: */
#include "layer.h"
#include <stdio.h>
void layer_define(int32_t layer, double mul, GRAPH_OBJ *pg) {
  printf("layer_define %d %f\n", (int) layer, mul);
}
void layer_derive(int32_t target, int32_t source, double mul) {
  printf("layer_derive %d %d %f\n", (int) target, (int) source, mul);
}
int16_t layer_get(int32_t layer, int32_t t) {
  return GRAPH_MAXVAL;
}
