/*
 * retro.c
 * =======
 * 
 * Main program module of the Retro synthesizer.
 * 
 * Syntax
 * ------
 * 
 *   retro [output]
 * 
 * [output] is the path to the output WAV file to write.  If it already
 * exists, it will be overwritten.
 * 
 * Operation
 * ---------
 * 
 * A Shastina file in the %retro-synth; format is read from standard
 * input.  The file is completely interpreted in order to program the
 * synthesizer.  The synthesizer then synthesizes the output audio in
 * accordance to the state of the synthesizer at the end of the Shastina
 * script execution.
 * 
 * Compilation
 * -----------
 * 
 * @@TODO:
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shastina.h"

/*
 * Error codes
 * ===========
 * 
 * Remember to update error_string!
 */

#define ERR_OK      (0)   /* No error */
#define ERR_ENTITY  (1)   /* Unsupported Shastina entity type */
#define ERR_METAMID (2)   /* Metacommand after header */
#define ERR_NORATE  (3)   /* Sampling rate not defined in header */
#define ERR_NOAMP   (4)   /* Square wave amplitude not in header */
#define ERR_NOSIG   (5)   /* Missing file type signature */
#define ERR_BADMETA (6)   /* Unknown metacommand */
#define ERR_MPARAMC (7)   /* Too many metacommand parameters */
#define ERR_METAINT (8)   /* Can't parse metacommand integer */
#define ERR_METAPRM (9)   /* Wrong number of metacommand parameters */
#define ERR_EMPTYMT (10)  /* Empty metacommand */
#define ERR_METAMUL (11)  /* Metacommand used multiple times */
#define ERR_BADRATE (12)  /* Invalid sampling rate */
#define ERR_BADAMP  (13)  /* Invalid square wave amplitude */
#define ERR_BADFRM  (14)  /* Invalid frame definition */
#define ERR_EMPTY   (15)  /* Nothing after header */
#define ERR_NUM     (16)  /* Can't parse numeric entity */
#define ERR_OVERFLW (17)  /* Stack overflow */
#define ERR_GROUP   (18)  /* Group closed improperly */

#define ERR_SN_MIN  (500) /* Mininum error code used for Shastina */
#define ERR_SN_MAX  (600) /* Maximum error code used for Shastina */

/*
 * Constants
 * =========
 */

/*
 * Maximum number of metacommand parameters.
 */
#define META_MAXPARAM (8)

/*
 * Metacommand codes.
 */
#define METACMD_NONE        (0)   /* No metacommand recorded yet */
#define METACMD_SIGNATURE   (1)   /* File signature "retro-synth" */
#define METACMD_RATE        (2)   /* Sampling rate "rate" */
#define METACMD_SQAMP       (3)   /* Square wave amplitude "sqamp" */
#define METACMD_NOSTEREO    (4)   /* No stereo "nostereo" */
#define METACMD_FRAME       (5)   /* Frame definition "frame" */

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int op(const char *pk, int *per);

static int begin_group(void);
static int end_group(void);
static int push_num(int32_t val);
static void header_config(
    int32_t rate,
    int32_t sqamp,
    int     nostereo,
    int32_t frame_before,
    int32_t frame_after);

static int parseInt(const char *pstr, int32_t *pv);
static int retro(FILE *pIn, const char *pOutPath, int *per, long *pln);
static const char *error_string(int code);

/* @@TODO: */
static int op(const char *pk, int *per) {
  printf("op %s\n", pk);
  return 1;
}
static int begin_group(void) {
  printf("begin group\n");
  return 1;
}
static int end_group(void) {
  printf("end group\n");
  return 1;
}
static int push_num(int32_t val) {
  printf("num %d\n", (int) val);
  return 1;
}

/*
 * Record basic configuration information from the header.
 * 
 * This must only be called once during the process.
 * 
 * Parameters:
 * 
 *   rate - the sampling rate (48000 or 44100)
 * 
 *   sqamp - the square wave amplitude
 * 
 *   nostereo - non-zero for no-stereo mode
 * 
 *   frame_before - the number of blank samples before
 * 
 *   frame_after - the number of blank samples after
 */
