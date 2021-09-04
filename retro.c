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
#define ERR_BADOP   (19)  /* Unrecognized operation */
#define ERR_OPPARAM (20)  /* Operation doesn't have enough parameters */
#define ERR_PARAMT  (21)  /* Wrong parameter type */
#define ERR_LAYERC  (22)  /* Invalid layer count */
#define ERR_BADT    (23)  /* t value is negative */
#define ERR_BADFRAC (24)  /* Invalid fraction value */
#define ERR_REMAIN  (25)  /* Elements remain on stack at end */

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
 * The maximum integer value used for representing fractions.
 */
#define MAX_FRAC (1024)

/*
 * The maximum number of entries on the interpreter stack.
 */
#define MAX_STACK (4096)

/*
 * The maximum number of nested groups.
 */
#define MAX_GROUP (1024)

/*
 * Opcodes.
 */
#define OPCODE_NONE   (0)   /* invalid opcode */
#define OPCODE_LC     (1)   /* lc */
#define OPCODE_LR     (2)   /* lr */
#define OPCODE_LAYER  (3)   /* layer */
#define OPCODE_DERIVE (4)   /* derive_layer */
#define OPCODE_INSTR  (5)   /* instr */
#define OPCODE_IDUP   (6)   /* instr_dup */
#define OPCODE_MAXMIN (7)   /* instr_maxmin */
#define OPCODE_FIELD  (8)   /* instr_field */
#define OPCODE_STEREO (9)   /* instr_stereo */
#define OPCODE_NOTE   (10)  /* n */

/*
 * Parameter types.
 */
#define PTYPE_INT (0)   /* Integer/numeric */
#define PTYPE_LC  (1)   /* lc constant graph node */
#define PTYPE_LR  (2)   /* lr ramp graph node */

/*
 * Type declarations
 * =================
 */

/*
 * The structure used on the interpreter stack.
 */
typedef struct {
  
  /*
   * For numeric entries, this is the integer value.
   * 
   * For graph nodes, this is the t offset.
   */
  int32_t val;
  
  /*
   * For numeric entries, this is set to -1.
   * 
   * For constant graph nodes, this is the value, in [0, MAX_FRAC].
   * 
   * For ramp graph nodes, this is the beginning value of the ramp, in
   * range [0, MAX_FRAC].
   */
  int16_t ra;
  
  /*
   * For numeric entries, this is set to -1.
   * 
   * For constant graph nodes, this is set to -1.
   * 
   * For ramp graph nodes, this is the end value of the ramp, in range
   * [0, MAX_FRAC].
   */
  int16_t rb;
  
} STACK_REC;

/*
 * Static data
 * ===========
 */

/*
 * Flag indicating whether module has been initialized with the
 * header_config() function.
 */
static int m_init = 0;

/*
 * The sampling rate, in hertz.
 * 
 * Only valid if m_init is non-zero.
 */
static int32_t m_rate;

/*
 * The amplitude of the square wave module.
 * 
 * Only valid if m_init is non-zero.
 */
static int32_t m_sqamp;

/*
 * Flag that is non-zero to have single-channel output.
 * 
 * Only valid if m_init is non-zero.
 */
static int m_nostereo;

/*
 * The number of samples of silence before synthesis starts.
 * 
 * Only valid if m_init is non-zero.
 */
static int32_t m_frame_before;

/*
 * The number of samples of silence after synthesis ends.
 * 
 * Only valid if m_init is non-zero.
 */
static int32_t m_frame_after;

/*
 * The number of groups open on the group stack.
 * 
 * Only valid if m_init is non-zero.
 */
static int32_t m_group_count;

/*
 * The group stack.
 * 
 * Only valid if m_init is non-zero.
 * 
 * m_group_count indicates how many entries are on this stack.
 * 
 * Each entry is a count of the number of elements that were on the full
 * stack when the group was opened.
 */
static int32_t m_group_stack[MAX_GROUP];

/*
 * The number of elements on the interpreter stack.
 * 
 * Only valid if m_init is non-zero.
 * 
 * This counts the number of elements on the full stack, regardless of
 * what groups might be open.
 */
