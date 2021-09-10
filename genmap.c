/*
 * genmap.c
 * ========
 * 
 * Implementation of genmap.h
 * 
 * See the header for further information.
 */

#include "genmap.h"
#include "adsr.h"

#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * ---------
 */

/*
 * GENVAR types.
 */
#define GENVAR_UNDEF  (0)
#define GENVAR_INT    (1)
#define GENVAR_FLOAT  (2)
#define GENVAR_ATOM   (3)
#define GENVAR_ADSR   (4)
#define GENVAR_GENOBJ (5)

/*
 * VARCELL status values.
 */
#define VARCELL_UNDEF (0)
#define VARCELL_VAR   (1)
#define VARCELL_CONST (2)

/*
 * The initial and maximum capacities of the interpreter stack.
 * 
 * The interpreter stack starts out with a capacity of ISTACK_INIT and
 * grows by doubling up to a limit of ISTACK_MAX.
 */
#define ISTACK_INIT   (16)
#define ISTACK_MAX    (INT32_C(65535))

/*
 * The maximum number of nested groups during interpretation.
 * 
 * This determines the size of the group stack, so be careful about
 * setting it to a large value.
 */
#define ISTACK_NEST   (32)

/*
 * Type declarations
 * -----------------
 */

/*
 * NAME_LINK structure.
 * 
 * Preceded by structure prototype to allow for self reference.
 * 
 * This is a single-linked list that stores all the variable and
 * constant name strings read during the first pass in the order they
 * appear in input.
 */
struct NAME_LINK_TAG;
typedef struct NAME_LINK_TAG NAME_LINK;
struct NAME_LINK_TAG {
  
  /*
   * Pointer to next name, or NULL if last name.
   */
  NAME_LINK *pNext;
  
  /*
   * The current name, dynamically allocated.
   */
  char *pName;
};

/*
 * NAME_DICT structure.
 * 
 * Use name_dict functions to manipulate
 */
typedef struct {
  
  /*
   * The number of names in the array, not including the NULL pointer at
   * the end of the array.
   * 
   * May be zero.
   */
  int32_t name_count;
  
  /*
   * A dynamically allocated array storing pointers to dynamically
   * allocated name strings.
   * 
   * There will be a NULL pointer at the end of the array.
   * 
   * Names will be sorted in strcmp() order.
   */
  char **ppNames;
  
} NAME_DICT;

/*
 * GENVAR structure.
 * 
 * Stores stack variables, variable, and constant values.  Use the
 * genvar_ functions to manipulate.
 */
typedef struct {
  
  /*
   * The type stored in this structure.
   * 
   * Must be one of the GENVAR_ constants.
   */
  int vtype;
  
  /*
   * The val field is a union of the different type possibilities.
   */
  union {
    
    /*
     * Dummy field used for GENVAR_UNDEF.
     */
    int dummy;
    
    /*
     * Integer value for GENVAR_INT.
     */
    int32_t ival;
    
    /*
     * Double value for GENVAR_FLOAT.
     */
    double fval;
    
    /*
     * Atom code for GENVAR_ATOM.
     */
    int atom;
    
    /*
     * ADSR object reference for GENVAR_ADSR.
     */
    ADSR_OBJ *pADSR;
    
    /*
     * Generator object reference for GENVAR_GENOBJ.
     */
    GENERATOR *pGen;
    
  } val;
  
} GENVAR;

/*
 * VARCELL structure.
 * 
 * Used in the bank of variables and constants.
 */
typedef struct {
  
  /*
   * One of the VARCELL_ constants indicating the status of this cell.
   */
  int status;
  
  /*
   * The value held in this cell.
   * 
   * If status is VARCELL_UNDEF, this value is always undef.
   * 
   * If status is VARCELL_VAR, then this value is read/write.
   * 
   * If status is VARCELL_CONST, then this value is read-only.
   */
  GENVAR gv;
  
} VARCELL;

/*
 * ISTATE structure.
 * 
 * Stores the interpreter state.
 */
