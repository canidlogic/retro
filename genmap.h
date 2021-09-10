#ifndef GENMAP_H_INCLUDED
#define GENMAP_H_INCLUDED

/*
 * genmap.h
 * ========
 * 
 * Retro module for constructing a generator graph from a Shastina
 * description.
 * 
 * See "genmap.txt" in the doc directory for an example generator map
 * Shastina script.
 * 
 * Compile with adsr and generator modules of Retro.
 * 
 * Also requires libshastina, beta version 0.9.3 or compatible.
 * 
 * May require the math library to be linked with -lm
 */

#include "retrodef.h"
#include "generator.h"
#include "shastina.h"

/*
 * Constants
 * ---------
 */

/*
 * Error codes.
 * 
 * Values less than zero are Shastina error codes.  Zero is always
 * GENMAP_OK, which means no error.  Greater than zero are
 * genmap-specific error codes.
 * 
 * Use genmap_errstr() to get an error message string for an error code.
 */
#define GENMAP_OK           (0)   /* No error */
#define GENMAP_ERR_DUPNAME  (1)   /* Duplicate variable/const name */
#define GENMAP_ERR_PASSONE  (2)   /* Can't find name from pass one */
#define GENMAP_ERR_UNDEF    (3)   /* Undefined variable or constant */
#define GENMAP_ERR_SETCONST (4)   /* Can't change constant value */
#define GENMAP_ERR_UNDERFLW (5)   /* Stack underflow */
#define GENMAP_ERR_OVERFLOW (6)   /* Stack overflow */
#define GENMAP_ERR_NESTING  (7)   /* Too much group nesting */
#define GENMAP_ERR_GROUPCHK (8)   /* Group check failed */
#define GENMAP_ERR_OPENGRP  (9)   /* Open group at end of script */
#define GENMAP_ERR_FINAL    (10)  /* Invalid final stack state */
#define GENMAP_ERR_RESULTYP (11)  /* Wrong type for result */

/*
 * Type declarations
 * -----------------
 */

/*
 * Structure storing the result of interpreting a generator map Shastina
 * script.
 */
typedef struct {
  
  /*
   * The error code result of the operation.
   * 
   * GENMAP_OK (0) if operation succeeded, else operation failed and
   * this is the error code.  Use genmap_errstr() to convert to an error
   * message.
   */
  int errcode;
  
  /*
   * The line number the error occurred at.
   * 
   * Only if errcode is not GENMAP_OK.  Zero if there is no line number
   * information.
   */
  long linenum;
  
  /*
   * Pointer to the root generator object of the constructed generator
   * map.
   * 
   * Only if errcode is GENMAP_OK, else this is NULL.
   * 
   * This object is newly constructed and has a single reference.  It
   * should eventually be freed with generator_release().
   */
  GENERATOR *pRoot;
  
  /*
   * The number of required GENERATOR_OPDATA structures required for an
   * instance of the generator map.
   * 
   * Only if errcode is GENMAP_OK, else this is zero.  If errcode is
   * GENMAP_OK, this will always be at least one.
   * 
   * The generator map indicated by pRoot is the "class" data for
   * generators.  To actually generate a sound, you must create a
   * specific instance of the generator map "class."  Do this by
   * allocating an array of GENERATOR_OPDATA structures with a number of
   * elements equal to this field value.  Then, initialize each of these
   * structures with generator_opdata_init().  This array of instance
   * data will be required on calls to generator_invoke() and to the
   * function generator_length().
   */
  int32_t icount;
  
} GENMAP_RESULT;

/*
 * Public functions
 * ----------------
 */

/*
 * Interpret a generator map Shastina script.
 * 
 * pIn is the Shastina source to read the Shastina script from.  The
 * given source must support multipass or a fault occurs.
 * 
 * pResult is the structure to store the result of interpreting the
 * Shastina script in.  See that structure for further details.
 * 
 * samp_rate is the sampling rate.  It must be RATE_CD or RATE_DVD.
 * 
 * CAUTION:  Do not use a result structure more than once unless you
 * free any generator within it.  Otherwise, a memory leak will occur.
 * 
 * The generator object within the result structure should eventually
 * be freed with generator_release().
 * 
 * Parameters:
 * 
 *   pIn - the Shastina input source
 * 
 *   pPass - the result of the first pass
 * 
 *   pResult - the structure to receive the interpretation result
 */
void genmap_run(
    SNSOURCE      * pIn,
    GENMAP_RESULT * pResult,
    int32_t         samp_rate);

/*
 * Convert an error code in the GENMAP_RESULT structure to a string.
 * 
 * The returned string begins with an uppercase letter but has no
 * punctuation nor line break at the end.  The string is statically
 * allocated, so do not attempt to free it.
 * 
 * This function will properly handle Shastina error codes as well.
 * 
 * If GENMAP_OK (0) is passed, "No error" is returned.  If an
 * unrecognized error code is passed, "Unknown error" is returned.
 * 
 * Parameters:
 * 
 *   code - the error code
 * 
 * Return:
 * 
 *   the corresponding error message
 */
const char *genmap_errstr(int code);

#endif
