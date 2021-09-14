#ifndef OS_H_INCLUDED
#define OS_H_INCLUDED

/*
 * os.h
 * ====
 * 
 * Platform-specific module for Retro.
 * 
 * Compile with the implementation file that is appropriate for the
 * target platform.  See the documentation in the specific platform file
 * for further requirements for compiling on the platform.
 * 
 * In order to port Retro to a new platform, simply create a new
 * implementation of this header file that works on the target platform.
 * 
 * See Porting.md in the doc directory for further information.
 */

/*
 * Return the character code in range [0x21, 0x7e] that is used for
 * separating directories within a path string on this platform.
 * 
 * On POSIX platforms, this is the "/" character, while on Windows
 * platforms, this is the "\" character.
 * 
 * Return:
 * 
 *   the separator character
 */
int os_getsep(void);

/*
 * Return whether the given character code qualifies as a separator
 * character between directories in a path string on this platform.
 * 
 * On POSIX platforms, this only returns non-zero for the "/" character,
 * while on Windows platforms, this returns non-zero for both the "/"
 * and "\" characters.
 * 
 * Parameters:
 * 
 *   c - the character code to check
 * 
 * Return:
 * 
 *   non-zero if given character is separator, zero if not
 */
int os_issep(int c);

/*
 * Check whether a given path refers to an existing directory.
 * 
 * The given path must NOT have a trailing separator or a fault occurs.
 * 
 * Parameters:
 * 
 *   pc - the path to check
 * 
 * Return:
 * 
 *   non-zero if path refers to existing directory, zero if not
 */
int os_isdir(const char *pc);

/*
 * Check whether a given path refers to an existing regular file.
 * 
 * Parameters:
 * 
 *   pc - the path to check
 * 
 * Return:
 * 
 *   non-zero if path refers to existing file, zero if not
 */
int os_isfile(const char *pc);

/*
 * Return a copy of the home directory path.
 * 
 * This path does NOT end with a separator.  On POSIX, this is the
 * contents of the HOME environment variable.  On Windows, the API
 * function SHGetFolderPath() is used with CSIDL_PERSONAL to get the
 * path to the "My Documents" folder.
 * 
 * The returned string is NOT guaranteed to be an existing directory.
 * You should use os_isdir() to check whether it exists.
 * 
 * The dynamically allocated copy of the path can be freed with free().
 * 
 * NULL is returned if the home directory can't be determined.
 * 
 * Return:
 * 
 *   a dynamically allocated copy of the home directory path, or NULL
 */
char *os_gethome(void);

#endif
