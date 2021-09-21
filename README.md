# Retro music synthesizer
A standalone music synthesizer with minimal dependencies, written in C.

The only dependency is [libshastina](https://www.purl.org/canidtech/r/shastina).  Retro compiles with Shastina beta 0.9.3 or compatiable.

See the documentation on top of the `retro.c` source file for further information about the program.  The `doc` directory also contains various documentation files; see the `README.md` file in that directory for an index.

Retro has a square-wave module that gives an effect similar to the classic IBM PC speaker, and it also has an FM synthesis module.

## Operation

The `retro` program has the following syntax:

    retro output.wav < input.retro

It takes a single command-line argument, which specifies the path to the output WAV audio file to generate.  It reads a synthesis script from standard input.  The `example.txt` document in the `doc` directory gives the format of the synthesis script.

If you have a self-contained synthesis script that includes all its instrument definitions, this is all there is to the program interface.  However, if you are using external instrument definitions, then `retro` has to locate external files that contain instrument definitions.

By default, `retro` will search for instrument definitions in the `retro_lib` folder of the current working directory and the user home directory.  You can add additional folders to the search path like this:

    retro -L instrument/folder output.wav < input.retro

See `Instruments.md` in the `doc` directory for further information about the instrument architecture.

## Compilation

See the "Compilation" section in the `retro.c` source file documentation near the top for specifics.  An example `gcc` build line is as follows (everything should be on a single command line with no line breaks):

    gcc -O2 -o retro
      -I/path/to/shastina/include
      -L/path/to/shastina/lib
      retro.c
      adsr.c
      generator.c
      genmap.c
      graph.c
      instr.c
      layer.c
      os_posix.c
      sbuf.c
      seq.c
      sqwave.c
      stereo.c
      ttone.c
      wavwrite.c
      -lshastina
      -lm

The `-I` and `-L` options indicate the directories holding the `shastina.h` and `libshastina.a` files, respectively.  Alternatively, you can copy `shastina.h` and `shastina.c` into this program directory and then use the following invocation:

    gcc -O2 -o retro
      retro.c
      adsr.c
      generator.c
      genmap.c
      graph.c
      instr.c
      layer.c
      os_posix.c
      sbuf.c
      seq.c
      sqwave.c
      stereo.c
      ttone.c
      wavwrite.c
      shastina.c
      -lm

The above will only work after the Shastina sources have been copied into this directory.

## Releases

### Beta 0.2.1

Optimized the FM generator module so that it uses a sine wave lookup table instead of directly computing the sine function each sample.  Real-world tests indicate a substantial performance boost by doing this, without any change in audio quality.  Also optimized main retro module so that it only initializes the square wave module if it is actually needed.  Clarified the documentation about the `%sqamp` configuration command.  Finally, adjusted documentation to indicate that square wave instruments are not deprecated, but rather are a separate instrument class from the FM module.

### Beta 0.2.0

Added a modular instrument definition system that supports FM synthesis.  This is a major overhaul, and yet it is still backwards compatible with the previous betas.  Retro synthesis scripts designed for previous betas should still run equivalently on this beta.

### Beta 0.1.1

Updated to compile against libshastina 0.9.2+ beta.  Fixed a small error in the Shastina interpretation loop, where the loop didn't end when an error happened.  Renamed the test programs with the `test_` prefixes.  Successfully tested that everything compiles and that the test programs and scripts work.  Filled in this `README.md` file.

### Beta 0.1.0

Complete version of Retro.  This version has already been used successfully on several projects.  Must be compiled against libshastina 0.9.1 beta.

### Alpha versions

Versions 0.0.x are the development alphas.