typedef struct {
  
  /*
   * Name dictionary that maps variable and constant names to indices in
   * the register bank.
   * 
   * Owned by the ISTATE structure.
   */
  NAME_DICT *pDict;
  
  /*
   * The register bank of variables and constants.
   * 
   * This is an array of VARCELL structures.  The length of the array is
   * the count of names in the pDict dictionary.  If this count is zero,
   * this pointer is NULL.
   * 
   * The pDict dictionary determines how variables and constants map to
   * indices in this register bank by their names.
   */
  VARCELL *pBank;
  
  /*
   * The current capacity of the interpreter stack, in GENVAR
   * structures.
   */
  int32_t cap;
  
  /*
   * The current height of the interpreter stack, in GENVAR structures.
   */
  int32_t height;
  
  /*
   * The interpreter stack.
   * 
   * This is an array of GENVAR structures.  The length of the array is
   * determined by the cap field.  If cap is zero, this pointer is NULL.
   * 
   * The first allocation will be for ISTACK_INIT capacity.  The stack
   * grows by doubling up to a maximum of ISTACK_MAX capacity.
   * 
   * The height field determines how many elements are currently on the
   * stack.  The stack grows upwards.
   */
  GENVAR *pStack;
  
  /*
   * The current height of the grouping stack, in integer values.
   */
  int32_t gheight;
  
  /*
   * The grouping stack.
   * 
   * The number of elements on this stack is determined by the gheight
   * field.  The stack grows upwards.
   * 
   * Each time a Shastina group begins, the current height of the
   * interpreter stack is pushed on top of gstack.  All interpreter
   * stack items up to the height stored on top of gstack are invisible
   * to the interpreted script until the group is closed.  If gstack is
   * empty, no items are hidden from the interpreter stack.
   * 
   * Each time a Shastina group ends, a check is made that the current
   * interpreter stack height is one greater than the stack height
   * stored on top of gstack, causing a group check error if this is not
   * the case.  The top of the group stack is then popped, which unhides
   * any stack elements that were hidden by the group.
   * 
   * The nesting depth is limited by ISTACK_NEST.
   */
  int32_t gstack[ISTACK_NEST];
  
} ISTATE;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static void genvar_init(GENVAR *pgv);
static void genvar_clear(GENVAR *pgv);
static void genvar_copy(GENVAR *pDest, const GENVAR *pSrc);
static int genvar_type(const GENVAR *pgv);
static int genvar_canfloat(const GENVAR *pgv);

static int32_t genvar_getInt(const GENVAR *pgv);
static double genvar_getFloat(const GENVAR *pgv);
static int genvar_getAtom(const GENVAR *pgv);
static ADSR_OBJ *genvar_getADSR(const GENVAR *pgv);
static GENERATOR *genvar_getGen(const GENVAR *pgv);

static void genvar_setInt(GENVAR *pgv, int32_t v);
static void genvar_setFloat(GENVAR *pgv, double v);
static void genvar_setAtom(GENVAR *pgv, int atom);
static void genvar_setADSR(GENVAR *pgv, ADSR_OBJ *pADSR);
static void genvar_setGen(GENVAR *pgv, GENERATOR *pGen);

static ISTATE *istate_new(NAME_DICT *pNames);
static void istate_free(ISTATE *ps);
static int istate_define(
          ISTATE * ps,
    const char   * pName,
          int      is_const,
    const GENVAR * val,
          int    * perr);
static int istate_set(
          ISTATE * ps,
    const char   * pName,
    const GENVAR * val,
          int    * perr);
static int istate_get(
          ISTATE * ps,
    const char   * pName,
          GENVAR * pDest,
          int    * perr);
static int32_t istate_height(ISTATE *ps);
static int istate_index(
    ISTATE  * ps,
    int32_t   i,
    GENVAR  * pDest,
    int     * perr);
static int istate_pop(ISTATE *ps, int32_t count, int *perr);
static int istate_push(ISTATE *ps, const GENVAR *pv, int *perr);
static int istate_grouped(ISTATE *ps);
static int istate_begin(ISTATE *ps, int *perr);
static int istate_end(ISTATE *ps, int *perr);

static void free_names(NAME_LINK *pNames);
static NAME_LINK *gather_names(SNSOURCE *pIn, int *perr, long *pline);

static int name_sort(const void *pa, const void *pb);
static int name_search(const void *pa, const void *pb);

static NAME_DICT *make_dict(NAME_LINK *pNames);
static void free_dict(NAME_DICT *pd);
static int32_t count_dict(NAME_DICT *pd);
static int32_t name_index(NAME_DICT *pd, const char *pName);

/*
 * Initialize a GENVAR structure and set it to undefined.
 * 
 * CAUTION:  Do not initialize a GENVAR structure that has already been
 * initialized, or a memory leak may occur!
 * 
 * The structure must be cleared with genvar_clear() before it is freed
 * to prevent a memory leak.
 * 
 * Parameters:
 * 
 *   pgv - the structure to initialize
 */