static int32_t m_stack_count;

/*
 * The interpreter stack.
 * 
 * Only valid if m_init is non-zero.
 * 
 * m_stack_count indicates how many elements are used on the stack.
 */
static STACK_REC m_stack[MAX_STACK];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int op_lc(int32_t t, int32_t r, int *per, STACK_REC *psr);
static int op_lr(
    int32_t     t,
    int32_t     ra,
    int32_t     rb,
    int       * per,
    STACK_REC * psr);
static int op_layer(
          int32_t     lid,
          int32_t     m,
          int32_t     c,
    const STACK_REC * psa,
          int       * per);
static int op_derive(int32_t lid, int32_t src, int32_t m, int *per);
static int op_instr(
    int32_t   iid,
    int32_t   i_max,
    int32_t   i_min,
    int32_t   attack,
    int32_t   decay,
    int32_t   sustain,
    int32_t   release,
    int     * per);
static int op_idup(int32_t iid, int32_t src, int *per);
static int op_maxmin(
    int32_t   iid,
    int32_t   i_max,
    int32_t   i_min,
    int     * per);
static int op_field(
    int32_t   iid,
    int32_t   low_pos,
    int32_t   low_pitch,
    int32_t   high_pos,
    int32_t   high_pitch,
    int     * per);
static int op_stereo(int32_t iid, int32_t pos, int *per);
static int op_note(
    int32_t   t,
    int32_t   dur,
    int32_t   pitch,
    int32_t   iid,
    int32_t   lid,
    int     * per);

static int32_t stack_height(void);
static int stack_type(int32_t i);
static int32_t stack_int(int32_t i);
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

/* 
 * Implementation of "lc" operation.
 * 
 * psr points to a structure to be filled in with the record to be
 * pushed on the stack, if successful.
 * 
 * Parameters:
 * 
 *   t - the time offset
 * 
 *   r - the constant intensity for the graph element
 * 
 *   per - pointer to a variable to receive an error code
 * 
 *   psr - pointer to result variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if operation failed
 */
static int op_lc(int32_t t, int32_t r, int *per, STACK_REC *psr) {
  
  int status = 1;
  
  /* Check per and psr */
  if ((per == NULL) || (psr == NULL)) {
    abort();
  }
  
  /* Range-check parameters */
  if (t < 0) {
    status = 0;
    *per = ERR_BADT;
  }
  if (status) {
    if ((r < 0) || (r > MAX_FRAC)) {
      status = 0;
      *per = ERR_BADFRAC;
    }
  }
  
  /* Fill in result */
  if (status) {
    psr->val = t;
    psr->ra = (int16_t) r;
    psr->rb = -1;
  }
  
  /* Return status */
  return status;
}

/* 
 * Implementation of "lr" operation.
 * 
 * psr points to a structure to be filled in with the record to be
 * pushed on the stack, if successful.
 * 
 * If ra and rb are equal, this has the effect of an lc operation.
 * 
 * Parameters:
 * 
 *   t - the time offset
 * 
 *   ra - the beginning intensity for the graph element
 * 
 *   rb - the ending intensity for the graph element
 * 
 *   per - pointer to a variable to receive an error code
 * 
 *   psr - pointer to result variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if operation failed
 */
static int op_lr(
    int32_t     t,
    int32_t     ra,
    int32_t     rb,
    int       * per,
    STACK_REC * psr) {
  
  int status = 1;
  
  /* Check per and psr */
  if ((per == NULL) || (psr == NULL)) {
    abort();
  }
  
  /* Range-check parameters */
  if (t < 0) {
    status = 0;
    *per = ERR_BADT;
  }
  if (status) {
    if ((ra < 0) || (ra > MAX_FRAC)) {
      status = 0;
      *per = ERR_BADFRAC;
    }
  }
  if (status) {
    if ((rb < 0) || (rb > MAX_FRAC)) {
      status = 0;
      *per = ERR_BADFRAC;
    }
  }
  
  /* Fill in result */
  if (status) {
    if (ra != rb) {
      psr->val = t;
      psr->ra = (int16_t) ra;
      psr->rb = (int16_t) rb;
    } else {
      psr->val = t;
      psr->ra = (int16_t) ra;
      psr->rb = -1;
    }
  }
  
  /* Return status */
  return status;
}

