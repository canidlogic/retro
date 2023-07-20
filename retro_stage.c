/*
 * retro_stage.c
 * =============
 * 
 * Retro synthesizer stage compiler.
 * 
 * Syntax
 * ------
 * 
 *   retro_stage < input.script > output.state
 * 
 * Description
 * -----------
 * 
 * Reads a Retro stage script in %retro-stage; from standard input,
 * interprets it, and generates the OPL2 register state necessary to set
 * the stage on standard output.
 * 
 * The register state output is an ASCII text file with 256 byte values,
 * each represented by two base-16 characters with zero-padding.  The
 * special value of two ASCII hyphens (--) is used to indicate that the
 * byte value should not be written.  Byte values are separated from
 * each other by a sequence containing at least one space or line break.
 * There may be both leading and trailing whitespace.
 * 
 * The register state output indicates the state of each OPL2 register,
 * such that the first byte value is register zero, the second byte is
 * register one, and so forth.  Registers that do not exist in the OPL2
 * or are registers that are not relevant have the special two-hyphen
 * value to indicate they should not be written.
 * 
 * Dependencies
 * ------------
 * 
 * The following external libraries are required:
 * 
 *   - librfdict
 *   - libshastina
 * 
 * The following Retro modules are required:
 * 
 *   - diagnostic.c
 */

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"

#include "rfdict.h"
#include "shastina.h"

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
 * Constants
 * =========
 */

/*
 * Size of m_lnum_buf.
 *
 * Must be big enough to hold the string "<unknown>" with a terminating
 * nul byte.  Should be big enough to hold the decimal representation of
 * the largest long value.
 */
#define LNUM_BUF_LEN (32)

/*
 * Maximum number of items that can be on the interpreter stack.
 */
#define STACK_MAX_HEIGHT (32)

/*
 * The maximum number of groups on the grouping stack.
 */
#define STACK_MAX_GROUP (32)

/*
 * The maximum number of variables and constant cells that can be
 * allocated.
 */
#define CELL_MAX_COUNT (256)

/*
 * The maximum number of timbre objects that can be defined.
 */
#define MAX_TIMBRE_COUNT (64)

/*
 * The maximum number of instrument objects that can be defined.
 */
#define MAX_INSTRUMENT_COUNT (32)

/*
 * The maximum number of drum sets that can be defined.
 */
#define MAX_DRUM_COUNT (16)

/*
 * Type ID codes.
 */
#define TYPE_NULL       (0)
#define TYPE_INTEGER    (1)
#define TYPE_TIMBRE     (2)
#define TYPE_INSTRUMENT (3)
#define TYPE_DRUM_SET   (4)

/*
 * Wave form shapes.
 */
#define WAVE_SINE   (1)
#define WAVE_HALF   (2)
#define WAVE_DOUBLE (3)
#define WAVE_PULSE  (4)

/*
 * Drum channels.
 */
#define DRUM_CH_BASS    (1)
#define DRUM_CH_BEAT    (2)
#define DRUM_CH_SPLASH  (3)

/*
 * Type declarations
 * =================
 */

/*
 * Structure for holding interpreted data types.
 * 
 * The tcode must be one of the TYPE constants.
 * 
 * The val is the value.  For TYPE_NULL, it is ignored.  For
 * TYPE_INTEGER, it is the integer value which must be greater than or
 * equal to zero.  For the object types, val is an index into the
 * corresponding object table.
 */
typedef struct {
  int16_t tcode;
  int16_t val;
} VARIANT;

/*
 * Memory cell structure for use in the variable and constant bank.
 * 
 * This is a variant along with a flag indicating whether it is
 * constant.
 */
typedef struct {
  VARIANT v;
  int is_const;
} CELL;

/*
 * The TIMBRE structure stores all the settings for a specific operator.
 * 
 * All structure members have a special value -1 indicating not yet
 * defined.
 */
typedef struct {
  
  /*
   * One of the WAVE constants indicating the wave shape.
   */
  int8_t shape;
  
  /*
   * The frequency multiplier.
   * 
   * Integer value in [1, 15] excluding 11, 13, and 14, or the special
   * value 0 indicating 0.5.
   */
  int8_t multiplier;
  
  /*
   * The base amplitude, in range [0, 63].
   */
  int8_t base_amp;
  
  /*
   * The amplitude attenuation, in range [0, 3].
   */
  int8_t amp_attenuate;
  
  /*
   * The ADSR attack length, in range [0, 15].
   */
  int8_t a;
  
  /*
   * The ADSR decay length, in range [0, 15].
   */
  int8_t d;
  
  /*
   * The ADSR sustain level, in range [0, 15].
   */
  int8_t s;
  
  /*
   * The ADSR release length, in range [0, 15].
   */
  int8_t r;
  
  /*
   * The ADSR attenuation, either 0 or 1.
   */
  int8_t adsr_attenuate;
  
  /*
   * Sustain mode flag, either 0 or 1.
   */
  int8_t sustain_mode;
  
  /*
   * Amplitude modulation enable flag, either 0 or 1.
   */
  int8_t am_enable;
  
  /*
   * Vibrato enable flag, either 0 or 1.
   */
  int8_t vibrato;
  
} TIMBRE;

/*
 * The INSTRUMENT structure defines all the settings for one particular
 * channel.
 * 
 * All structure members have a special value -1 indicating not yet
 * defined.
 */
typedef struct {
  
  /*
   * The index of a TIMBRE object in the timbre table that defines the
   * inner operator.
   * 
   * The inner operator is the modulator in FM synthesis.
   */
  int16_t innerTimbre;
  
  /*
   * The index of a TIMBRE object in the timbre table that defines the
   * outer operator.
   * 
   * The outer operator is the one that produces sound in FM synthesis.
   */
  int16_t outerTimbre;
  
  /*
   * Flag that is one to use FM synthesis or zero to use additive 
   * synthesis.
   */
  int8_t use_fm;
  
  /*
   * The amount of feedback on the inner operator.  Range is [0, 7].
   */
  int8_t feedback;
  
} INSTRUMENT;

/*
 * The DRUM_SET structure defines all the settings for a rhythm section.
 * 
 * All structure members have a special value -1 indicating not yet
 * defined.
 */
typedef struct {
  
  /*
   * The index of an INSTRUMENT object in the instrument table that
   * defines the rhythm channel controlling the bass drum.
   */
  int16_t bassInstrument;
  
  /*
   * The index of an INSTRUMENT object in the instrument table that
   * defines the rhythm channel controlling the snare drum and hi-hat.
   */
  int16_t beatInstrument;
  
  /*
   * The index of an INSTRUMENT object in the instrument table that
   * defines the rhythm channel controlling the tom-tom and cymbal.
   */
  int16_t splashInstrument;
  
} DRUM_SET;

/*
 * Local data
 * ==========
 */

/*
 * Buffer for storing line number output for lineString().
 */
static char m_lnum_buf[LNUM_BUF_LEN];

/*
 * The interpreter stack.
 * 
 * m_stack_init indicates whether the stack array has been initialized.
 * m_stack_height is the number of elements in use on the stack.  
 * m_stack is the actual stack.
 */
static int m_stack_init = 0;
static int32_t m_stack_height = 0;
static VARIANT m_stack[STACK_MAX_HEIGHT];

/*
 * The grouping stack.
 * 
 * m_gs_init indicates whether the grouping stack has been initialized.
 * m_gs_height is the current number of elements on the grouping stack.
 * m_gs is the grouping stack.
 * 
 * If the stack is not empty, then the topmost element indicates how
 * many elements on the main stack are currently hidden.
 */
static int m_gs_init = 0;
static int32_t m_gs_height = 0;
static int32_t m_gs[STACK_MAX_GROUP];

/*
 * The variable and constant bank.
 * 
 * m_bank_count is the number of cells in use.  If it is zero, the bank
 * has not been initialized yet.
 */
static int32_t m_bank_count = 0;
static CELL m_bank[CELL_MAX_COUNT];

/*
 * The mapping of variable and constant names to indices within the
 * variable and constant bank.
 * 
 * NULL if not yet initialized.
 */