static void genvar_init(GENVAR *pgv) {
  
  /* Check parameter */
  if (pgv == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pgv, 0, sizeof(GENVAR));
  
  pgv->vtype = GENVAR_UNDEF;
  (pgv->val).dummy = 0;
}

/*
 * Clear an initialized GENVAR structure back to undefined.
 * 
 * You must use this function to clear initialized GENVAR structures
 * before freeing them to prevent memory leaks.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 */
static void genvar_clear(GENVAR *pgv) {
  
  /* Check parameter */
  if (pgv == NULL) {
    abort();
  }
  
  /* Release any object references */
  if (pgv->vtype == GENVAR_ADSR) {
    adsr_release((pgv->val).pADSR);
    (pgv->val).pADSR = NULL;
    
  } else if (pgv->vtype == GENVAR_GENOBJ) {
    generator_release((pgv->val).pGen);
    (pgv->val).pGen = NULL;
  }
  
  /* Clear the structure back to undefined */
  memset(pgv, 0, sizeof(GENVAR));
  
  pgv->vtype = GENVAR_UNDEF;
  (pgv->val).dummy = 0;
}

/*
 * Copy the value of one GENVAR structure to another.
 * 
 * pSrc is the source structure to copy from, and pDest is the
 * destination structure to copy to.  If both pointers indicate the same
 * structure, the call is ignored.  Otherwise, pDest is cleared with
 * genvar_clear() and then the value from pSrc is copied into it.
 * 
 * If an object reference is copied, the reference count is increased by
 * one.
 * 
 * Both source and destination structures must already be initialized.
 * 
 * Parameters:
 * 
 *   pDest - the destination structure
 * 
 *   pSrc - the source structure
 */
static void genvar_copy(GENVAR *pDest, const GENVAR *pSrc) {
  
  /* Check parameters */
  if ((pDest == NULL) || (pSrc == NULL)) {
    abort();
  }
  
  /* Only proceed if the structures are different */
  if (pDest != pSrc) {
  
    /* Clear the destination structure */
    genvar_clear(pDest);
    
    /* Copy the source structure to destination */
    memcpy(pDest, pSrc, sizeof(GENVAR));
    
    /* If we copied an object reference, increment reference count */
    if (pDest->vtype == GENVAR_ADSR) {
      adsr_addref((pDest->val).pADSR);
      
    } else if (pDest->vtype == GENVAR_GENOBJ) {
      generator_addref((pDest->val).pGen);
    }
  }
}

/*
 * Return the type of the value stored in a GENVAR structure.
 * 
 * The given structure must already be initialized.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   one of the GENVAR_ constants indicating the type stored in the
 *   structure
 */
static int genvar_type(const GENVAR *pgv) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Return type */
  return pgv->vtype;
}

/*
 * Check whether a GENVAR structure is storing either an integer or a
 * floating-point value.
 * 
 * Integers can be automatically promoted to floating-point when the
 * function genvar_getFloat() is used, hence the utility of this
 * function.
 * 
 * The given structure must already be initialized.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   non-zero if structure storing an integer or a floating-point value,
 *   zero otherwise
 */
static int genvar_canfloat(const GENVAR *pgv) {
  
  int result = 0;
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Check type */
  if ((pgv->vtype == GENVAR_INT) || (pgv->vtype == GENVAR_FLOAT)) {
    result = 1;
  } else {
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * Return the integer value stored in a GENVAR structure.
 * 
 * The given structure must already be initialized.  The type held in
 * the structure must be an integer or a fault occurs.  Use the function
 * genvar_type() to check the type.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   the integer value contained within
 */
static int32_t genvar_getInt(const GENVAR *pgv) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Check type */
  if (pgv->vtype != GENVAR_INT) {
    abort();
  }
  
  /* Return integer value */
  return (pgv->val).ival;
}

/*
 * Return the floating-point value stored in a GENVAR structure.
 * 
 * If an integer is stored in the structure, this function will
 * automatically promote the integer to a floating-point value.  The
 * value stored in the structure remains an integer, however.
 * 
 * The given structure must already be initialized.  The type held in
 * the structure must be an integer or a float, or a fault occurs.  Use
 * the function genvar_type() to check the type, or the specialized
 * function genvar_canfloat() to check for either an integer or a
 * floating-point value.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   the floating-point value contained within, or promoted from the
 *   integer value
 */
static double genvar_getFloat(const GENVAR *pgv) {
  
  double result = 0.0;
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Handle different types */
  if (pgv->vtype == GENVAR_FLOAT) {
    /* Float stored in structure, so return that */
    result = (pgv->val).fval;
    
  } else if (pgv->vtype == GENVAR_INT) {
    /* Integer stored in structure, so promote that */
    result = (double) ((pgv->val).ival);
    if (!isfinite(result)) {
      abort();
    }
    
  } else {
    /* Unsupported type */
    abort();
  }
  
  /* Return result */
  return result;
}

/*
 * Return the atom value stored in a GENVAR structure.
 * 
 * The given structure must already be initialized.  The type held in
 * the structure must be an atom or a fault occurs.  Use the function
 * genvar_type() to check the type.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   the integer value of the atom value contained within
 */
static int genvar_getAtom(const GENVAR *pgv) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Check type */
  if (pgv->vtype != GENVAR_ATOM) {
    abort();
  }
  
  /* Return integer value */
  return (pgv->val).atom;
}

