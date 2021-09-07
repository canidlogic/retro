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
 * Also requires libshastina, beta version 0.9.2 or compatible.
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
#define GENMAP_OK   (0)     /* No error */

/*
 * Type declarations
 * -----------------
 */

/*
 * GENMAP_PASS structure prototype.
 * 
 * Definition given in implementation file.
 */
struct GENMAP_PASS_TAG;
typedef struct GENMAP_PASS_TAG GENMAP_PASS;

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
 * Perform the first pass over a generator map Shastina script.
 * 
 * pIn is the Shastina source to read the Shastina script from.
 * 
 * perr points to a variable to receive an error code and pline points
 * to a variable to receive a line number.  See the errcode and linenum
 * fields in the GENMAP_RESULT structure for further information.
 * 
 * The returned pass object can then be used with genmap_run() to
 * actually build the generator map in a second pass.
 * 
 * The pass object should eventually be freed with genmap_freepass().
 * 
 * Parameters:
 * 
 *   pIn - the Shastina input source
 * 
 *   perr - pointer to variable to receive error code, or NULL
 * 
 *   pline - pointer to variable to receive error line number, or NULL
 * 
 * Return:
 * 
 *   a newly constructed pass object, or NULL if first pass failed
 */
GENMAP_PASS *genmap_pass(SNSOURCE *pIn, int *perr, long *pline);

/*
 * Free an allocated pass object.
 * 
 * Ignored if NULL is passed.
 * 
 * Parameters:
 * 
 *   pPass - the first-pass object to free
 */
void genmap_freepass(GENMAP_PASS *pPass);

/*
 * Interpret a generator map Shastina script.
 * 
 * pIn is the Shastina source to read the Shastina script from.  The
 * source should be reset back to the beginning of input so that this
 * is the second pass after genmap_pass().
 * 
 * pPass is the result of running the first-pass processor.  Use the
 * genmap_pass() function to perform the first pass.  You must pass over
 * the same source data twice, once with that function and once with
 * this function.  Caller still owns this structure, and it is still
 * caller's responsibility to free it.
 * 
 * pResult is the structure to store the result of interpreting the
 * Shastina script in.  See that structure for further details.
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
    GENMAP_PASS   * pPass,
    GENMAP_RESULT * pResult);

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
