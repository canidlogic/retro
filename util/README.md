# Retro utilities

This directory contains some additional utility programs that are not part of the main Retro program.  Currently, this is limited to a few test programs:

- `test_beep.c` tests the square-wave module of Retro.
- `test_fm.c` tests the FM synthesis module of Retro.
- `test_scale.c` generates a full square-wave chromatic scale.

See the documentation at the top of each program source file for further information.

All of these programs require certain modules of Retro.  To compile these programs with the appropriate Retro module, it is recommended that you run the C compiler in the main Retro source directory, add the appropriate source file from this `util` directory, and add a `-I.` option so that headers from the main Retro directory can be included.

For example, to compile `test_beep.c` with `gcc` you can run the following in the main source directory (__not__ in this directory!):

    gcc -o test_beep -I. util/test_beep.c wavwrite.c sqwave.c ttone.c -lm