/*
 * Return the ADSR object stored in a GENVAR structure.
 * 
 * The given structure must already be initialized.  The type held in
 * the structure must be an ADSR object reference or a fault occurs.
 * Use the function genvar_type() to check the type.
 * 
 * The reference count of the returned ADSR object is not modified by
 * this function.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   the ADSR object reference contained within
 */
static ADSR_OBJ *genvar_getADSR(const GENVAR *pgv) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Check type */
  if (pgv->vtype != GENVAR_ADSR) {
    abort();
  }
  
  /* Return the object reference */
  return (pgv->val).pADSR;
}

/*
 * Return the generator object stored in a GENVAR structure.
 * 
 * The given structure must already be initialized.  The type held in
 * the structure must be a generator object reference or a fault occurs.
 * Use the function genvar_type() to check the type.
 * 
 * The reference count of the returned generator object is not modified
 * by this function.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 * Return:
 * 
 *   the generator object reference contained within
 */
static GENERATOR *genvar_getGen(const GENVAR *pgv) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Check type */
  if (pgv->vtype != GENVAR_GENOBJ) {
    abort();
  }
  
  /* Return the object reference */
  return (pgv->val).pGen;
}

/*
 * Set an integer value in the given GENVAR structure.
 * 
 * The given structure must already be initialized.  It will first be
 * cleared using genvar_clear() and then the given integer value will be
 * written into it.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 *   v - the integer value to store
 */
static void genvar_setInt(GENVAR *pgv, int32_t v) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Clear to undefined */
  genvar_clear(pgv);
  
  /* Write the integer value */
  pgv->vtype = GENVAR_INT;
  (pgv->val).ival = v;
}

/*
 * Set a float value in the given GENVAR structure.
 * 
 * The given structure must already be initialized.  It will first be
 * cleared using genvar_clear() and then the given float value will be
 * written into it.
 * 
 * The given float value must be finite or a fault occurs.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 *   v - the float value to store
 */
static void genvar_setFloat(GENVAR *pgv, double v) {
  
  /* Check parameters */
  if ((pgv == NULL) || (!isfinite(v))) {
    abort();
  }
  
  /* Clear to undefined */
  genvar_clear(pgv);
  
  /* Write the float value */
  pgv->vtype = GENVAR_FLOAT;
  (pgv->val).fval = v;
}

/*
 * Set an atom value in the given GENVAR structure.
 * 
 * The given structure must already be initialized.  It will first be
 * cleared using genvar_clear() and then the given atom value will be
 * written into it.
 * 
 * The given atom value can have any integer value.  It should
 * correspond to the unique integer value of the atom.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 *   atom - the atom value to store
 */
static void genvar_setAtom(GENVAR *pgv, int atom) {
  
  /* Check parameters */
  if (pgv == NULL) {
    abort();
  }
  
  /* Clear to undefined */
  genvar_clear(pgv);
  
  /* Write the integer value */
  pgv->vtype = GENVAR_ATOM;
  (pgv->val).atom = atom;
}

/*
 * Set an ADSR object reference in the given GENVAR structure.
 * 
 * The given structure must already be initialized.  It will first be
 * cleared using genvar_clear() and then the given ADSR object reference
 * will be written into it.
 * 
 * The reference count of the ADSR object will be incremented by this
 * function.
 * 
 * CAUTION:  This function is only intended for transferring an object
 * reference that is not already stored in GENVAR structure.  If you
 * take an object reference that is already stored in a GENVAR structure
 * and try storing it in the same GENVAR structure using this function,
 * a dangling pointer will result.  Use genvar_copy() to copy references
 * safely between GENVAR structures.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 *   pADSR - the ADSR object reference to store
 */
