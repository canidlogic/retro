/*
 * diagnostic.c
 * ============
 * 
 * Implementation of diagnostic.h
 * 
 * See the header for further information.
 */

#include "diagnostic.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Diagnostics
 * ===========
 */

static void raiseErr(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(1, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

static void sayWarn(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(0, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

/*
 * Local data
 * ==========
 */

/*
 * The executable module name for use in diagnostic messages, or NULL
 * if not known.
 */
static const char *pModule = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * diagnostic_set_module function.
 */
void diagnostic_set_module(const char *pName) {
  pModule = pName;
}

/*
 * diagnostic_global function.
 */
void diagnostic_global(
          int       err,
    const char    * pSource,
          int       lnum,
    const char    * pDetail,
          va_list   ap) {
  
  /* Report module name if known */
  if (pModule != NULL) {
    fprintf(stderr, "%s: ", pModule);
  }
  
  /* Report message class */
  if (err) {
    fprintf(stderr, "[Error");
  } else {
    fprintf(stderr, "[Warning");
  }
  
  /* Report source file if known */
  if (pSource != NULL) {
    fprintf(stderr, " in \"%s\"", pSource);
  }
  
  /* Report line number if known */
  if (lnum >= 0) {
    fprintf(stderr, " at line %d", lnum);
  }
  
  /* Finish message class and then add space if there is a detail
   * message */
  if (pDetail != NULL) {
    fprintf(stderr, "] ");
  } else {
    fprintf(stderr, "]");
  }
  
  /* Print detail message if given */
  if (pDetail != NULL) {
    vfprintf(stderr, pDetail, ap);
  }
  
  /* Print line break */
  fprintf(stderr, "\n");
  
  /* If an error, stop the program */
  if (err) {
    exit(EXIT_FAILURE);
  }
}

/*
 * diagnostic_startup function.
 */
void diagnostic_startup(int argc, char *argv[], const char *pDefault) {
  
  const char *pName = NULL;
  int i = 0;
  
  /* Determine module name */
  if ((argc > 0) && (argv != NULL)) {
    pName = argv[0];
  }
  if (pName == NULL) {
    pName = pDefault;
  }
  
  /* Register module name */
  diagnostic_set_module(pName);
  
  /* Check argc */
  if (argc < 0) {
    raiseErr(__LINE__, "Invalid argc parameter for main()");
  }
  
  /* If argc is greater than zero, check argv */
  if (argc > 0) {
    /* Check that argv is non-NULL */
    if (argv == NULL) {
      raiseErr(__LINE__, "Invalid argv parameter for main()");
    }
    
    /* Check that parameters are non-NULL */
    for(i = 0; i < argc; i++) {
      if (argv[i] == NULL) {
        raiseErr(__LINE__, "Null argv argument for main()");
      }
    }
    
    /* Check that after parameters is a NULL pointer */
    if (argv[argc] != NULL) {
      raiseErr(__LINE__, "Missing termination for argv in main()");
    }
  }
}

/*
 * diagnostic_log function.
 * 
 * NOTE:  diagnostic_vlog is a slightly modified copy of this function.
 */
void diagnostic_log(const char *pFormat, ...) {
  
  va_list ap;
  
  /* Only proceed if non-NULL passed */
  if (pFormat != NULL) {
    
    /* Report module name if known */
    if (pModule != NULL) {
      fprintf(stderr, "%s: ", pModule);
    }
    
    /* Print log message */
    va_start(ap, pFormat);
    vfprintf(stderr, pFormat, ap);
    va_end(ap);
    
    /* Print line break */
    fprintf(stderr, "\n");
  }
}

/*
 * diagnostic_vlog function.
 * 
 * NOTE:  slightly modified copy of diagnostic_log.
 */
void diagnostic_vlog(const char *pFormat, va_list ap) {
  /* Only proceed if non-NULL passed */
  if (pFormat != NULL) {
    
    /* Report module name if known */
    if (pModule != NULL) {
      fprintf(stderr, "%s: ", pModule);
    }
    
    /* Print log message */
    vfprintf(stderr, pFormat, ap);
    
    /* Print line break */
    fprintf(stderr, "\n");
  }
}
