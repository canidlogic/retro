#ifndef LAYER_H_INCLUDED
#define LAYER_H_INCLUDED

/*
 * layer.h
 * 
 * Layer module of the Retro synthesizer.
 */

/* @@TODO: */
#include <stdint.h>
#include "graph.h"

/*
 * The maximum number of layers that may be defined.
 * 
 * The layer table is statically allocated, so be careful of setting
 * this too high.
 */
#define LAYER_MAXCOUNT (16384)

/* @@TODO: */
void layer_define(int32_t layer, double mul, GRAPH_OBJ *pg);
void layer_derive(int32_t target, int32_t source, double mul);
int16_t layer_get(int32_t layer, int32_t t);

#endif