/* @@TODO: */
static int op_layer(
          int32_t     lid,
          int32_t     m,
          int32_t     c,
    const STACK_REC * psa,
          int       * per) {
  printf("layer %d %d %d\n", (int) lid, (int) m, (int) c);
  return 1;
}
static int op_derive(int32_t lid, int32_t src, int32_t m, int *per) {
  printf("derive %d %d %d\n", (int) lid, (int) src, (int) m);
  return 1;
}
static int op_instr(
    int32_t   iid,
    int32_t   i_max,
    int32_t   i_min,
    int32_t   attack,
    int32_t   decay,
    int32_t   sustain,
    int32_t   release,
    int     * per) {
  printf("instr %d %d %d %d %d %d %d\n",
    (int) iid, (int) i_max, (int) i_min,
    (int) attack, (int) decay, (int) sustain,
    (int) release);
  return 1;
}
static int op_idup(int32_t iid, int32_t src, int *per) {
  printf("idup %d %d\n", (int) iid, (int) src);
  return 1;
}
static int op_maxmin(
    int32_t   iid,
    int32_t   i_max,
    int32_t   i_min,
    int     * per) {
  printf("maxmin %d %d %d\n", (int) iid, (int) i_max, (int) i_min);
  return 1;
}
static int op_field(
    int32_t   iid,
    int32_t   low_pos,
    int32_t   low_pitch,
    int32_t   high_pos,
    int32_t   high_pitch,
    int     * per) {
  printf("field %d %d %d %d %d\n",
    (int) iid, (int) low_pos, (int) low_pitch,
    (int) high_pos, (int) high_pitch);
  return 1;
}
static int op_stereo(int32_t iid, int32_t pos, int *per) {
  printf("stereo %d %d\n", (int) iid, (int) pos);
  return 1;
}
static int op_note(
    int32_t   t,
    int32_t   dur,
    int32_t   pitch,
    int32_t   iid,
    int32_t   lid,
    int     * per) {
  printf("note %d %d %d %d %d\n",
    (int) t, (int) dur, (int) pitch,
    (int) iid, (int) lid);
  return 1;
}

/*
 * Get the current height of the stack, taking into account any open
 * groups.
 * 
 * header_config() must be called before this function.
 * 
 * Return:
 * 
 *   the stack height, accounting for open groups
 */
static int32_t stack_height(void) {
  
  int32_t result = 0;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check if an open group */
  if (m_group_count > 0) {
    /* Open groups, so use stack count minus top of group stack */
    result = m_stack_count - m_group_stack[m_group_count - 1];
    
  } else {
    /* No open groups, so just use stack count */
    result = m_stack_count;
  }
  
  /* Return result */
  return result;
}

/*
 * Get the type of element on the stack at index i.
 * 
 * header_config() must be called before this function.
 * 
 * The return value is one of the PTYPE constants.
 * 
 * i must be in range [0, m_stack_count - 1].  This function ignores any
 * open groups.
 * 
 * Parameters:
 * 
 *   i - the index on the stack
 * 
 * Return:
 * 
 *   the type of element on the stack
 */
static int stack_type(int32_t i) {
  
  STACK_REC *psr = NULL;
  int pt = 0;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameter */
  if ((i < 0) || (i > m_stack_count - 1)) {
    abort();
  }
  
  /* Get pointer to record */
  psr = &(m_stack[i]);
  
  /* Determine type */
  if (psr->ra < 0) {
    pt = PTYPE_INT;
  
  } else if (psr->rb < 0) {
    pt = PTYPE_LC;
    
  } else {
    pt = PTYPE_LR;
  }
  
  /* Return result */
  return pt;
}

