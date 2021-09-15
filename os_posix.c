/*
 * os_posix.c
 * ==========
 * 
 * POSIX implementation of the platform-specific os.h module.
 * 
 * See the header for further information.
 * 
 * This module is appropriate for UNIX and UNIX-like operating systems.
 * Just add os_posix.c as one of the modules during compilation and you
 * should be good to go.
 */

#include "os.h"
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * os_getsep function.
 */
int os_getsep(void) {
  return '/';
}

/*
 * os_issep function.
 */
int os_issep(int c) {
  if (c == '/') {
    return 1;
  } else {
    return 0;
  }
}

/*
 * os_isdir function.
 */
int os_isdir(const char *pc) {
  
  int status = 1;
  struct stat st;
  size_t slen = 0;
  
  /* Initialize structures */
  memset(&st, 0, sizeof(struct stat));
  
  /* Check parameters */
  if (pc == NULL) {
    abort();
  }
  
  /* Check for trailing separator */
  slen = strlen(pc);
  if (slen > 0) {
    if (os_issep(pc[slen - 1])) {
      abort();
    }
  }
  
  /* Query the path */
  if (stat(pc, &st)) {
    status = 0;
  }
  
  /* Check whether a directory */
  if (status) {
    if (S_ISDIR(st.st_mode)) {
      status = 1;
    } else {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * os_isfile function.
 */
int os_isfile(const char *pc) {
  
  int status = 1;
  struct stat st;
  
  /* Initialize structures */
  memset(&st, 0, sizeof(struct stat));
  
  /* Check parameters */
  if (pc == NULL) {
    abort();
  }
  
  /* Query the path */
  if (stat(pc, &st)) {
    status = 0;
  }
  
  /* Check whether a regular file */
  if (status) {
    if (S_ISREG(st.st_mode)) {
      status = 1;
    } else {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * os_gethome function.
 */
char *os_gethome(void) {
  
  char *pval = NULL;
  char *pcopy = NULL;
  size_t slen = 0;
  int32_t i = 0;
  
  /* Query the HOME environment variable */
  pval = getenv("HOME");
  if (pval != NULL) {
    /* We found the variable, so make a copy */
    slen = strlen(pval);
    pcopy = (char *) malloc(slen + 1);
    if (pcopy == NULL) {
      abort();
    }
    strcpy(pcopy, pval);
    
    /* If the copy is not empty, then blank out any separator characters
     * at the end of the string */
    for(i = ((int32_t) slen) - 1; i >= 0; i--) {
      if (os_issep(pcopy[i])) {
        pcopy[i] = (char) 0;
      } else {
        break;
      }
    }
  }
  
  /* Return result or NULL */
  return pcopy;
}