static void genvar_setADSR(GENVAR *pgv, ADSR_OBJ *pADSR) {
  
  /* Check parameters */
  if ((pgv == NULL) || (pADSR == NULL)) {
    abort();
  }
  
  /* Clear to undefined */
  genvar_clear(pgv);
  
  /* Write the object reference */
  pgv->vtype = GENVAR_ADSR;
  (pgv->val).pADSR = pADSR;
  
  /* Increment reference count */
  adsr_addref(pADSR);
}

/*
 * Set a generator object reference in the given GENVAR structure.
 * 
 * The given structure must already be initialized.  It will first be
 * cleared using genvar_clear() and then the given generator object
 * reference will be written into it.
 * 
 * The reference count of the generator object will be incremented by
 * this function.
 * 
 * CAUTION:  This function is only intended for transferring an object
 * reference that is not already stored in GENVAR structure.  If you
 * take an object reference that is already stored in a GENVAR structure
 * and try storing it in the same GENVAR structure using this function,
 * a dangling pointer will result.  Use genvar_copy() to copy references
 * safely between GENVAR structures.
 * 
 * Parameters:
 * 
 *   pgv - the initialized structure
 * 
 *   pGen - the generator object reference to store
 */
static void genvar_setGen(GENVAR *pgv, GENERATOR *pGen) {
  
  /* Check parameters */
  if ((pgv == NULL) || (pGen == NULL)) {
    abort();
  }
  
  /* Clear to undefined */
  genvar_clear(pgv);
  
  /* Write the object reference */
  pgv->vtype = GENVAR_GENOBJ;
  (pgv->val).pGen = pGen;
  
  /* Increment reference count */
  generator_addref(pGen);
}

/* @@TODO: */
static ISTATE *istate_new(NAME_DICT *pNames);
static void istate_free(ISTATE *ps);
static int istate_define(
          ISTATE * ps,
    const char   * pName,
          int      is_const,
    const GENVAR * val,
          int    * perr);
static int istate_set(
          ISTATE * ps,
    const char   * pName,
    const GENVAR * val,
          int    * perr);
static int istate_get(
          ISTATE * ps,
    const char   * pName,
          GENVAR * pDest,
          int    * perr);
static int32_t istate_height(ISTATE *ps);
static int istate_index(
    ISTATE  * ps,
    int32_t   i,
    GENVAR  * pDest,
    int     * perr);
static int istate_pop(ISTATE *ps, int32_t count, int *perr);
static int istate_push(ISTATE *ps, const GENVAR *pv, int *perr);
static int istate_grouped(ISTATE *ps);
static int istate_begin(ISTATE *ps, int *perr);
static int istate_end(ISTATE *ps, int *perr);

/*
 * Release a linked-list of names.
 * 
 * Does nothing if NULL is passed.
 * 
 * Parameters:
 * 
 *   pNames - the first node in the chain or NULL
 */
static void free_names(NAME_LINK *pNames) {
  
  NAME_LINK *pl = NULL;
  
  /* Go through all name elements */
  pl = pNames;
  while(pl != NULL) {
    /* Free current string copy if allocated */
    if (pl->pName != NULL) {
      free(pl->pName);
      pl->pName = NULL;
    }
    
    /* Update name pointer to point to this node */
    pNames = pl;
    
    /* Update node pointer to next node */
    pl = pl->pNext;
    
    /* Free the node that pNames points to */
    free(pNames);
    pNames = NULL;
  }
}

/*
 * Go through a Shastina input source and gather all the names of
 * defined variables and constants in a single-linked list.
 * 
 * The return value will be NULL both in the case of an error and in the
 * case of no names found, so be sure to check *perr to determine
 * whether the operation succeeded.
 * 
 * perr points to a variable to receive an error code or GENMAP_OK if
 * successful.  pline points to a variable to receive an error line
 * number or zero.
 * 
 * The given Shastina source must be multipass.  It is rewound at the
 * start of this function and the function goes through to the EOF
 * marker.
 * 
 * The returned list should eventually be freed with free_names().
 * 
 * Parameters:
 * 
 *   pIn - the input source
 * 
 *   perr - pointer to variable to receive error status
 * 
 *   pline - pointer to variable to receive line number
 * 
 * Return:
 * 
 *   the dynamically allocated linked list of names, or NULL if no names
 *   or error
 */