static void header_config(
    int32_t rate,
    int32_t sqamp,
    int     nostereo,
    int32_t frame_before,
    int32_t frame_after) {
  /* @@TODO: */
  printf("header_config %d %d %d %d %d\n",
          (int) rate,
          (int) sqamp,
          nostereo,
          (int) frame_before,
          (int) frame_after);
}

/*
 * Parse the given string as a signed integer.
 * 
 * pstr is the string to parse.
 * 
 * pv points to the integer value to use to return the parsed numeric
 * value if the function is successful.
 * 
 * In two's complement, this function will not successfully parse the
 * least negative value.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 *   pv - pointer to the return numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int parseInt(const char *pstr, int32_t *pv) {
  
  int negflag = 0;
  int32_t result = 0;
  int status = 1;
  int32_t d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* If first character is a sign character, set negflag appropriately
   * and skip it */
  if (*pstr == '+') {
    negflag = 0;
    pstr++;
  } else if (*pstr == '-') {
    negflag = 1;
    pstr++;
  } else {
    negflag = 0;
  }
  
  /* Make sure we have at least one digit */
  if (*pstr == 0) {
    status = 0;
  }
  
  /* Parse all digits */
  if (status) {
    for( ; *pstr != 0; pstr++) {
    
      /* Make sure in range of digits */
      if ((*pstr < '0') || (*pstr > '9')) {
        status = 0;
      }
    
      /* Get numeric value of digit */
      if (status) {
        d = (int32_t) (*pstr - '0');
      }
      
      /* Multiply result by 10, watching for overflow */
      if (status) {
        if (result <= INT32_MAX / 10) {
          result = result * 10;
        } else {
          status = 0; /* overflow */
        }
      }
      
      /* Add in digit value, watching for overflow */
      if (status) {
        if (result <= INT32_MAX - d) {
          result = result + d;
        } else {
          status = 0; /* overflow */
        }
      }
    
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Invert result if negative mode */
  if (status && negflag) {
    result = -(result);
  }
  
  /* Write result if successful */
  if (status) {
    *pv = result;
  }
  
  /* Return status */
  return status;
}

/*
 * Run the Retro synthesizer on the given input file and generate the
 * output file.
 * 
 * Undefined behavior occurs if this function is called more than once
 * during a process.
 * 
 * pIn is the Shastina file to read to program the synthesizer.  It must
 * be open for reading.  Reading is fully sequential.
 * 
 * pOutPath is the path to the output WAV file to create.  If a WAV file
 * already exists at that location, it will be overwritten.
 * 
 * per points to the variable to receive the error status.  If the
 * status is not required, it may be NULL.  Use error_string() to get an
 * error string for the error code.
 * 
 * pln points to the variable to receive the line number, if there is a
 * line number in the input file associated with the error.  It may be
 * NULL if not required.  -1 or LONG_MAX is returned if there is no line
 * number associated with the error.
 * 
 * Parameters:
 * 
 *   pIn - the input Shastina file to program the synthesizer
 * 
 *   pOutPath - the output WAV file path
 * 
 *   per - pointer to the error status variable, or NULL
 * 
 *   pln - pointer to line number status variable, or NULL
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int retro(FILE *pIn, const char *pOutPath, int *per, long *pln) {
  
  int status = 1;
  int dummy = 0;
  long dummy_l = 0;
  SNPARSER *pp = NULL;
  SNENTITY ent;
  
  int header_done = 0;
  int sig_read = 0;
  int32_t rate = -1;
  int32_t sqamp = -1;
  int nostereo = 0;
  int32_t frame_before = -1;
  int32_t frame_after = -1;
  
  int32_t meta_param[META_MAXPARAM];
  int meta_count = 0;
  int meta_cmd = METACMD_NONE;
  
  int32_t v = 0;
  
  /* Initialize structures and arrays */
  memset(&ent, 0, sizeof(SNENTITY));
  memset(meta_param, 0, sizeof(int32_t) * META_MAXPARAM);
  
  /* Check parameters */
  if ((pIn == NULL) || (pOutPath == NULL)) {
    abort();
  }
  
  /* Redirect errors to dummy if not defined */
  if (per == NULL) {
    per = &dummy;
  }
  if (pln == NULL) {
    pln = &dummy_l;
  }
  
  /* Reset error status */
  *per = ERR_OK;
  *pln = -1;
  
  /* Allocate a Shastina parser */
  pp = snparser_alloc();
  
  /* Read Shastina entities until EOF entity or error */
  for(snparser_read(pp, &ent, pIn);
      (ent.status >= 0) && (ent.status != SNENTITY_EOF);
      snparser_read(pp, &ent, pIn)) {
    
    /* First of all, fail if an unsupported entity type */
    if ((ent.status != SNENTITY_BEGIN_META) &&
        (ent.status != SNENTITY_END_META) &&
        (ent.status != SNENTITY_META_TOKEN) &&
        (ent.status != SNENTITY_NUMERIC) &&
        (ent.status != SNENTITY_BEGIN_GROUP) &&
        (ent.status != SNENTITY_END_GROUP) &&
        (ent.status != SNENTITY_ARRAY) &&
        (ent.status != SNENTITY_OPERATION)) {
      status = 0;
      *per = ERR_ENTITY;
      *pln = snparser_count(pp);
    }
    
    /* If header is done, make sure not a meta type */
    if (status) {
      if (header_done) {
        if ((ent.status == SNENTITY_BEGIN_META) ||
            (ent.status == SNENTITY_END_META) ||
            (ent.status == SNENTITY_META_TOKEN)) {
          status = 0;
          *per = ERR_METAMID;
          *pln = snparser_count(pp);
        }
      }
    }
    
    /* If header is not done and the entity is not a meta type, then
     * make sure we got the required header information, fill in any
     * necessary defaults, and set the header_done flag, and report the
     * header information */
    if (status && (!header_done)) {
      if ((ent.status != SNENTITY_BEGIN_META) &&
          (ent.status != SNENTITY_END_META) &&
          (ent.status != SNENTITY_META_TOKEN)) {
        /* Header finishing -- make sure we got sig, rate, and sqamp */
        if (!sig_read) {
          status = 0;
          *per = ERR_NOSIG;
          *pln = snparser_count(pp);
        }
        if (status && (rate < 0)) {
          status = 0;
          *per = ERR_NORATE;
          *pln = snparser_count(pp);
        }
        if (status && (sqamp < 0)) {
          status = 0;
          *per = ERR_NOAMP;
          *pln = snparser_count(pp);
        }
        
        /* Set defaults that weren't set */
        if (status && (frame_before < 0)) {
          frame_before = rate;  /* one second */
        }
        if (status && (frame_after < 0)) {
          frame_after = rate;   /* one second */
        }
        
        /* Set the header_done flag */
        if (status) {
          header_done = 1;
        }
        
        /* Report header information */
        if (status) {
          header_config(
            rate, sqamp, nostereo, frame_before, frame_after);
        }
      }
    }
    
    /* Different handling depending whether in header mode or not */
    if (status && header_done) {
      /* Header is complete, parsing main -- handle types */
      if (ent.status == SNENTITY_NUMERIC) {
        /* Numeric entity */
        if (parseInt(ent.pKey, &v)) {
          if (!push_num(v)) {
            status = 0;
            *per = ERR_OVERFLW;
            *pln = snparser_count(pp);
          }
          
        } else {
          status = 0;
          *per = ERR_NUM;
          *pln = snparser_count(pp);
        }
        
      } else if (ent.status == SNENTITY_BEGIN_GROUP) {
        /* Begin group */
        if (!begin_group()) {
          status = 0;
          *per = ERR_SN_MAX + SNERR_DEEPGROUP;
          *pln = snparser_count(pp);
        }
      
      } else if (ent.status == SNENTITY_END_GROUP) {
        /* End group */
        if (!end_group()) {
          status = 0;
          *per = ERR_GROUP;
          *pln = snparser_count(pp);
        }
        
      } else if (ent.status == SNENTITY_ARRAY) {
        /* Array entity */
        if (ent.count <= INT32_MAX) {
          if (!push_num((int32_t) ent.count)) {
            status = 0;
            *per = ERR_OVERFLW;
            *pln = snparser_count(pp);
          }
          
        } else {
          status = 0;
          *per = ERR_SN_MAX + SNERR_LONGARRAY;
          *pln = snparser_count(pp);
        }
        
      } else if (ent.status == SNENTITY_OPERATION) {
        /* Operation entity */
        if (!op(ent.pKey, per)) {
          status = 0;
          *pln = snparser_count(pp);
        }
        
      } else {
        /* Unrecognized entity type -- shouldn't happen */
        abort();
      }
      
    } else if (status) {
      /* Parsing header -- check type */
      if (ent.status == SNENTITY_BEGIN_META) {
        /* Beginning a metacommand -- reset state */
        meta_count = 0;
        meta_cmd = METACMD_NONE;
        
      } else if (ent.status == SNENTITY_META_TOKEN) {
        /* Meta token -- if we haven't recorded the command type yet,
         * parse as the meta command; else, parse as numeric param */
        if (meta_cmd == METACMD_NONE) {
          /* Don't have command yet, so parse token as command */
          if (strcmp(ent.pKey, "retro-synth") == 0) {
            meta_cmd = METACMD_SIGNATURE;
          
          } else if (strcmp(ent.pKey, "rate") == 0) {
            meta_cmd = METACMD_RATE;
          
          } else if (strcmp(ent.pKey, "sqamp") == 0) {
            meta_cmd = METACMD_SQAMP;
          
          } else if (strcmp(ent.pKey, "nostereo") == 0) {
            meta_cmd = METACMD_NOSTEREO;
          
          } else if (strcmp(ent.pKey, "frame") == 0) {
            meta_cmd = METACMD_FRAME;
          
          } else {
            /* Unrecognized metacommand */
            status = 0;
            *per = ERR_BADMETA;
            *pln = snparser_count(pp);
          }
          
        } else {
          /* We already have the command, so we are adding a param --
           * check that not too many parameters */
          if (meta_count >= META_MAXPARAM) {
            status = 0;
            *per = ERR_MPARAMC;
            *pln = snparser_count(pp);
          }
          
          /* Parse the parameter as a signed integer */
          if (status) {
            if (!parseInt(ent.pKey, &(meta_param[meta_count]))) {
              status = 0;
              *per = ERR_METAINT;
              *pln = snparser_count(pp);
            }
          }
          
          /* Increase the metacommand parameter count */
          if (status) {
            meta_count++;
          }
        }
        
      } else if (ent.status == SNENTITY_END_META) {
        /* Ending a metacommand -- interpret it, first checking that
         * metacommand has correct number of parameters */
        if (meta_cmd == METACMD_SIGNATURE) {
          if (meta_count != 0) {
            status = 0;
            *per = ERR_METAPRM;
            *pln = snparser_count(pp);
          }
          
        } else if (meta_cmd == METACMD_RATE) {
          if (meta_count != 1) {
            status = 0;
            *per = ERR_METAPRM;
            *pln = snparser_count(pp);
          }
          
        } else if (meta_cmd == METACMD_SQAMP) {
          if (meta_count != 1) {
            status = 0;
            *per = ERR_METAPRM;
            *pln = snparser_count(pp);
          }
          
        } else if (meta_cmd == METACMD_NOSTEREO) {
          if (meta_count != 0) {
            status = 0;
            *per = ERR_METAPRM;
            *pln = snparser_count(pp);
          }
          
        } else if (meta_cmd == METACMD_FRAME) {
          if (meta_count != 2) {
            status = 0;
            *per = ERR_METAPRM;
            *pln = snparser_count(pp);
          }
          
        } else if (meta_cmd == METACMD_NONE) {
          /* Metacommand had no tokens */
          status = 0;
          *per = ERR_EMPTYMT;
          *pln = snparser_count(pp);
          
        } else {
          /* Unrecognized metacommand -- shouldn't happen */
          abort();
        }
        
        /* If we haven't read the signature yet, metacommand must be the
         * signature */
        if (status && (!sig_read)) {
          if (meta_cmd != METACMD_SIGNATURE) {
            status = 0;
            *per = ERR_NOSIG;
            *pln = snparser_count(pp);
          }
        }
        
        /* Interpret specific metacommand */
        if (status && (meta_cmd == METACMD_SIGNATURE)) {
          /* Signature -- set sig_read flag, error if already set */
          if (!sig_read) {
            sig_read = 1;
          } else {
            status = 0;
            *per = ERR_METAMUL;
            *pln = snparser_count(pp);
          }
          
        } else if (status && (meta_cmd == METACMD_RATE)) {
          /* Rate -- set rate, error if invalid value or already set */
          if (rate < 0) {
            if ((meta_param[0] == 48000) || (meta_param[0] == 44100)) {
              rate = meta_param[0];
            } else {
              status = 0;
              *per = ERR_BADRATE;
              *pln = snparser_count(pp);
            }
            
          } else {
            status = 0;
            *per = ERR_METAMUL;
            *pln = snparser_count(pp);
          }
          
        } else if (status && (meta_cmd == METACMD_SQAMP)) {
          /* Square wave amplitude, error if invalid value or set */
          if (sqamp < 0) {
            if (meta_param[0] > 0) {
              sqamp = meta_param[0];
            } else {
              status = 0;
              *per = ERR_BADAMP;
              *pln = snparser_count(pp);
            }
            
          } else {
            status = 0;
            *per = ERR_METAMUL;
            *pln = snparser_count(pp);
          }
          
        } else if (status && (meta_cmd == METACMD_NOSTEREO)) {
          /* No-stereo flag, set flag */
          nostereo = 1;
          
        } else if (status && (meta_cmd == METACMD_FRAME)) {
          /* Frame command, error if invalid value or set already */
          if (frame_before < 0) {
            if ((meta_param[0] >= 0) && (meta_param[1] >= 0)) {
              frame_before = meta_param[0];
              frame_after = meta_param[1];
            } else {
              status = 0;
              *per = ERR_BADFRM;
              *pln = snparser_count(pp);
            }
            
          } else {
            status = 0;
            *per = ERR_METAMUL;
            *pln = snparser_count(pp);
          }
          
        } else if (status) {
          /* Unrecognized metacommand -- shouldn't happen */
          abort();
        }
        
      } else {
        /* Non-meta type; shouldn't happen */
        abort();
      }
    }
  }
  
  /* If we left loop on account of a Shastina error, record it */
  if (status && (ent.status < 0)) {
    status = 0;
    *per = ERR_SN_MAX + ent.status;
    *pln = snparser_count(pp);
  }
  
  /* If there was nothing after the header, empty error */
  if (status && (!header_done)) {
    status = 0;
    *per = ERR_EMPTY;
    *pln = snparser_count(pp);
  }
  
  /* @@TODO: empty stack check */
  /* @@TODO: synthesize */
  
  /* Free parser if allocated */
  snparser_free(pp);
  pp = NULL;
  
  /* Return status */
  return status;
}

