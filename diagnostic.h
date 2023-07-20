#ifndef DIAGNOSTIC_H_INCLUDED
#define DIAGNOSTIC_H_INCLUDED

/*
 * diagnostic.h
 * ============
 * 
 * Error and warning handling module.
 * 
 * Dependencies:
 *   (none)
 * 
 * Use diagnostic_startup() at the start of the program to set the
 * executable module name for use in diagnostic messages and also to
 * check the parameters passed to the main() function.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Public functions
 * ================
 */

/* 
 * Set the executable module name for use in diagnostic messages.
 * 
 * The given parameter should be equal to the argv[0] value passed to 
 * the program entrypoint.  It may be NULL, in which case no module name
 * will be reported.
 * 
 * The diagnostic_startup() function is a wrapper around this function
 * that is easier to use in most cases.
 * 
 * Parameters:
 * 
 *   pName - the executable module name for diagnostics, or NULL
 */
void diagnostic_set_module(const char *pName);

/*
 * Global function for handling errors and warnings that occur within
 * any module.
 * 
 * err is non-zero if this is an error, zero if this is a warning.
 * Errors will stop the program, warnings will not.
 * 
 * pSource is the source file from which the error or warning
 * originated.  Use the predefined __FILE__ macro for this.  It will
 * only be reported if it has a value that is not NULL.
 * 
 * lnum is the line number of the error or warning within the source
 * file.  Use the predefined __LINE__ macro for this.  It will only be
 * reported if it has a value that is greater than zero.
 * 
 * pDetail is an optional detail message, or NULL if this is a generic
 * error or warning without a detail message.  If a detail message is
 * provided, it should be a printf-style format string.
 * 
 * ap is used to pass any arguments that the format message used for the
 * detail string may take.  These arguments are ignored if pDetail is
 * NULL.  Otherwise, they will be passed through along with the detail
 * format string to the standard library vfprintf() function in order to
 * print them.  This function does not call va_end.
 * 
 * Each module should define its own private wrapper functions that
 * invoke this global function, setting the appropriate __FILE__
 * parameter.  It should look like this:
 * 
 *   #include <stdarg.h>
 *   #include "diagnostic.h"
 *   
 *   static void raiseErr(int lnum, const char *pDetail, ...) {
 *     va_list ap;
 *     va_start(ap, pDetail);
 *     diagnostic_global(1, __FILE__, lnum, pDetail, ap);
 *     va_end(ap);
 *   }
 *   
 *   static void sayWarn(int lnum, const char *pDetail, ...) {
 *     va_list ap;
 *     va_start(ap, pDetail);
 *     diagnostic_global(0, __FILE__, lnum, pDetail, ap);
 *     va_end(ap);
 *   }
 * 
 * Then, within the module, you could raise an error like this:
 * 
 *   raiseErr(__LINE__, "Can't open file: %s", pPath);
 * 
 * This global handler begins by printing a diagnostic message to
 * standard error.  If diagnostic_set_module() was used to set the
 * executable module name, the message begins with the executable module
 * name, followed by a colon and a space.  Next, the message identifies
 * whether this is an error or a warning and then provides any source
 * file and source line information that was provided.  Then, if a
 * detail message was provided, it is printed after a space, using the
 * given variable argument list.  Finally, a line break is printed to
 * complete the line.
 * 
 * A full warning message looks like this:
 * 
 *   program_name: [Warning in "source.c" at line 25] Detail message
 * 
 * If a warning is reported, then the function returns after printing
 * the diagnostic message.  If an error was reported, then the function
 * will call exit() to stop the process with EXIT_FAILURE, and this
 * function will not return.
 * 
 * Parameters:
 * 
 *   err - non-zero if error, zero if warning
 * 
 *   pSource - the source file, call with __FILE__
 * 
 *   lnum - the source line, call with __LINE__
 * 
 *   pDetail - the detail format message, or NULL
 * 
 *   ap - arguments used in the detail format message
 */
void diagnostic_global(
          int       err,
    const char    * pSource,
          int       lnum,
    const char    * pDetail,
          va_list   ap);

/*
 * Set the module name and check program arguments at the start of the
 * program.
 * 
 * Call this function at the start of the main() entrypoint and it will
 * automatically perform diagnostic_set_module() and also check the argc
 * and argv parameters.
 * 
 * Pass the argc and the argv parameters through that were passed to
 * main().  Neither argument is modified by this function.
 * 
 * pDefault is the default name to use for diagnostic_set_module() if
 * the executable name can not be determined from argv[0].  This should
 * just be the usual name of the program.  It should be a string
 * constant or statically allocated so that it remains present for the
 * entire process.
 * 
 * argc will be checked to be zero or greater.  If argc is greater than
 * zero, argv will be checked to be non-NULL.  Furthermore, if argc is
 * greater than zero, a check will be made that each of the arguments in
 * the argv[] array is not a NULL pointer and that after each of the
 * arguments is a terminating NULL pointer.
 * 
 * Parameters:
 * 
 *   argc - the argc parameter passed to main()
 * 
 *   argv - the argv parameter passed to main()
 * 
 *   pDefault - the default executable name if argv[0] is unavailable
 */
void diagnostic_startup(int argc, char *argv[], const char *pDefault);

/*
 * Write a log message to standard error.
 * 
 * If the executable module name is known, it will be prefixed to the
 * log message.  A line break will be appended to the log message.
 * 
 * pFormat is the log message, which is a printf-style format string.
 * The variable argument list is passed afterwards.
 * 
 * Parameters:
 * 
 *   pFormat - the format string for the log message
 * 
 *   [variable] - the arguments to the format string
 */
void diagnostic_log(const char *pFormat, ...);

/*
 * Equivalent to diagnostic_log() except takes a va_list instead of a
 * variable argument list.  Does not call va_end().
 */
void diagnostic_vlog(const char *pFormat, va_list ap);

#endif