static NAME_LINK *gather_names(SNSOURCE *pIn, int *perr, long *pline) {
  
  int status = 1;
  SNENTITY ent;
  SNPARSER *pParse = NULL;
  NAME_LINK *pFirst = NULL;
  NAME_LINK *pLast = NULL;
  NAME_LINK *pl = NULL;
  char *pCopy = NULL;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check parameters */
  if ((pIn == NULL) || (perr == NULL) || (pline == NULL)) {
    abort();
  }
  if (!snsource_ismulti(pIn)) {
    abort();
  }
  
  /* Reset error and line information */
  *perr = GENMAP_OK;
  *pline = 0;
  
  /* Rewind the source */
  if (!snsource_rewind(pIn)) {
    status = 0;
    *perr = SNERR_IOERR;
  }
  
  /* Allocate parser */
  if (status) {
    pParse = snparser_alloc();
  }
  
  /* Go through all entities */
  if (status) {
    for(snparser_read(pParse, &ent, pIn);
        ent.status > 0;
        snparser_read(pParse, &ent, pIn)) {
      
      /* We read an entity that is not EOF -- only do something if we
       * have a VARIABLE or CONSTANT entity */
      if ((ent.status == SNENTITY_VARIABLE) ||
          (ent.status == SNENTITY_CONSTANT)) {
        
        /* Get a copy of the variable or constant name */
        pCopy = (char *) malloc(strlen(ent.pKey) + 1);
        if (pCopy == NULL) {
          abort();
        }
        strcpy(pCopy, ent.pKey);
        
        /* Allocate a new link and put the string copy therein */
        pl = (NAME_LINK *) malloc(sizeof(NAME_LINK));
        if (pl == NULL) {
          abort();
        }
        memset(pl, 0, sizeof(NAME_LINK));
        
        pl->pNext = NULL;
        pl->pName = pCopy;
        pCopy = NULL;
        
        /* Add the name to the linked list */
        if (pLast == NULL) {
          /* This is the first link in the chain */
          pFirst = pl;
          pLast = pl;
          
        } else {
          /* The chain already exists, so append */
          pLast->pNext = pl;
          pLast = pl;
        }
        
        pl = NULL;
      }
    }
    
    /* Check if we stopped on a parser error */
    if (status && (ent.status < 0)) {
      status = 0;
      *perr = ent.status;
      *pline = snparser_count(pParse);
    }
  }
  
  /* Free parser if allocated */
  snparser_free(pParse);
  pParse = NULL;
  
  /* If failure, then free the whole chain */
  if (!status) {
    free_names(pFirst);
    pFirst = NULL;
    pLast = NULL;
  }
  
  /* Return the name chain or NULL */
  return pFirst;
}

/*
 * Comparison function between name array entries.
 * 
 * Interface matches function pointer for qsort().
 */
static int name_sort(const void *pa, const void *pb) {
  
  const char **pac = NULL;
  const char **pab = NULL;
  
  /* Check parameters */
  if ((pa == NULL) || (pb == NULL)) {
    abort();
  }
  
  /* Cast pointers */
  pac = (const char **) pa;
  pab = (const char **) pb;
  
  /* Use string comparison */
  return strcmp(*pac, *pab);
}

/*
 * Comparison function between a name key and a name array entry.
 * 
 * Interface matches function pointer for bsearch().
 */
static int name_search(const void *pa, const void *pb) {
  
  const char *pKey = NULL;
  const char **pe = NULL;
  
  /* Check parameters */
  if ((pa == NULL) || (pb == NULL)) {
    abort();
  }
  
  /* Cast pointers */
  pKey = (const char *) pa;
  pe = (const char **) pb;
  
  /* Use string comparison */
  return strcmp(pKey, *pe);
}

/*
 * Given a linked list of variable and constant names, turn it into a
 * name dictionary object.
 * 
 * The pNames list will be freed by this function.  The name strings
 * will be transferred directly to the dictionary.  Passing a NULL
 * parameter is OK, and it means an empty dictionary should be
 * constructed.
 * 
 * The function fails and NULL is returned if there are duplicate names.
 * The linked list is still freed in this case.  A fault occurs if any
 * name pointer in the linked list is NULL.  A fault also occurs in the
 * likely event that the count of names overflows the 32-bit counter.
 * 
 * The dictionary should eventually be freed with free_dict().
 * 
 * Parameters:
 * 
 *   pNames - the linked list of names
 * 
 * Return:
 * 
 *   the new name dictionary object, or NULL if there were duplicate
 *   names
 */