/*
 * Convert an error code return into a string.
 * 
 * The string has the first letter capitalized, but no punctuation or
 * line break at the end.
 * 
 * If the code is ERR_OK, then "No error" is returned.  If the code is
 * unrecognized, then "Unknown error" is returned.
 * 
 * Parameters:
 * 
 *   code - the error code
 * 
 * Return:
 * 
 *   an error message
 */
static const char *error_string(int code) {
  
  const char *pResult = NULL;
  
  switch (code) {
    
    case ERR_OK:
      pResult = "No error";
      break;
    
    case ERR_ENTITY:
      pResult = "Unsupported Shastina entity type";
      break;
    
    case ERR_METAMID:
      pResult = "Metacommand after header";
      break;
    
    case ERR_NORATE:
      pResult = "Sampling rate not defined in header";
      break;
    
    case ERR_NOAMP:
      pResult = "Square wave amplitude not defined in header";
      break;
      
    case ERR_NOSIG:
      pResult = "Missing file type signature on input";
      break;
    
    case ERR_BADMETA:
      pResult = "Metacommand not recognized";
      break;
    
    case ERR_MPARAMC:
      pResult = "Too many metacommand parameters";
      break;
    
    case ERR_METAINT:
      pResult = "Can't parse metacommand parameter as integer";
      break;
    
    case ERR_METAPRM:
      pResult = "Wrong number of parameters for metacommand";
      break;
    
    case ERR_EMPTYMT:
      pResult = "Empty metacommand";
      break;
    
    case ERR_METAMUL:
      pResult = "Metacommand used multiple times";
      break;
    
    case ERR_BADRATE:
      pResult = "Invalid sampling rate";
      break;
      
    case ERR_BADAMP:
      pResult = "Invalid square wave amplitude";
      break;
    
    case ERR_BADFRM:
      pResult = "Invalid frame definition";
      break;
    
    case ERR_EMPTY:
      pResult = "Nothing in file after header";
      break;
    
    case ERR_NUM:
      pResult = "Can't parse numeric entity";
      break;
    
    case ERR_OVERFLW:
      pResult = "Stack overflow";
      break;
    
    case ERR_GROUP:
      pResult = "Group closed improperly";
      break;
    
    case (ERR_SN_MAX+SNERR_IOERR):
      pResult = "I/O error";
      break;
    
    case (ERR_SN_MAX+SNERR_EOF):
      pResult = "Unexpected end of file";
      break;
    
    case (ERR_SN_MAX+SNERR_BADSIG):
      pResult = "Unrecognized file signature";
      break;
    
    case (ERR_SN_MAX+SNERR_OPENSTR):
      pResult = "File ends in middle of string";
      break;
    
    case (ERR_SN_MAX+SNERR_LONGSTR):
      pResult = "String is too long";
      break;
    
    case (ERR_SN_MAX+SNERR_NULLCHR):
      pResult = "Nul character encountered in string";
      break;
    
    case (ERR_SN_MAX+SNERR_DEEPCURLY):
      pResult = "Too much curly nesting in string";
      break;
    
    case (ERR_SN_MAX+SNERR_BADCHAR):
      pResult = "Illegal character encountered";
      break;
    
    case (ERR_SN_MAX+SNERR_LONGTOKEN):
      pResult = "Token is too long";
      break;
      
    case (ERR_SN_MAX+SNERR_TRAILER):
      pResult = "Content present after |; token";
      break;
    
    case (ERR_SN_MAX+SNERR_DEEPARRAY):
      pResult = "Too much array nesting";
      break;
    
    case (ERR_SN_MAX+SNERR_METANEST):
      pResult = "Nested metacommands";
      break;
    
    case (ERR_SN_MAX+SNERR_SEMICOLON):
      pResult = "Semicolon used outside of metacommand";
      break;
    
    case (ERR_SN_MAX+SNERR_DEEPGROUP):
      pResult = "Too much group nesting";
      break;
    
    case (ERR_SN_MAX+SNERR_RPAREN):
      pResult = "Right parenthesis outside of group";
      break;
    
    case (ERR_SN_MAX+SNERR_RSQR):
      pResult = "Right square bracket outside array";
      break;
    
    case (ERR_SN_MAX+SNERR_OPENGROUP):
      pResult = "Open group";
      break;

    case (ERR_SN_MAX+SNERR_LONGARRAY):
      pResult = "Array has too many elements";
      break;
    
    case (ERR_SN_MAX+SNERR_METAEMBED):
      pResult = "Embedded data in metacommand";
      break;
    
    case (ERR_SN_MAX+SNERR_OPENMETA):
      pResult = "Unclosed metacommand";
      break;
    
    case (ERR_SN_MAX+SNERR_OPENARRAY):
      pResult = "Unclosed array";
      break;
    
    case (ERR_SN_MAX+SNERR_COMMA):
      pResult = "Comma used outside of array or meta";
      break;
    
    default:
      pResult = "Unknown error";
  }
  
  return pResult;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  const char *pModule = NULL;
  int status = 1;
  int errnum = 0;
  long errline = 0;
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "retro";
  }
  
  /* Check argument count */
  if (status) {
    if (argc != 2) {
      status = 0;
      fprintf(stderr, "%s: Expecting one argument!\n", pModule);
    }
  }
  
  /* Verify argument is present */
  if (status) {
    if (argv == NULL) {
      abort();
    }
    if (argv[0] == NULL) {
      abort();
    }
    if (argv[1] == NULL) {
      abort();
    }
  }
  
  /* Call through */
  if (status) {
    if (!retro(stdin, argv[1], &errnum, &errline)) {
      if ((errline >= 0) && (errline < LONG_MAX)) {
        /* Line number to report */
        status = 0;
        fprintf(stderr, "%s: [Line %ld] %s!\n",
                  pModule, errline, error_string(errnum));
        
      } else {
        /* No line number to report */
        status = 0;
        fprintf(stderr, "%s: %s!\n", pModule, error_string(errnum));
      }
    }
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