static RFDICT *m_names = NULL;

/*
 * The accumulator used for constructing objects.
 * 
 * m_acc_type is the type of object being constructed, or TYPE_NULL if
 * nothing is currently being constructed.
 * 
 * The other structures are used for constructing specific types of
 * objects.
 */
static int m_acc_type = TYPE_NULL;
static TIMBRE m_acc_timbre;
static INSTRUMENT m_acc_instr;
static DRUM_SET m_acc_drum;

/*
 * The timbre object table.
 * 
 * m_timbre_count is the number of timbres in use.  If it is zero, the
 * table has not been initialized yet.
 */
static int32_t m_timbre_count = 0;
static TIMBRE m_timbre[MAX_TIMBRE_COUNT];

/*
 * The instrument object table.
 * 
 * m_instr_count is the number of instruments in use.  If it is zero,
 * the table has not been initialized yet.
 */
static int32_t m_instr_count = 0;
static INSTRUMENT m_instr[MAX_INSTRUMENT_COUNT];

/*
 * The drum set object table.
 * 
 * m_drum_count is the number of drum sets in use.  If it is zero, the
 * table has not been initialized yet.
 */
static int32_t m_drum_count = 0;
static DRUM_SET m_drum[MAX_DRUM_COUNT];

/*
 * The orchestra defined by interpreting the script.
 * 
 * m_orch_init is zero if the instrument array has not been initialized
 * yet.
 * 
 * m_orch_drums is the index of a drum set in the drum set table, or -1
 * if there is no drum set in this orchestra.  If there is a drum set,
 * the last three instruments in the instrument array must be undefined.
 * 
 * m_orch_wide_am and m_orch_wide_vibrato are flags that control the
 * depth of amplitude modulation and vibrato across all operators that
 * have those features enabled.
 * 
 * m_orch_instr, once initialized, contains indices into the instrument
 * object table selecting instruments for each channel.  Or, channels
 * can have a value of -1 meaning that no instrument is defined for that
 * channel.  If m_orch_drums is defined, the last three elements of this
 * array must be undefined.
 */
static int m_orch_init = 0;
static int32_t m_orch_drums = -1;
static int8_t m_orch_wide_am = 0;
static int8_t m_orch_wide_vibrato = 0;
static int32_t m_orch_instr[9];

/*
 * The OPL2 register set.
 * 
 * Each register is 8-bit, but 16-bit integers are used so that the
 * special value -1 can be used to indicate that the register shouldn't
 * be written in the image.  Only values [-1, 255] are used.
 * 
 * m_regs_init indicates whether the set has been initialized yet.
 */
static int m_regs_init = 0;
static int16_t m_regs[256];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void initReg(void);
static void writeReg(int reg_i, int val);
static void dumpReg(void);

static int indexOp(int op);
static INSTRUMENT *findInstr(int ch);
static TIMBRE *findTimbre(int op);
static void computeReg(void);

static const char *lineString(long input_line);
static void readEntity(SNPARSER *pp, SNENTITY *pEnt, SNSOURCE *pSrc);
static void readHeader(SNPARSER *pp, SNSOURCE *pSrc);

static void pushVariant(long lnum, const VARIANT *pv);
static void popVariant(long lnum, VARIANT *pv);

static void handleNumeric(long lnum, const char *pStr);
static void handleDefine(long lnum, const char *pKey, int is_const);
static void handleAssign(long lnum, const char *pKey);
static void handleGet(long lnum, const char *pKey);
static void handleBeginGroup(long lnum);
static void handleEndGroup(long lnum);

static void defaultTimbre(TIMBRE *ps);
static void defaultInstrument(INSTRUMENT *ps);
static void defaultDrumSet(DRUM_SET *ps);

static int fullTimbre(const TIMBRE *ps);
static int fullInstrument(const INSTRUMENT *ps);
static int fullDrumSet(const DRUM_SET *ps);

static void opBeginTimbre(long lnum);
static void opInheritTimbre(long lnum);
static void opEndTimbre(long lnum);
static void opShape(long lnum, int wshape);
static void opMultiplier(long lnum);
static void opBaseAmp(long lnum);
static void opAmpAttenuate(long lnum);
static void opAttack(long lnum);
static void opDecay(long lnum);
static void opSustain(long lnum);
static void opRelease(long lnum);
static void opADSRAttenuate(long lnum);
static void opSustainMode(long lnum);
static void opAMEnable(long lnum);
static void opVibrato(long lnum);

static void opBeginInstr(long lnum);
static void opInheritInstr(long lnum);
static void opEndInstr(long lnum);
static void opInnerTimbre(long lnum);
static void opOuterTimbre(long lnum);
static void opSynthMode(long lnum, int use_fm);
static void opFeedback(long lnum);

static void opBeginDrums(long lnum);
static void opInheritDrums(long lnum);
static void opEndDrums(long lnum);
static void opSetDrum(long lnum, int drum_channel);

static void opSetPercussion(long lnum);
static void opSetChannel(long lnum);
static void opSetAMDepth(long lnum);
static void opSetVibratoDepth(long lnum);

/*
 * Initialize the register image if not yet initialized.
 * 
 * This is automatically called by the other register functions.
 */
static void initReg(void) {
  int i = 0;
  if (!m_regs_init) {
    memset(m_regs, 0, sizeof(int16_t) * 256);
    for(i = 0; i < 256; i++) {
      m_regs[i] = (int16_t) -1;
    }
    m_regs_init = 1;
  }
}

/*
 * Write a register value to the register image.
 * 
 * reg_i is the register index, in range [0, 255].
 * 
 * val is the value to write, in range [0, 255].
 * 
 * The register image will be initialized if not already initialized.
 * 
 * Parameters:
 * 
 *   reg_i - the register index to writeReg
 *   
 *   val - the value to write to the register
 */
static void writeReg(int reg_i, int val) {
  if ((reg_i < 0) || (reg_i > 255) ||
      (val < 0) || (val > 255)) {
    raiseErr(__LINE__, NULL);
  }
  
  initReg();
  m_regs[reg_i] = (int16_t) val;
}

/*
 * Dump the register image to standard output.
 * 
 * The register image will be initialized if not already initialized.
 */
static void dumpReg(void) {
  int i = 0;
  initReg();
  for(i = 0; i < 256; i++) {
    if (i > 0) {
      if ((i % 16) == 0) {
        printf("\n");
      } else if ((i % 8) == 0) {
        printf("    ");
      } else {
        printf(" ");
      }
    }
    if (m_regs[i] < 0) {
      printf("--");
    } else {
      printf("%02x", (int) m_regs[i]);
    }
  }
  printf("\n");
}

/*
 * Given the index of an operator, compute the register offset of the
 * operator within a register bank.
 * 
 * op is the desired operator, which is in range [0, 17] to select one
 * of the 18 OPL2 operators.  0 and 1 are the first channel operators,
 * 2 and 3 are the second channel operators, and so forth.
 * 
 * The return value is in range [0, 0x15] and provides the register
 * offset within an OPL2 operator bank.  The register offset uses an
 * unusual ordering.
 * 
 * Parameters:
 * 
 *   op - the index of an operator
 * 
 * Return:
 * 
 *   the register offset within a bank of that operator
 */
static int indexOp(int op) {
  int result = 0;
  
  switch (op) {
    
    case 0:
      result = 0x00;
      break;
      
    case 1:
      result = 0x03;
      break;
    
    case 2:
      result = 0x01;
      break;
    
    case 3:
      result = 0x04;
      break;
    
    case 4:
      result = 0x02;
      break;
    
    case 5:
      result = 0x05;
      break;
    
    case 6:
      result = 0x08;
      break;
    
    case 7:
      result = 0x0b;
      break;
    
    case 8:
      result = 0x09;
      break;
    
    case 9:
      result = 0x0c;
      break;
    
    case 10:
      result = 0x0a;
      break;
    
    case 11:
      result = 0x0d;
      break;
    
    case 12:
      result = 0x10;
      break;
    
    case 13:
      result = 0x13;
      break;
    
    case 14:
      result = 0x11;
      break;
    
    case 15:
      result = 0x14;
      break;
    
    case 16:
      result = 0x12;
      break;
    
    case 17:
      result = 0x15;
      break;
    
    default:
      raiseErr(__LINE__, NULL);
  }
  
  return result;
}