static NAME_DICT *make_dict(NAME_LINK *pNames) {
  
  int status = 1;
  int32_t count = 0;
  int32_t i = 0;
  NAME_LINK *pl = NULL;
  NAME_DICT *pResult = NULL;
  char **ppn = NULL;
  char **pp = NULL;
  
  /* Count how many names there are */
  count = 0;
  for(pl = pNames; pl != NULL; pl = pl->pNext) {
    if (count < INT32_MAX - 1) {
      count++;
    } else {
      /* Too many names */
      abort();
    }
  }
  
  /* Allocate an array with one more than the total number of names */
  ppn = (char **) calloc(count + 1, sizeof(char *));
  if (ppn == NULL) {
    abort();
  }
  
  /* Copy names into array, clear names in original linked list, and
   * check that pointers are non-NULL */
  pp = ppn;
  for(pl = pNames; pl != NULL; pl = pl->pNext) {
    /* Check that name pointer is not NULL */
    if (pl->pName == NULL) {
      abort();
    }
    
    /* Move name into array */
    *pp = pl->pName;
    pl->pName = NULL;
    
    /* Advance array pointer */
    pp++;
  }
  
  /* Set last pointer in the array to NULL */
  ppn[count] = NULL;
  
  /* Free the linked list of names */
  free_names(pNames);
  pNames = NULL;
  
  /* If there is more than one name, sort all the name pointers and make
   * sure no duplicates */
  if (count > 1) {
    qsort(ppn, count, sizeof(char *), &name_sort);
    for(i = 1; i < count; i++) {
      if (strcmp(ppn[i], ppn[i - 1]) == 0) {
        /* Duplicate names */
        status = 0;
        break;
      }
    }
  }
  
  /* If we successfully got the sorted name array, then build the
   * dictionary object; else, free all the names and the name array */
  if (status) {
    /* Got the name array, so wrap in a dictionary object */
    pResult = (NAME_DICT *) malloc(sizeof(NAME_DICT));
    if (pResult == NULL) {
      abort();
    }
    memset(pResult, 0, sizeof(NAME_DICT));
    
    pResult->name_count = count;
    pResult->ppNames = ppn;
    ppn = NULL;
    
  } else {
    /* Duplicates were found, so clean up name array */
    for(i = 0; i < count; i++) {
      free(ppn[i]);
      ppn[i] = NULL;
    }
    free(ppn);
    ppn = NULL;
  }
  
  /* Return result or NULL */
  return pResult;
}

/*
 * Free a name dictionary.
 * 
 * If NULL is passed, the call is ignored.
 * 
 * Parameters:
 * 
 *   pd - the dictionary to free, or NULL
 */
static void free_dict(NAME_DICT *pd) {
  
  char **ppn = NULL;
  
  /* Only proceed if non-NULL value passed */
  if (pd != NULL) {
  
    /* Free all name strings in the name array */
    for(ppn = pd->ppNames; *ppn != NULL; ppn++) {
      free(*ppn);
      *ppn = NULL;
    }
  
    /* Free the name array */
    free(pd->ppNames);
    pd->ppNames = NULL;
  
    /* Free the dictionary structure */
    free(pd);
  }
}

/*
 * Count the total number of names in a given name dictionary.
 * 
 * The count is zero or greater.
 * 
 * Parameters:
 * 
 *   pd - the dictionary to check
 * 
 * Return:
 * 
 *   the number of names in the dictionary
 */
static int32_t count_dict(NAME_DICT *pd) {
  
  /* Check parameter */
  if (pd == NULL) {
    abort();
  }
  if (pd->name_count < 0) {
    abort();
  }
  
  /* Return count */
  return pd->name_count;
}

/*
 * Given a variable or constant name, look it up in the given name
 * dictionary and return a unique index value for the name.
 * 
 * The index value will be in range [0, count - 1], where count is the
 * total number of names, determined by count_dict().
 * 
 * If the given name is not found in the dictionary, -1 is returned.
 * 
 * Parameters:
 * 
 *   pd - the dictionary to check
 * 
 * Return:
 * 
 *   a unique index for the name, or -1 if the name was not found
 */
