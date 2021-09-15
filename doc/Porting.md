# Porting Retro to different platforms

To allow for easy porting, all API calls that are platform specific and not to the standard C library are placed in the `os` module.

There is a single header for the `os` module, named `os.h`.  Each specific platform has its own implementation of this header.  For example, the POSIX implementation has the implementation `os_posix.c`.  Retro should be compiled only with the implementation file that is appropriate for the target platform.