/*
 * Find the instrument definition associated with a given channel index,
 * if one is specified in the orchestra.
 * 
 * NULL is returned if there is no associated instrument.
 * 
 * ch is the zero-based channel index, in range [0, 8].
 * 
 * If no drum set is defined in the orchestra, this uses the orchestra
 * instrument array for all queries.
 * 
 * If a drum set is defined in the orchestra, the last three channel
 * indices are resolved using the percussion instrument definitions
 * instead.
 * 
 * Parameters:
 * 
 *   ch - the zero-based channel index
 * 
 * Return:
 * 
 *   the associated instrument definition, or NULL
 */
static INSTRUMENT *findInstr(int ch) {
  
  int use_drums = 0;
  INSTRUMENT *pInstr = NULL;
  DRUM_SET *pDrum = NULL;
  int32_t ii = 0;
  
  /* Check range */
  if ((ch < 0) || (ch > 8)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Determine whether this is a drum channel */
  use_drums = 0;
  if (m_orch_drums != -1) {
    if (ch >= 6) {
      use_drums = 1;
    }
  }
  
  /* Find the instrument */
  pInstr = NULL;
  if (use_drums) {
    if ((m_orch_drums < 0) || (m_orch_drums >= m_drum_count)) {
      raiseErr(__LINE__, NULL);
    }
    pDrum = &(m_drum[m_orch_drums]);
    if (ch == 6) {
      ii = pDrum->bassInstrument;
    } else if (ch == 7) {
      ii = pDrum->beatInstrument;
    } else if (ch == 8) {
      ii = pDrum->splashInstrument;
    } else {
      raiseErr(__LINE__, NULL);
    }
    if ((ii < 0) || (ii >= m_instr_count)) {
      raiseErr(__LINE__, NULL);
    }
    pInstr = &(m_instr[ii]);
    
  } else {
    if (m_orch_init) {
      if (m_orch_instr[ch] != -1) {
        if ((m_orch_instr[ch] < 0) ||
            (m_orch_instr[ch] >= m_instr_count)) {
          raiseErr(__LINE__, NULL);
        }
        pInstr = &(m_instr[m_orch_instr[ch]]);
      }
    }
  }
  
  /* Return the instrument */
  return pInstr;
}

/*
 * Find the timbre definition associated with a specific operator, if
 * one is specified in the orchestra.
 * 
 * NULL is returned if there is no associated timbre.
 * 
 * op is the operator index, in range [0, 17].  This is NOT the register
 * offset of the operator.
 * 
 * This function first uses findInstr() to find the instrument this
 * operator belongs to.  If there is no associated instrument with the
 * operator's channel, then the result of this function will be NULL.
 * 
 * Once the instrument is found, either the inner or outer operator will
 * be returned, based on the given operator index.
 * 
 * Parameters:
 * 
 *   op - the operator index
 * 
 * Return:
 * 
 *   the associated timbre, or NULL
 */
static TIMBRE *findTimbre(int op) {
  
  INSTRUMENT *pInstr = NULL;
  TIMBRE *pTimbre = NULL;
  int32_t ii = 0;
  
  /* Check parameter */
  if ((op < 0) || (op > 17)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Look up the instrument of the associated channel */
  pInstr = findInstr(op / 2);
  
  /* Only proceed if we found an instrument */
  if (pInstr != NULL) {
    
    /* Get the proper timbre index */
    if ((op % 2) == 1) {
      ii = pInstr->outerTimbre;
    } else {
      ii = pInstr->innerTimbre;
    }
    
    /* Check range */
    if ((ii < 0) || (ii >= m_timbre_count)) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Get the timbre pointer */
    pTimbre = &(m_timbre[ii]);
  }
  
  /* Return result or NULL */
  return pTimbre;
}

/*
 * Compute the full register configuration using the current state of
 * the orchestra.
 * 
 * The register image will be initialized if not already initialized.
 */
static void computeReg(void) {
  
  int i = 0;
  int c = 0;
  
  INSTRUMENT *pInstr = NULL;
  TIMBRE *pTimbre = NULL;
  
  /* Initialize registers if necessary */
  initReg();
  
  /* Begin by resetting all the OPL2 registers to zero, excepting the
   * timer control registers -- start with the global registers */
  writeReg(0x01, 0x00);
  writeReg(0x08, 0x00);
  writeReg(0xbd, 0x00);
  
  /* Now reset all the channel-specific registers */
  for(i = 0; i < 9; i++) {
    writeReg(0xa0 + i, 0x00);
    writeReg(0xb0 + i, 0x00);
    writeReg(0xc0 + i, 0x00);
  }
  
  /* Finally, reset all the operator-specific registers */
  for(i = 0; i < 18; i++) {
    writeReg(0x20 + indexOp(i), 0x00);
    writeReg(0x40 + indexOp(i), 0x00);
    writeReg(0x60 + indexOp(i), 0x00);
    writeReg(0x80 + indexOp(i), 0x00);
    writeReg(0xe0 + indexOp(i), 0x00);
  }
  
  /* REGISTER 0x01 -- Turn on wave select bit so we can specify
   * individual wave forms */
  writeReg(0x01, 0x20);
  
  /* REGISTER 0x08 -- Don't use composite sine-wave mode, and don't use
   * keyboard split point flag */
  writeReg(0x08, 0x00);
  
  /* REGISTER 0xBD -- AM/vibrato global depth settings, and rhythm
   * section enable */
  c = 0;
  if (m_orch_drums != -1) {
    c |= 0x20;
  }
  if (m_orch_wide_vibrato) {
    c |= 0x40;
  }
  if (m_orch_wide_am) {
    c |= 0x80;
  }
  writeReg(0xbd, c);
  
  /* CHANNEL REGISTERS 0xC0-0xC8 -- Set feedback and synthesis type */
  for(i = 0; i < 9; i++) {
    /* Find the instrument or NULL */
    pInstr = findInstr(i);
    
    /* Skip if not defined */
    if (pInstr == NULL) {
      continue;
    }
    
    /* Determine register setting */
    c = 0;
    if (!(pInstr->use_fm)) {
      c |= 0x1;
    }
    c |= (((int) pInstr->feedback) << 1);
    
    /* Write the register */
    writeReg(0xc0 + i, c);
  }
  
  /* OPERATOR REGISTERS 0x20-0x35 -- Various operator settings */
  for(i = 0; i < 18; i++) {
    /* Find the timbre or NULL */
    pTimbre = findTimbre(i);
    
    /* Skip if not defined */
    if (pTimbre == NULL) {
      continue;
    }
    
    /* Determine register setting */
    c = 0;
    if (pTimbre->multiplier == 15) {
      c += 14;
    } else {
      c += ((int) pTimbre->multiplier);
    }
    if (pTimbre->adsr_attenuate) {
      c |= 0x10;
    }
    if (pTimbre->sustain_mode) {
      c |= 0x20;
    }
    if (pTimbre->vibrato) {
      c |= 0x40;
    }
    if (pTimbre->am_enable) {
      c |= 0x80;
    }
    
    /* Write the register */
    writeReg(0x20 + indexOp(i), c);
  }
  
  /* OPERATOR REGISTERS 0x40-0x55 -- Amplitude settings */
  for(i = 0; i < 18; i++) {
    /* Find the timbre or NULL */
    pTimbre = findTimbre(i);
    
    /* Skip if not defined */
    if (pTimbre == NULL) {
      continue;
    }

    /* Determine register setting */
    c = 0;
    c += (63 - ((int) pTimbre->base_amp));
    c |= (((int) pTimbre->amp_attenuate) << 6);
    
    /* Write the register */
    writeReg(0x40 + indexOp(i), c);
  }
  
  /* OPERATOR REGISTERS 0x60-0x75 -- Attack and decay settings */
  for(i = 0; i < 18; i++) {
    /* Find the timbre or NULL */
    pTimbre = findTimbre(i);
    
    /* Skip if not defined */
    if (pTimbre == NULL) {
      continue;
    }
    
    /* Determine register setting */
    c = 0;
    c += (15 - ((int) pTimbre->d));
    c |= ((15 - ((int) pTimbre->a)) << 4);
    
    /* Write the register */
    writeReg(0x60 + indexOp(i), c);
  }
  
  /* OPERATOR REGISTERS 0x80-0x95 -- Attack and decay settings */
  for(i = 0; i < 18; i++) {
    /* Find the timbre or NULL */
    pTimbre = findTimbre(i);
    
    /* Skip if not defined */
    if (pTimbre == NULL) {
      continue;
    }
    
    /* Determine register setting */
    c = 0;
    c += (15 - ((int) pTimbre->r));
    c |= ((15 - ((int) pTimbre->s)) << 4);
    
    /* Write the register */
    writeReg(0x80 + indexOp(i), c);
  }
  
  /* OPERATOR REGISTERS 0xE0-0xF5 -- Waveform select */
  for(i = 0; i < 18; i++) {
    /* Find the timbre or NULL */
    pTimbre = findTimbre(i);
    
    /* Skip if not defined */
    if (pTimbre == NULL) {
      continue;
    }
    
    /* Determine register setting */
    if (pTimbre->shape == WAVE_SINE) {
      c = 0;
      
    } else if (pTimbre->shape == WAVE_HALF) {
      c = 1;
      
    } else if (pTimbre->shape == WAVE_DOUBLE) {
      c = 2;
      
    } else if (pTimbre->shape == WAVE_PULSE) {
      c = 3;
      
    } else {
      raiseErr(__LINE__, NULL);
    }
    
    /* Write the register */
    writeReg(0xe0 + indexOp(i), c);
  }
}

/*
 * Return a string representation of an input file line number, or the
 * string "<unknown>" if the given line number is not valid or the line
 * number doesn't fit in the buffer.
 * 
 * The returned string is held in a static buffer.  Each call to this
 * function overwrites the buffer.
 * 
 * Parameters:
 * 
 *   input_line - the line number from the Shastina parser
 * 
 * Return:
 * 
 *   an appropriate string representation
 */
static const char *lineString(long input_line) {
  
  int retval = 0;
  
  /* Clear the memory buffer */
  memset(m_lnum_buf, 0, LNUM_BUF_LEN);
  
  /* Generate the appropriate string */
  if ((input_line > 0) && (input_line < LONG_MAX)) {
    /* Get the decimal representation */
    retval = snprintf(m_lnum_buf, LNUM_BUF_LEN, "%ld", input_line);
    if ((retval < 0) || (retval > LNUM_BUF_LEN - 1)) {
      memset(m_lnum_buf, 0, LNUM_BUF_LEN);
      strncpy(m_lnum_buf, "<unknown>", LNUM_BUF_LEN - 1);
    }
    
  } else {
    /* Line number is out of range */
    strncpy(m_lnum_buf, "<unknown>", LNUM_BUF_LEN - 1);
  }
  
  /* Return the string in the buffer */
  return m_lnum_buf;
}

/*
 * Wrapper around snparser_read() that checks for and reports error
 * returns.
 * 
 * If this function returns, the entity status is guaranteed to be zero
 * or greater, indicating an entity was successfully read.
 * 
 * Parameters:
 * 
 *   pp - the Shastina parser
 * 
 *   pEnt - the structure to receive the entity
 * 
 *   pSrc - the input source to read from
 */
static void readEntity(SNPARSER *pp, SNENTITY *pEnt, SNSOURCE *pSrc) {
  /* Call through */
  snparser_read(pp, pEnt, pSrc);
  
  /* Check for error */
  if (pEnt->status < 0) {
    /* We got a parser error, so report it */
    raiseErr(__LINE__,
              "Shastina parser error on input line %s: %s",
              lineString(snparser_count(pp)),
              snerror_str(pEnt->status));
  }
}

/*
 * Read the header of the input script and make sure it is valid.
 * 
 * Parameters:
 * 
 *   pp - the Shastina parser
 * 
 *   pSrc - the input source to read from
 */
static void readHeader(SNPARSER *pp, SNSOURCE *pSrc) {
  
  SNENTITY ent;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check parameters */
  if ((pp == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* First entity read must be start of a metacommand */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_BEGIN_META) {
    raiseErr(__LINE__, "Failed to read script header");
  }
  
  /* Second entity read must be metatoken "retro-stage" */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_META_TOKEN) {
    raiseErr(__LINE__, "Failed to read script header");
  }
  if (strcmp(ent.pKey, "retro-stage") != 0) {
    raiseErr(__LINE__, "Wrong input file type");
  }
  
  /* Third entity read must end metacommand or we have an unrecognized
   * version */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_END_META) {
    raiseErr(__LINE__, "Unsupported input file version");
  }
}

/*
 * Push a copy of a variant on top of the stack.
 * 
 * This function does not check that variant structure is valid.  It
 * merely copies the variant onto the stack and makes sure the stack
 * state is updated.
 * 
 * Parameters:
 * 
 *   lnum - the input file line for diagnostics
 * 
 *   pv - the variant to copy
 */
static void pushVariant(long lnum, const VARIANT *pv) {
  
  /* Check parameters */
  if (pv == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If stack is not initialized, initialize it */
  if (!m_stack_init) {
    memset(m_stack, 0, sizeof(VARIANT) * STACK_MAX_HEIGHT);
    m_stack_init = 1;
  }
  
  /* Check that room still in the stack */
  if (m_stack_height >= STACK_MAX_HEIGHT) {
    raiseErr(__LINE__, "Interpreter stack overflow at input line %s",
                        lineString(lnum));
  }
  
  /* Copy variant into stack */
  memcpy(&(m_stack[m_stack_height]), pv, sizeof(VARIANT));
  m_stack_height++;
}

/*
 * Pop a variant off the top of the stack.
 * 
 * This function does not check that variant structure is valid.  It
 * merely copies the variant from the stack and makes sure the stack
 * state is updated.
 * 
 * This function takes the grouping stack into account.  It will not pop
 * a variant off the stack that is hidden by grouping.
 * 
 * Parameters:
 * 
 *   lnum - the input file line for diagnostics
 * 
 *   pv - the variant structure to copy the value into
 */
static void popVariant(long lnum, VARIANT *pv) {
  
  int32_t group_top = 0;
  
  /* Check parameters */
  if (pv == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If there is an open group, get the number of hidden elements from
   * the top of the grouping stack; else, the number of hidden elements
   * is zero */
  if (m_gs_height > 0) {
    group_top = m_gs[m_gs_height - 1];
  } else {
    group_top = 0;
  }
  
  /* Check that there is an unhidden value to pop */
  if (group_top >= m_stack_height) {
    raiseErr(__LINE__, "Interpreter stack underflow at input line %s",
                        lineString(lnum));
  }
  
  /* Copy variant from stack */
  memcpy(pv, &(m_stack[m_stack_height - 1]), sizeof(VARIANT));
  m_stack_height--;
}

/*
 * Handle a numeric Shastina entity.
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   pStr - the numeric entity string
 */
static void handleNumeric(long lnum, const char *pStr) {
  
  VARIANT v;
  int c = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check parameters */
  if (pStr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Set type to integer */
  v.tcode = (int16_t) TYPE_INTEGER;
  
  /* Value starts out zero */
  v.val = (int16_t) 0;
  
  /* Make sure decimal string is not empty */
  if (pStr[0] == 0) {
    raiseErr(__LINE__, "Empty numeric on input line %s",
                        lineString(lnum));
  }
  
  /* Parse the numeric value */
  for( ; *pStr != 0; pStr++) {
    
    /* Get current character */
    c = *pStr;
    
    /* Parse as digit */
    if ((c >= '0') && (c <= '9')) {
      c = c - '0';
    } else {
      raiseErr(__LINE__, "Invalid numeric on input line %s",
                          lineString(lnum));
    }
    
    /* Multiply value by 10, watching for overflow */
    if (v.val <= INT16_MAX / 10) {
      v.val *= ((int16_t) 10);
    } else {
      raiseErr(__LINE__, "Numeric out of range on input line %s",
                          lineString(lnum));
    }
    
    /* Add digit to value, watching for overflow */
    if (v.val <= INT16_MAX - c) {
      v.val += ((int16_t) c);
    } else {
      raiseErr(__LINE__, "Numeric out of range on input line %s",
                          lineString(lnum));
    }
  }
  
  /* Push numeric value onto stack */
  pushVariant(lnum, &v);
}

/*
 * Handle a variable or constant definition Shastina entity.
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   pKey - the variable or constant string
 * 
 *   is_const - non-zero if defining a constant, zero for variable
 */
static void handleDefine(long lnum, const char *pKey, int is_const) {
  
  /* Check parameters */
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If name mapping not initialized yet, initialize it */
  if (m_names == NULL) {
    m_names = rfdict_alloc(1);
  }
  
  /* If variable and constant bank not initialized, intialize it */
  if (m_bank_count < 1) {
    memset(m_bank, 0, sizeof(CELL) * CELL_MAX_COUNT);
  }
  
  /* Check that space available in cell bank */
  if (m_bank_count >= CELL_MAX_COUNT) {
    raiseErr(__LINE__, "Too many definitions reached on input line %s",
                        lineString(lnum));
  }
  
  /* Map name to new cell index */
  if (!rfdict_insert(m_names, pKey, (long) m_bank_count)) {
    raiseErr(__LINE__, "Redefinition of %s on input line %s",
                        pKey, lineString(lnum));
  }
  
  /* Pop initializing value from stack into cell */
  popVariant(lnum, &((m_bank[m_bank_count]).v));
  
  /* Set constant flag appropriately */
  if (is_const) {
    (m_bank[m_bank_count]).is_const = 1;
  } else {
    (m_bank[m_bank_count]).is_const = 0;
  }
  
  /* Increase the bank count */
  m_bank_count++;
}

/*
 * Handle a Shastina variable assignment entity.
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   pKey - the name of the variable
 */
static void handleAssign(long lnum, const char *pKey) {
  
  long i = 0;
  
  /* Check parameters */
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If name mapping not initialized yet, initialize it */
  if (m_names == NULL) {
    m_names = rfdict_alloc(1);
  }
  
  /* Look up the cell index of the name */
  i = rfdict_get(m_names, pKey, -1);
  if (i < 0) {
    raiseErr(__LINE__, "Undefined name %s on input line %s",
                        pKey, lineString(lnum));
  }
  
  /* Make sure this isn't a constant */
  if ((m_bank[i]).is_const) {
    raiseErr(__LINE__, "Can't assign to constant %s on input line %s",
                        pKey, lineString(lnum));
  }
  
  /* Pop new value from top of stack into cell */
  popVariant(lnum, &((m_bank[i]).v));
}

/*
 * Handle a Shastina variable or constant get entity.
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   pKey - the name of the variable or constant
 */
static void handleGet(long lnum, const char *pKey) {
  
  long i = 0;
  
  /* Check parameters */
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If name mapping not initialized yet, initialize it */
  if (m_names == NULL) {
    m_names = rfdict_alloc(1);
  }
  
  /* Look up the cell index of the name */
  i = rfdict_get(m_names, pKey, -1);
  if (i < 0) {
    raiseErr(__LINE__, "Undefined name %s on input line %s",
                        pKey, lineString(lnum));
  }
  
  /* Push value from cell onto top of stack */
  pushVariant(lnum, &((m_bank[i]).v));
}

/*
 * Handle a Shastina begin group entity.
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void handleBeginGroup(long lnum) {
  
  /* Initialize grouping stack if not yet initialized */
  if (!m_gs_init) {
    memset(m_gs, 0, sizeof(int32_t) * STACK_MAX_GROUP);
    m_gs_init = 1;
  }
  
  /* Make sure space left on grouping stack */
  if (m_gs_height >= STACK_MAX_GROUP) {
    raiseErr(__LINE__, "Too much group nesting on input line %s",
                lineString(lnum));
  }
  
  /* Push the current stack height on top of the grouping stack to hide
   * everything currently on the interpreter stack */
  m_gs[m_gs_height] = m_stack_height;
  m_gs_height++;
}

/*
 * Handle a Shastina end group entity.
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void handleEndGroup(long lnum) {
  
  /* Make sure grouping stack is not empty */
  if (m_gs_height < 1) {
    raiseErr(__LINE__, "Grouping mismatch on input line %s",
                lineString(lnum));
  }
  
  /* Make sure current stack height is one greater than value on top of
   * grouping stack */
  if (m_stack_height - 1 != m_gs[m_gs_height - 1]) {
    raiseErr(__LINE__, "Grouping constraint violated on input line %s",
                    lineString(lnum));
  }
  
  /* Pop the grouping stack */
  m_gs_height--;
}

/*
 * Clear a given timbre structure and set the default values.
 * 
 * Fields that do not have default values are set to -1.
 * 
 * Parameters:
 * 
 *   ps - the structure to set to defaults
 */
static void defaultTimbre(TIMBRE *ps) {
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  memset(ps, 0, sizeof(TIMBRE));
  
  ps->shape          = (int8_t) WAVE_SINE;
  ps->multiplier     = (int8_t)  1;
  ps->base_amp       = (int8_t) -1;
  ps->amp_attenuate  = (int8_t)  0;
  ps->a              = (int8_t) -1;
  ps->d              = (int8_t) -1;
  ps->s              = (int8_t) -1;
  ps->r              = (int8_t) -1;
  ps->adsr_attenuate = (int8_t)  0;
  ps->sustain_mode   = (int8_t)  1;
  ps->am_enable      = (int8_t)  0;
  ps->vibrato        = (int8_t)  0;
}

/*
 * Clear a given instrument structure and set the default values.
 * 
 * Fields that do not have default values are set to -1.
 * 
 * Parameters:
 * 
 *   ps - the structure to set to defaults
 */
static void defaultInstrument(INSTRUMENT *ps) {
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  memset(ps, 0, sizeof(INSTRUMENT));
  
  ps->innerTimbre = (int16_t) -1;
  ps->outerTimbre = (int16_t) -1;
  ps->use_fm      = ( int8_t)  1;
  ps->feedback    = ( int8_t)  0;
}

/*
 * Clear a given drum set structure and set the default values.
 * 
 * Fields that do not have default values are set to -1.
 * 
 * Parameters:
 * 
 *   ps - the structure to set to defaults
 */
static void defaultDrumSet(DRUM_SET *ps) {
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  memset(ps, 0, sizeof(DRUM_SET));
  
  ps->bassInstrument   = (int16_t) -1;
  ps->beatInstrument   = (int16_t) -1;
  ps->splashInstrument = (int16_t) -1;
}

/*
 * Check whether a given timbre structure is fully initialized.
 * 
 * Does not check individual fields for validity.
 * 
 * Parameters:
 * 
 *   ps - the structure to check
 * 
 * Return:
 * 
 *   non-zero if all fields filled in, zero otherwise
 */
static int fullTimbre(const TIMBRE *ps) {
  int result = 0;
  
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if ((ps->shape          >= 0) &&
      (ps->multiplier     >= 0) &&
      (ps->base_amp       >= 0) &&
      (ps->amp_attenuate  >= 0) &&
      (ps->a              >= 0) &&
      (ps->d              >= 0) &&
      (ps->s              >= 0) &&
      (ps->r              >= 0) &&
      (ps->adsr_attenuate >= 0) &&
      (ps->sustain_mode   >= 0) &&
      (ps->am_enable      >= 0) &&
      (ps->vibrato        >= 0)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Check whether a given instrument structure is fully initialized.
 * 
 * Does not check individual fields for validity.
 * 
 * Parameters:
 * 
 *   ps - the structure to check
 * 
 * Return:
 * 
 *   non-zero if all fields filled in, zero otherwise
 */
static int fullInstrument(const INSTRUMENT *ps) {
  int result = 0;
  
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if ((ps->innerTimbre >= 0) &&
      (ps->outerTimbre >= 0) &&
      (ps->use_fm      >= 0) &&
      (ps->feedback    >= 0)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Check whether a given drum set structure is fully initialized.
 * 
 * Does not check individual fields for validity.
 * 
 * Parameters:
 * 
 *   ps - the structure to check
 * 
 * Return:
 * 
 *   non-zero if all fields filled in, zero otherwise
 */
static int fullDrumSet(const DRUM_SET *ps) {
  int result = 0;
  
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if ((ps->bassInstrument   >= 0) &&
      (ps->beatInstrument   >= 0) &&
      (ps->splashInstrument >= 0)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * - begin_timbre -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opBeginTimbre(long lnum) {
  
  /* Make sure accumulator is empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not completed on input line %s",
                  lineString(lnum));
  }
  
  /* Set up default timbre in accumulator and fill accumulator */
  defaultTimbre(&m_acc_timbre);
  m_acc_type = TYPE_TIMBRE;
}

/*
 * [timbre] inherit_timbre -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opInheritTimbre(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Make sure accumulator is empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not completed on input line %s",
                  lineString(lnum));
  }
  
  /* Pop the parent timbre from the stack */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_TIMBRE) {
    raiseErr(__LINE__, "Expecting timbre parameter on input line %s",
                    lineString(lnum));
  }
  
  /* Check timbre index */
  if ((v.val < 0) || (v.val >= m_timbre_count)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Copy indicated timbre into accumulator and fill accumulator */
  memcpy(&m_acc_timbre, &(m_timbre[v.val]), sizeof(TIMBRE));
  m_acc_type = TYPE_TIMBRE;
}

/*
 * - end_timbre [timbre]
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opEndTimbre(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Make sure accumulator holds a timbre */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "Accumulator has no timbre on input line %s",
                  lineString(lnum));
  }
  
  /* Make sure timbre is completely defined */
  if (!fullTimbre(&m_acc_timbre)) {
    raiseErr(__LINE__, "Incomplete timbre on input line %s",
                  lineString(lnum));
  }
  
  /* Initialize timbre table if necessary */
  if (m_timbre_count < 1) {
    memset(m_timbre, 0, sizeof(TIMBRE) * MAX_TIMBRE_COUNT);
  }
  
  /* Make sure room for another timbre */
  if (m_timbre_count >= MAX_TIMBRE_COUNT) {
    raiseErr(__LINE__, "Timbre limit reached on input line %s",
                  lineString(lnum));
  }
  
  /* Copy completed timbre to timbre table and store reference in the
   * local variant */
  memcpy(&(m_timbre[m_timbre_count]), &m_acc_timbre, sizeof(TIMBRE));
  v.tcode = (int16_t) TYPE_TIMBRE;
  v.val = (int16_t) m_timbre_count;
  
  /* Increase timbre count and push reference onto stack */
  m_timbre_count++;
  pushVariant(lnum, &v);
  
  /* Clear accumulator */
  m_acc_type = TYPE_NULL;
}

/*
 * - sine_wave -
 * - half_wave -
 * - double_wave -
 * - pulse_wave -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   wshape - one of the WAVE constants indicating the desired wave
 *   shape
 */
static void opShape(long lnum, int wshape) {
  
  /* Check parameters */
  if ((wshape != WAVE_SINE) && (wshape != WAVE_HALF) &&
      (wshape != WAVE_DOUBLE) && (wshape != WAVE_PULSE)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.shape = (int8_t) wshape;
}

/*
 * [numerator:integer] [denominator:integer] multiplier -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opMultiplier(long lnum) {
  
  VARIANT v;
  int32_t p_num = 0;
  int32_t p_denom = 0;
  int8_t pv = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameters */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p_num = v.val;
  
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p_denom = v.val;
  
  /* Check parameter range and decode to storage value */
  if (p_denom == 1) {
    /* Denominator is 1 so we have an integer value */
    if ((p_num >= 1) && (p_num <= 15) &&
          (p_num != 11) && (p_num != 13) && (p_num != 14)) {
      pv = (int8_t) p_num;
      
    } else {
      raiseErr(__LINE__, "Unsupported multiplier on input line %s",
                lineString(lnum));
    }
    
  } else if (p_denom == 2) {
    /* We only support 1/2 as fractional value */
    if (p_num == 1) {
      pv = 0;
      
    } else {
      raiseErr(__LINE__, "Unsupported multiplier on input line %s",
                lineString(lnum));
    }
    
  } else {
    raiseErr(__LINE__, "Unsupported multiplier on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.multiplier = pv;
}

/*
 * [integer] amplitude -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opBaseAmp(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 63)) {
    raiseErr(__LINE__, "Amplitude out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.base_amp = (int8_t) p;
}

/*
 * [integer] amplitude_attenuate -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opAmpAttenuate(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 3)) {
    raiseErr(__LINE__,
              "Amplitude attenuation out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.amp_attenuate = (int8_t) p;
}

/*
 * [integer] attack -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opAttack(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 15)) {
    raiseErr(__LINE__, "Attack out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.a = (int8_t) p;
}

/*
 * [integer] decay -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opDecay(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 15)) {
    raiseErr(__LINE__, "Decay out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.d = (int8_t) p;
}

/*
 * [integer] sustain -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opSustain(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 15)) {
    raiseErr(__LINE__, "Sustain out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.s = (int8_t) p;
}

/*
 * [integer] release -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opRelease(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 15)) {
    raiseErr(__LINE__, "Release out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.r = (int8_t) p;
}

/*
 * [integer] envelope_attenuate -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opADSRAttenuate(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 1)) {
    raiseErr(__LINE__,
              "Envelope attenuation out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.adsr_attenuate = (int8_t) p;
}

/*
 * [integer] sustain_mode -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opSustainMode(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 1)) {
    raiseErr(__LINE__, "Sustain mode out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.sustain_mode = (int8_t) p;
}

/*
 * [integer] am_enable -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opAMEnable(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 1)) {
    raiseErr(__LINE__, "AM enable out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.am_enable = (int8_t) p;
}

/*
 * [integer] vibrato_enable -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opVibrato(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that timbre is in accumulator */
  if (m_acc_type != TYPE_TIMBRE) {
    raiseErr(__LINE__, "No timbre loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 15)) {
    raiseErr(__LINE__, "Vibrato enable out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_timbre.vibrato = (int8_t) p;
}

/*
 * - begin_instr -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opBeginInstr(long lnum) {
  
  /* Make sure accumulator is empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not completed on input line %s",
                  lineString(lnum));
  }
  
  /* Set up default instrument in accumulator and fill accumulator */
  defaultInstrument(&m_acc_instr);
  m_acc_type = TYPE_INSTRUMENT;
}

/*
 * [instrument] inherit_instr -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opInheritInstr(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Make sure accumulator is empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not completed on input line %s",
                  lineString(lnum));
  }
  
  /* Pop the parent instrument from the stack */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INSTRUMENT) {
    raiseErr(__LINE__,
                "Expecting instrument parameter on input line %s",
                    lineString(lnum));
  }
  
  /* Check instrument index */
  if ((v.val < 0) || (v.val >= m_instr_count)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Copy indicated instrument into accumulator and fill accumulator */
  memcpy(&m_acc_instr, &(m_instr[v.val]), sizeof(INSTRUMENT));
  m_acc_type = TYPE_INSTRUMENT;
}

/*
 * - end_instr [instrument]
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opEndInstr(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Make sure accumulator holds an instrument */
  if (m_acc_type != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "Accumulator has no instrument on input line %s",
                  lineString(lnum));
  }
  
  /* Make sure instrument is completely defined */
  if (!fullInstrument(&m_acc_instr)) {
    raiseErr(__LINE__, "Incomplete instrument on input line %s",
                  lineString(lnum));
  }
  
  /* Initialize instrument table if necessary */
  if (m_instr_count < 1) {
    memset(m_instr, 0, sizeof(INSTRUMENT) * MAX_INSTRUMENT_COUNT);
  }
  
  /* Make sure room for another instrument */
  if (m_instr_count >= MAX_INSTRUMENT_COUNT) {
    raiseErr(__LINE__, "Instrument limit reached on input line %s",
                  lineString(lnum));
  }
  
  /* Copy completed instrument to instrument table and store reference
   * in the local variant */
  memcpy(&(m_instr[m_instr_count]), &m_acc_instr, sizeof(INSTRUMENT));
  v.tcode = (int16_t) TYPE_INSTRUMENT;
  v.val = (int16_t) m_instr_count;
  
  /* Increase instrument count and push reference onto stack */
  m_instr_count++;
  pushVariant(lnum, &v);
  
  /* Clear accumulator */
  m_acc_type = TYPE_NULL;
}

/*
 * [timbre] inner_timbre -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opInnerTimbre(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that instrument is in accumulator */
  if (m_acc_type != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "No instrument loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_TIMBRE) {
    raiseErr(__LINE__, "Expecting timbre on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_instr.innerTimbre = v.val;
}

/*
 * [timbre] outer_timbre -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opOuterTimbre(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that instrument is in accumulator */
  if (m_acc_type != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "No instrument loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_TIMBRE) {
    raiseErr(__LINE__, "Expecting timbre on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_instr.outerTimbre = v.val;
}

/*
 * - fm_synthesis -
 * - additive_synthesis -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   use_fm - non-zero for FM synthesis, zero for additive synthesis
 */
static void opSynthMode(long lnum, int use_fm) {
  
  /* Check that instrument is in accumulator */
  if (m_acc_type != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "No instrument loaded on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  if (use_fm) {
    m_acc_instr.use_fm = (int8_t) 1;
  } else {
    m_acc_instr.use_fm = (int8_t) 0;
  }
}

/*
 * [integer] feedback -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opFeedback(long lnum) {
  
  VARIANT v;
  int32_t p = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check that instrument is in accumulator */
  if (m_acc_type != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "No instrument loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  p = v.val;
  
  /* Check parameter range */
  if ((p < 0) || (p > 7)) {
    raiseErr(__LINE__, "Feedback out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  m_acc_instr.feedback = (int8_t) p;
}

/*
 * - begin_drums -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opBeginDrums(long lnum) {
  
  /* Make sure accumulator is empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not completed on input line %s",
                  lineString(lnum));
  }
  
  /* Set up default drum set in accumulator and fill accumulator */
  defaultDrumSet(&m_acc_drum);
  m_acc_type = TYPE_DRUM_SET;
}

/*
 * [drum_set] inherit_drums -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opInheritDrums(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Make sure accumulator is empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not completed on input line %s",
                  lineString(lnum));
  }
  
  /* Pop the parent drum set from the stack */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_DRUM_SET) {
    raiseErr(__LINE__,
                "Expecting drum set parameter on input line %s",
                    lineString(lnum));
  }
  
  /* Check drum set index */
  if ((v.val < 0) || (v.val >= m_drum_count)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Copy indicated drum set into accumulator and fill accumulator */
  memcpy(&m_acc_drum, &(m_drum[v.val]), sizeof(DRUM_SET));
  m_acc_type = TYPE_DRUM_SET;
}

/*
 * - end_drums [drum_set]
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opEndDrums(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Make sure accumulator holds a drum set */
  if (m_acc_type != TYPE_DRUM_SET) {
    raiseErr(__LINE__, "Accumulator has no drum set on input line %s",
                  lineString(lnum));
  }
  
  /* Make sure drum set is completely defined */
  if (!fullDrumSet(&m_acc_drum)) {
    raiseErr(__LINE__, "Incomplete drum set on input line %s",
                  lineString(lnum));
  }
  
  /* Initialize drum set table if necessary */
  if (m_drum_count < 1) {
    memset(m_drum, 0, sizeof(DRUM_SET) * MAX_DRUM_COUNT);
  }
  
  /* Make sure room for another drum set */
  if (m_drum_count >= MAX_DRUM_COUNT) {
    raiseErr(__LINE__, "Drum set limit reached on input line %s",
                  lineString(lnum));
  }
  
  /* Copy completed drum set to drum set table and store reference in
   * the local variant */
  memcpy(&(m_drum[m_drum_count]), &m_acc_drum, sizeof(DRUM_SET));
  v.tcode = (int16_t) TYPE_DRUM_SET;
  v.val = (int16_t) m_drum_count;
  
  /* Increase drum set count and push reference onto stack */
  m_drum_count++;
  pushVariant(lnum, &v);
  
  /* Clear accumulator */
  m_acc_type = TYPE_NULL;
}

/*
 * [instrument] bass_channel -
 * [instrument] beat_channel -
 * [instrument] splash_channel -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 * 
 *   drum_channel - one of the DRUM_CH constants
 */
static void opSetDrum(long lnum, int drum_channel) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Check parameters */
  if ((drum_channel != DRUM_CH_BASS) &&
      (drum_channel != DRUM_CH_BEAT) &&
      (drum_channel != DRUM_CH_SPLASH)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check that drum set is in accumulator */
  if (m_acc_type != TYPE_DRUM_SET) {
    raiseErr(__LINE__, "No drum set loaded on input line %s",
                lineString(lnum));
  }
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "Expecting instrument on input line %s",
                lineString(lnum));
  }
  
  /* Update accumulator */
  if (drum_channel == DRUM_CH_BASS) {
    m_acc_drum.bassInstrument = v.val;
  
  } else if (drum_channel == DRUM_CH_BEAT) {
    m_acc_drum.beatInstrument = v.val;
    
  } else if (drum_channel == DRUM_CH_SPLASH) {
    m_acc_drum.splashInstrument = v.val;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * [drum_set] set_percussion -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opSetPercussion(long lnum) {
  
  VARIANT v;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_DRUM_SET) {
    raiseErr(__LINE__, "Expecting drum set on input line %s",
                lineString(lnum));
  }
  
  /* Check that if orchestra is initialized, last three channels are
   * undefined */
  if (m_orch_init) {
    if ((m_orch_instr[6] != -1) ||
        (m_orch_instr[7] != -1) ||
        (m_orch_instr[8] != -1)) {
      raiseErr(__LINE__, "Percussion conflict on input line %s",
                  lineString(lnum));
    }
  }
  
  /* Update orchestra */
  m_orch_drums = v.val;
}

/*
 * [instrument] [channel:integer] set_channel -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opSetChannel(long lnum) {
  
  VARIANT v;
  int32_t ch = 0;
  int32_t i = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Get stack parameters */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  ch = v.val;
  
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INSTRUMENT) {
    raiseErr(__LINE__, "Expecting instrument on input line %s",
                lineString(lnum));
  }
  
  /* Check channel range */
  if ((ch < 1) || (ch > 9)) {
    raiseErr(__LINE__, "Channel number out of range on input line %s",
                lineString(lnum));
  }
  
  /* If percussion is defined, make sure not setting the last three
   * channels */
  if (m_orch_drums != -1) {
    if (ch >= 7) {
      raiseErr(__LINE__, "Percussion conflict on input line %s",
                  lineString(lnum));
    }
  }
  
  /* If orchestra not initialized yet, initialize it */
  if (!m_orch_init) {
    memset(m_orch_instr, 0, sizeof(int32_t) * 9);
    for(i = 0; i < 9; i++) {
      m_orch_instr[i] = (int16_t) -1;
    }
    m_orch_init = 1;
  }
  
  /* Update orchestra */
  m_orch_instr[ch - 1] = v.val;
}

/*
 * [integer] wide_am -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opSetAMDepth(long lnum) {
  
  VARIANT v;
  int32_t depth = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  depth = v.val;
  
  /* Check parameter range */
  if ((depth < 0) || (depth > 1)) {
    raiseErr(__LINE__, "Depth parameter out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update orchestra */
  m_orch_wide_am = (int8_t) depth;
}

/*
 * [integer] wide_vibrato -
 * 
 * Parameters:
 * 
 *   lnum - the input line number for diagnostics
 */
static void opSetVibratoDepth(long lnum) {
  
  VARIANT v;
  int32_t depth = 0;
  
  /* Initialize structures */
  memset(&v, 0, sizeof(VARIANT));
  
  /* Get stack parameter */
  popVariant(lnum, &v);
  if (v.tcode != TYPE_INTEGER) {
    raiseErr(__LINE__, "Expecting integer on input line %s",
                lineString(lnum));
  }
  depth = v.val;
  
  /* Check parameter range */
  if ((depth < 0) || (depth > 1)) {
    raiseErr(__LINE__, "Depth parameter out of range on input line %s",
                lineString(lnum));
  }
  
  /* Update orchestra */
  m_orch_wide_vibrato = (int8_t) depth;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  SNSOURCE *pSrc = NULL;
  SNPARSER *pp = NULL;
  SNENTITY ent;
  int retval = 0;
  long lnum = 0;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Initialize diagnostics and check arguments */
  diagnostic_startup(argc, argv, "retro_stage");
  
  /* Make sure no program arguments beyond the executable name */
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  /* Open standard input as Shastina source */
  pSrc = snsource_stream(stdin, SNSTREAM_NORMAL);
  
  /* Create a Shastina parser */
  pp = snparser_alloc();
  
  /* Read the input file header */
  readHeader(pp, pSrc);
  
  /* Process all Shastina entities after the header until EOF entity
   * encountered */
  for(readEntity(pp, &ent, pSrc);
      ent.status != SNENTITY_EOF;
      readEntity(pp, &ent, pSrc)) {
    
    /* Get the line number for diagnostics */
    lnum = snparser_count(pp);
    
    /* Handle the different entities */
    if (ent.status == SNENTITY_NUMERIC) {
      handleNumeric(lnum, ent.pKey);
      
    } else if (ent.status == SNENTITY_VARIABLE) {
      handleDefine(lnum, ent.pKey, 0);
      
    } else if (ent.status == SNENTITY_CONSTANT) {
      handleDefine(lnum, ent.pKey, 1);
      
    } else if (ent.status == SNENTITY_ASSIGN) {
      handleAssign(lnum, ent.pKey);
      
    } else if (ent.status == SNENTITY_GET) {
      handleGet(lnum, ent.pKey);
      
    } else if (ent.status == SNENTITY_BEGIN_GROUP) {
      handleBeginGroup(lnum);
      
    } else if (ent.status == SNENTITY_END_GROUP) {
      handleEndGroup(lnum);
      
    } else if (ent.status == SNENTITY_OPERATION) {
      
      if (strcmp(ent.pKey, "begin_timbre") == 0) {
        opBeginTimbre(lnum);
        
      } else if (strcmp(ent.pKey, "inherit_timbre") == 0) {
        opInheritTimbre(lnum);
        
      } else if (strcmp(ent.pKey, "end_timbre") == 0) {
        opEndTimbre(lnum);
      
      } else if (strcmp(ent.pKey, "sine_wave") == 0) {
        opShape(lnum, WAVE_SINE);
        
      } else if (strcmp(ent.pKey, "half_wave") == 0) {
        opShape(lnum, WAVE_HALF);
        
      } else if (strcmp(ent.pKey, "double_wave") == 0) {
        opShape(lnum, WAVE_DOUBLE);
        
      } else if (strcmp(ent.pKey, "pulse_wave") == 0) {
        opShape(lnum, WAVE_PULSE);
        
      } else if (strcmp(ent.pKey, "multiplier") == 0) {
        opMultiplier(lnum);
        
      } else if (strcmp(ent.pKey, "amplitude") == 0) {
        opBaseAmp(lnum);
        
      } else if (strcmp(ent.pKey, "amplitude_attenuate") == 0) {
        opAmpAttenuate(lnum);
      
      } else if (strcmp(ent.pKey, "attack") == 0) {
        opAttack(lnum);
      
      } else if (strcmp(ent.pKey, "decay") == 0) {
        opDecay(lnum);
        
      } else if (strcmp(ent.pKey, "sustain") == 0) {
        opSustain(lnum);
        
      } else if (strcmp(ent.pKey, "release") == 0) {
        opRelease(lnum);
        
      } else if (strcmp(ent.pKey, "envelope_attenuate") == 0) {
        opADSRAttenuate(lnum);
        
      } else if (strcmp(ent.pKey, "sustain_mode") == 0) {
        opSustainMode(lnum);
        
      } else if (strcmp(ent.pKey, "am_enable") == 0) {
        opAMEnable(lnum);
        
      } else if (strcmp(ent.pKey, "vibrato_enable") == 0) {
        opVibrato(lnum);
        
      } else if (strcmp(ent.pKey, "begin_instr") == 0) {
        opBeginInstr(lnum);
        
      } else if (strcmp(ent.pKey, "inherit_instr") == 0) {
        opInheritInstr(lnum);
        
      } else if (strcmp(ent.pKey, "end_instr") == 0) {
        opEndInstr(lnum);
        
      } else if (strcmp(ent.pKey, "inner_timbre") == 0) {
        opInnerTimbre(lnum);
        
      } else if (strcmp(ent.pKey, "outer_timbre") == 0) {  
        opOuterTimbre(lnum);
        
      } else if (strcmp(ent.pKey, "fm_synthesis") == 0) {
        opSynthMode(lnum, 1);
        
      } else if (strcmp(ent.pKey, "additive_synthesis") == 0) {
        opSynthMode(lnum,  0);
      
      } else if (strcmp(ent.pKey, "feedback") == 0) {
        opFeedback(lnum);
        
      } else if (strcmp(ent.pKey, "begin_drums") == 0) {
        opBeginDrums(lnum);
        
      } else if (strcmp(ent.pKey, "inherit_drums") == 0) {
        opInheritDrums(lnum);
        
      } else if (strcmp(ent.pKey, "end_drums") == 0) {
        opEndDrums(lnum);
        
      } else if (strcmp(ent.pKey, "bass_channel") == 0) {
        opSetDrum(lnum, DRUM_CH_BASS);
      
      } else if (strcmp(ent.pKey, "beat_channel") == 0) {
        opSetDrum(lnum, DRUM_CH_BEAT);
        
      } else if (strcmp(ent.pKey, "splash_channel") == 0) {
        opSetDrum(lnum, DRUM_CH_SPLASH);
        
      } else if (strcmp(ent.pKey, "set_percussion") == 0) {
        opSetPercussion(lnum);
        
      } else if (strcmp(ent.pKey, "set_channel") == 0) {
        opSetChannel(lnum);
        
      } else if (strcmp(ent.pKey, "wide_am") == 0) {
        opSetAMDepth(lnum);
        
      } else if (strcmp(ent.pKey, "wide_vibrato") == 0) {
        opSetVibratoDepth(lnum);
      
      } else {
        raiseErr(__LINE__, "Unrecognized operation %s on line %s",
                  ent.pKey, lineString(lnum));
      }
      
    } else {
      raiseErr(__LINE__, "Unsupported Shastina entity on input line %s",
                lineString(snparser_count(pp)));
    }
  }
  
  /* Stack should be empty */
  if (m_stack_height > 0) {
    raiseErr(__LINE__, "Interpreter stack not empty at EOF");
  }
  
  /* Accumulator should be empty */
  if (m_acc_type != TYPE_NULL) {
    raiseErr(__LINE__, "Accumulator not empty at EOF");
  }
  
  /* Nothing should be present in the trailer */
  retval = snsource_consume(pSrc);
  if (retval < 0) {
    if (retval == SNERR_TRAILER) {
      sayWarn(__LINE__,  "Skipped data present in the input trailer");
    } else {
      sayWarn(__LINE__, "Failed to read input trailer");
    }
  }
  
  /* Release the parser */
  snparser_free(pp);
  pp = NULL;
  
  /* Release the input source */
  snsource_free(pSrc);
  pSrc = NULL;
  
  /* Compute registers and dump the register image to standard output */
  computeReg();
  dumpReg();
  
  /* If we got here, return successfully */
  return EXIT_SUCCESS;
}