static int32_t name_index(NAME_DICT *pd, const char *pName) {
  
  int32_t result = 0;
  char **ppn = NULL;
  
  /* Check parameters */
  if ((pd == NULL) || (pName == NULL)) {
    abort();
  }
  
  /* Handle different cases depending on name count */
  if (pd->name_count < 1) {
    /* The dictionary has no names, so always return -1 */
    result = -1;
    
  } else if (pd->name_count == 1) {
    /* The dictionary has a single name, so just check that name */
    if (strcmp(pName, (pd->ppNames)[0]) == 0) {
      /* Matches the one name */
      result = 0;
    } else {
      /* Does not match the one name */
      result = -1;
    }
    
  } else {
    /* The dictionary has multiple names, so perform a binary search */
    ppn = bsearch(
            pName,
            &((pd->ppNames)[0]),
            (size_t) pd->name_count,
            sizeof(char *),
            &name_search);
    
    /* Determine result */
    if (ppn != NULL) {
      /* Name found, so determine index */
      result = (int32_t) (ppn - &((pd->ppNames)[0]));
      
    } else {
      /* No name found */
      result = -1;
    }
  }
  
  /* Return result or -1 */
  return result;
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See header for specifications.
 */

/*
 * genmap_run function.
 */
void genmap_run(
    SNSOURCE      * pIn,
    GENMAP_RESULT * pResult,
    int32_t         samp_rate) {
  
  int status = 1;
  NAME_LINK *pNames = NULL;
  NAME_DICT *pDict = NULL;
  
  /* Check parameters */
  if ((pIn == NULL) || (pResult == NULL)) {
    abort();
  }
  if (!snsource_ismulti(pIn)) {
    abort();
  }
  if ((samp_rate != RATE_CD) && (samp_rate != RATE_DVD)) {
    abort();
  }
  
  /* Clear results */
  memset(pResult, 0, sizeof(GENMAP_RESULT));
  
  pResult->errcode = GENMAP_OK;
  pResult->linenum = 0;
  pResult->pRoot = NULL;
  pResult->icount = 0;
  
  /* Gather all the names of variables and constants in a linked list */
  if (status) {
    pNames = gather_names(
              pIn, &(pResult->errcode), &(pResult->linenum));
    if (pResult->errcode != GENMAP_OK) {
      status = 0;
    }
  }
  
  /* Convert the linked list into a name dictionary, freeing the linked
   * list in the process */
  if (status) {
    pDict = make_dict(pNames);
    pNames = NULL;
    if (pDict == NULL) {
      status = 0;
      pResult->errcode = GENMAP_ERR_DUPNAME;
      pResult->linenum = 0;
    }
  }
  
  /* @@TODO: */
  
  /* If line number is LONG_MAX or negative, set to zero */
  if ((pResult->linenum == LONG_MAX) || (pResult->linenum < 0)) {
    pResult->linenum = 0;
  }
  
  /* Free name chain if allocated */
  free_names(pNames);
  pNames = NULL;
  
  /* Free dictionary if allocated */
  free_dict(pDict);
  pDict = NULL;
  
  /* @@TODO: test network below */
  if (status) {
    ADSR_OBJ *adsr = NULL;
    adsr = adsr_alloc(
              25.0,   /* Attack duration in milliseconds */
              0.0,    /* Decay duration in milliseconds */
              1.0,    /* Sustain level multiplier */
              250.0,  /* Release duration in milliseconds */
              samp_rate);
    memset(pResult, 0, sizeof(GENMAP_RESULT));
    pResult->errcode = GENMAP_OK;
    pResult->pRoot = generator_op(
                GENERATOR_F_SINE,   /* function */
                1.0,                /* frequency multiplier */
                0.0,                /* frequency boost */
                20000.0,            /* base amplitude */
                adsr,               /* ADSR envelope */
                0.0,                /* FM feedback */
                0.0,                /* AM feedback */
                NULL,               /* FM modulator */
                NULL,               /* AM modulator */
                0.0,                /* FM modulator scale */
                0.0,                /* AM modulator scale */
                samp_rate,
                20000,              /* ny_limit */
                0,                  /* hlimit */
                0);                 /* instance data offset */
    pResult->icount = 1;
  }
}

/*
 * genmap_errstr function.
 */
const char *genmap_errstr(int code) {
  
  const char *pResult = NULL;
  
  /* Handle error code */
  if (code < 0) {
    /* Shastina code, so pass through to Shastina */
    pResult = snerror_str(code);
    
  } else {
    /* Genmap error code */
    switch (code) {
      
      case GENMAP_OK:
        pResult = "No error";
        break;
      
      case GENMAP_ERR_DUPNAME:
        pResult = "Duplicate definition of variable or constant name";
        break;
      
      default:
        pResult = "Unknown error";
    }
  }
  
  /* Return result */
  return pResult;
}
