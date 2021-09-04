#ifndef LAYER_H_INCLUDED
#define LAYER_H_INCLUDED

/*
 * layer.h
 * 
 * Layer module of the Retro synthesizer.
 * 
 * Compilation
 * ===========
 * 
 * May require the math library -lm
 */

#include "retrodef.h"
#include "graph.h"

/*
 * The maximum number of layers that may be defined.
 * 
 * The layer table is statically allocated, so be careful of setting
 * this too high.
 */
#define LAYER_MAXCOUNT (16384)

/*
 * Clear a layer register.
 * 
 * layer is the index of the layer.  It must be in the range
 * [0, LAYER_MAXCOUNT - 1].
 * 
 * If no layer is defined for that register, this function has no
 * effect.  Otherwise, it releases the given layer, returning the
 * register to a clear state.
 * 
 * Parameters:
 * 
 *   layer - the layer to clear
 */
void layer_clear(int32_t layer);

/*
 * Define a layer.
 * 
 * layer is the index of the layer.  It must be in the range
 * [0, LAYER_MAXCOUNT - 1].
 * 
 * This function automatically calls layer_clear() on the given layer
 * register before setting it.
 * 
 * mul is the multiplier to use for the layer register.  This must be a
 * finite value that is in range [0.0, 1.0].  If it is 0.0, then the
 * call is equivalent to layer_clear().  If it is 1.0, then the graph
 * values are used as-is.  Otherwise, each graph value is multiplied by
 * mul when layer_get() is used, so that everything in the layer is
 * scaled by mul.
 * 
 * pg is the graph object to use.  This function will add a reference
 * count to the given graph object.
 * 
 * Parameters:
 * 
 *   layer - the layer index
 * 
 *   mul - the multiplier
 * 
 *   pg - the graph object
 */
void layer_define(int32_t layer, double mul, GRAPH_OBJ *pg);

/*
 * Derive one layer from another by a constant multiplier.
 * 
 * target is the target layer to derive, and source is the layer to
 * derive from.  Both must be in range [0, LAYER_MAXCOUNT - 1].  If
 * target equals source, then if the layer is defined, this simply
 * changes the multiplier.  (If the layer is undefined, the call does
 * nothing.)
 * 
 * If target and source are different, then the target register is
 * cleared with layer_clear() and the graph object of the source
 * register is copied over.  The reference count of the graph object is
 * increased, so the same graph object is shared between the layers.  If
 * the source is undefined, then this has the effect of clearing the
 * target register.
 * 
 * mul is used to set the multiplier on the derived layer.  It must be
 * finite and in range [0.0, 1.0].  If the multiplier is 0.0, then this
 * function simply has the effect of clearing the target register.
 * 
 * Parameters:
 * 
 *   target - the target register
 * 
 *   source - the source register to copy from
 * 
 *   mul - the new multiplier
 */
void layer_derive(int32_t target, int32_t source, double mul);

/*
 * Compute an intensity value from the given layer.
 * 
 * layer is the layer index, in range [0, LAYER_MAXCOUNT - 1].
 * 
 * t is the time offset, which must be zero or greater.
 * 
 * If the given layer is defined, then the result is computed value from
 * the graph at that time t, scaled by the multiplier.
 * 
 * If the given layer is undefined, this function always returns zero.
 * 
 * The return value is in range [0, MAX_FRAC].
 * 
 * Parameters:
 * 
 *   layer - the layer index
 * 
 *   t - the time offset
 * 
 * Return:
 * 
 *   the computed intensity value
 */
int16_t layer_get(int32_t layer, int32_t t);

#endif