/*
 * Get the integer stack element value at stack index i.
 * 
 * header_config() must be called before this function.
 * 
 * i must be in range [0, m_stack_count - 1].  Open groups are ignored
 * by this function.
 * 
 * A fault occurs if the indicated record is not for an integer.
 * 
 * Parameters:
 * 
 *   i - the index of the stack element
 * 
 * Return:
 * 
 *   the integer value of the requested stack element
 */
static int32_t stack_int(int32_t i) {
  
  STACK_REC *psr = NULL;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameter */
  if ((i < 0) || (i > m_stack_count - 1)) {
    abort();
  }
  
  /* Get pointer to record */
  psr = &(m_stack[i]);
  
  /* Verify type */
  if (psr->ra >= 0) {
    abort();
  }
  
  /* Return result */
  return psr->val;
}

/*
 * Called to interpret an operation from the Shastina file.
 * 
 * header_config() must be called before this function.
 * 
 * per points to the variable to receive the error code in case of
 * error.  It may not be NULL.
 * 
 * Parameters:
 * 
 *   pk - pointer to the opname token
 * 
 *   per - pointer to the error code variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if operation failed
 */
static int op(const char *pk, int *per) {
  
  int status = 1;
  int opcode = OPCODE_NONE;
  int32_t sh = 0;
  int32_t opcount = 0;
  int32_t varcount = 0;
  int32_t x = 0;
  int32_t st = 0;
  int pt = 0;
  STACK_REC sr;
  
  /* Initialize structures */
  memset(&sr, 0, sizeof(STACK_REC));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((pk == NULL) || (per == NULL)) {
    abort();
  }
  
  /* First of all, translate token to opcode */
  if (strcmp(pk, "n") == 0) {
    opcode = OPCODE_NOTE;
  
  } else if (strcmp(pk, "lc") == 0) {
    opcode = OPCODE_LC;
    
  } else if (strcmp(pk, "lr") == 0) {
    opcode = OPCODE_LR;
    
  } else if (strcmp(pk, "layer") == 0) {
    opcode = OPCODE_LAYER;
    
  } else if (strcmp(pk, "derive_layer") == 0) {
    opcode = OPCODE_DERIVE;
    
  } else if (strcmp(pk, "instr") == 0) {
    opcode = OPCODE_INSTR;
  
  } else if (strcmp(pk, "instr_dup") == 0) {
    opcode = OPCODE_IDUP;
    
  } else if (strcmp(pk, "instr_maxmin") == 0) {
    opcode = OPCODE_MAXMIN;
    
  } else if (strcmp(pk, "instr_field") == 0) {
    opcode = OPCODE_FIELD;
    
  } else if (strcmp(pk, "instr_stereo") == 0) {
    opcode = OPCODE_STEREO;
    
  } else {
    /* Unrecognized opcode */
    status = 0;
    *per = ERR_BADOP;
  }
  
  /* Next, make sure stack height is sufficient for operation
   * parameters; for the layer opcode that has varying parameters, make
   * sure height is enough for the fixed parameters; also, save the
   * number of (fixed) parameters */
  if (status) {
    /* Get stack height */
    sh = stack_height();
    
    /* Check stack height and get parameter count */
    if (opcode == OPCODE_NOTE) {
      if (sh >= 5) {
        opcount = 5;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_LC) {
      if (sh >= 2) {
        opcount = 2;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_LR) {
      if (sh >= 3) {
        opcount = 3;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_LAYER) {
      if (sh >= 3) {
        opcount = 3;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_DERIVE) {
      if (sh >= 3) {
        opcount = 3;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_INSTR) {
      if (sh >= 7) {
        opcount = 7;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_IDUP) {
      if (sh >= 2) {
        opcount = 2;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_MAXMIN) {
      if (sh >= 3) {
        opcount = 3;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_FIELD) {
      if (sh >= 5) {
        opcount = 5;
      } else {
        status = 0;
      }
      
    } else if (opcode == OPCODE_STEREO) {
      if (sh >= 2) {
        opcount = 2;
      } else {
        status = 0;
      }
      
    } else {
      /* Unrecognized opcode -- shouldn't happen */
      abort();
    }
    
    /* If there was an error, set error code */
    if (!status) {
      *per = ERR_OPPARAM;
    }
  }

  /* Check that all fixed parameters are integers */
  if (status) {
    for(x = 0; x < opcount; x++) {
      if (stack_type(m_stack_count - x - 1) != PTYPE_INT) {
        status = 0;
        *per = ERR_PARAMT;
        break;
      }
    }
  }
  
  /* Get the number of variable parameters -- for a layer, this is given
   * by the third-from-top parameter; for everything else, this is zero;
   * verify for layer that this is at least one and doesn't exceed stack
   * height */
  if (status && (opcode == OPCODE_LAYER)) {
    /* Layer op -- get count */
    varcount = (m_stack[m_stack_count - 3]).val;
    
    /* Verify count is at least one */
    if (varcount < 1) {
      status = 0;
      *per = ERR_LAYERC;
    }
    
    /* Verify (varcount+opcount) doesn't exceed stack height */
    if (status) {
      if (varcount > sh - opcount) {
        status = 0;
        *per = ERR_LAYERC;
      }
    }
    
  } else if (status) {
    /* Not a layer op -- set varcount to zero */
    varcount = 0;
  }
  
  /* Check that if there are variable parameters, they are all graph
   * types */
  if (status) {
    st = m_stack_count - 1 - opcount;
    for(x = 0; x < varcount; x++) {
      pt = stack_type(st - 1);
      if ((pt != PTYPE_LC) && (pt != PTYPE_LR)) {
        status = 0;
        *per = ERR_PARAMT;
        break;
      }
    }
  }
  
  /* Route to appropriate implementation function */
  if (status) {
    if (opcode == OPCODE_NOTE) {
      if (!op_note(
            stack_int(m_stack_count - 5),
            stack_int(m_stack_count - 4),
            stack_int(m_stack_count - 3),
            stack_int(m_stack_count - 2),
            stack_int(m_stack_count - 1),
            per)) {
        status = 0;
      }
    
    } else if (opcode == OPCODE_LC) {
      if (!op_lc(
            stack_int(m_stack_count - 2),
            stack_int(m_stack_count - 1),
            per, &sr)) {
        status = 0;
      }
      
    } else if (opcode == OPCODE_LR) {
      if (!op_lr(
            stack_int(m_stack_count - 3),
            stack_int(m_stack_count - 2),
            stack_int(m_stack_count - 1),
            per, &sr)) {
        status = 0;
      }
    
    } else if (opcode == OPCODE_LAYER) {
      if (!op_layer(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 2),
            varcount,
            &(m_stack[m_stack_count - opcount - varcount]),
            per)) {
        status = 0;
      }
      
    } else if (opcode == OPCODE_DERIVE) {
      if (!op_derive(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 2),
            stack_int(m_stack_count - 3),
            per)) {
        status = 0;
      }
    
    } else if (opcode == OPCODE_INSTR) {
      if (!op_instr(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 7),
            stack_int(m_stack_count - 6),
            stack_int(m_stack_count - 5),
            stack_int(m_stack_count - 4),
            stack_int(m_stack_count - 3),
            stack_int(m_stack_count - 2),
            per)) {
        status = 0;
      }
  
    } else if (opcode == OPCODE_IDUP) {
      if (!op_idup(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 2),
            per)) {
        status = 0;
      }
    
    } else if (opcode == OPCODE_MAXMIN) {
      if (!op_maxmin(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 3),
            stack_int(m_stack_count - 2),
            per)) {
        status = 0;
      }
    
    } else if (opcode == OPCODE_FIELD) {
      if (!op_field(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 5),
            stack_int(m_stack_count - 4),
            stack_int(m_stack_count - 3),
            stack_int(m_stack_count - 2),
            per)) {
        status = 0;
      }
    
    } else if (opcode == OPCODE_STEREO) {
      if (!op_stereo(
            stack_int(m_stack_count - 1),
            stack_int(m_stack_count - 2),
            per)) {
        status = 0;
      }
    
    } else {
      /* Unrecognized opcode (shouldn't happen) */
      abort();
    }
  }
  
  /* Clear parameters from stack */
  if (status) {
    m_stack_count = m_stack_count - opcount - varcount;
  }
  
  /* If opcode was lc or lr, push the result on the stack */
  if (status && ((opcode == OPCODE_LC) || (opcode == OPCODE_LR))) {
    memcpy(&(m_stack[m_stack_count]), &sr, sizeof(STACK_REC));
    m_stack_count++;
  }
  
  /* Return status */
  return status;
}

/*
 * Called when a group begins while interpreting the Shastina file.
 * 
 * header_config() must be called before this function.
 * 
 * Return:
 * 
 *   non-zero if successful, zero if too much group nesting
 */
static int begin_group(void) {
  
  int status = 1;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check for group overflow */
  if (m_group_count < MAX_GROUP) {
    /* No overflow, so open new group */
    m_group_stack[m_group_count] = m_stack_count;
    m_group_count++;
    
  } else {
    /* Group stack overflow */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Called when a group ends while interpreting the Shastina file.
 * 
 * header_config() must be called before this function.
 * 
 * In order for the group end to be successful, there must be exactly
 * one element left in the stack group
 * 
 * Return:
 * 
 *   non-zero if successful, zero if improper group closing
 */
static int end_group(void) {
  
  int status = 1;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  if (m_group_count < 1) {
    /* Shastina should make sure this doesn't happen */
    abort();
  }
  
  /* Check if stack height is exactly one */
  if (stack_height() == 1) {
    /* Stack height exactly one, so close the group */
    m_group_count--;
    
  } else {
    /* Stack height not exactly one, so improper group closing */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Called to push a number on the stack while interpreting the Shastina
 * file.
 * 
 * header_config() must be called before this function.
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack overflow
 */
static int push_num(int32_t val) {
  
  int status = 1;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check for overflow */
  if (m_stack_count < MAX_STACK) {
    /* No overflow, so push number */
    (m_stack[m_stack_count]).val = val;
    (m_stack[m_stack_count]).ra = -1;
    (m_stack[m_stack_count]).rb = -1;
    m_stack_count++;
    
  } else {
    /* Stack overflow */
    status = 0;
  }
  
  /* Return status */
  return status;
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
  
  /* Check state */
  if (m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((rate != 48000) && (rate != 44100)) {
    abort();
  }
  if ((sqamp < 1) || (frame_before < 0) || (frame_after < 0)) {
    abort();
  }
  
  /* Set initialization flag */
  m_init = 1;
  
  /* Set parameter values */
  m_rate = rate;
  m_sqamp = sqamp;
  if (nostereo) {
    m_nostereo = 1;
  } else {
    m_nostereo = 0;
  }
  m_frame_before = frame_before;
  m_frame_after = frame_after;
  
  /* Initialize stacks */
  m_group_count = 0;
  m_stack_count = 0;
  
  memset(m_group_stack, 0, MAX_GROUP * sizeof(int32_t));
  memset(m_stack, 0, MAX_STACK * sizeof(STACK_REC));
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
  
  /* Check that stack is empty */
  if (status && (m_stack_count > 0)) {
    status = 0;
    *per = ERR_REMAIN;
    *pln = snparser_count(pp);
  }
  
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
      
    case ERR_BADOP:
      pResult = "Unrecognized operation";
      break;
    
    case ERR_OPPARAM:
      pResult = "Operation doesn't have enough parameters";
      break;
    
    case ERR_PARAMT:
      pResult = "Wrong parameter type for operation";
      break;
    
    case ERR_LAYERC:
      pResult = "Invalid parameter count for layer op";
      break;
    
    case ERR_BADT:
      pResult = "t parameter value is negative";
      break;
    
    case ERR_BADFRAC:
      pResult = "Fraction parameter value out of range";
      break;
    
    case ERR_REMAIN:
      pResult = "Elements remaining on stack at end";
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
