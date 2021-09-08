/*
 * genmap.c
 * ========
 * 
 * Implementation of genmap.h
 * 
 * See the header for further information.
 */

#include "genmap.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

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
 * Local functions
 * ---------------
 */

/* Prototypes */
static void free_names(NAME_LINK *pNames);
static NAME_LINK *gather_names(SNSOURCE *pIn, int *perr, long *pline);

static int name_sort(const void *pa, const void *pb);
static NAME_DICT *make_dict(NAME_LINK *pNames);
static void free_dict(NAME_DICT *pd);

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
  
  /* @@TODO: test for printing all names below */
  if (status) {
    int32_t i = 0;
    for(i = 0; i < pDict->name_count; i++) {
      fprintf(stderr, "%s\n", (pDict->ppNames)[i]);
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
