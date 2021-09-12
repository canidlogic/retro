# Retro music synthesizer
A standalone music synthesizer with minimal dependencies, written in C.

The only dependency is [libshastina](https://www.purl.org/canidtech/r/shastina).  Retro compiles with Shastina beta 0.9.3 or compatiable.  See the documentation in `retro.c` for further information about building.

A full synthesis script is provided in Shastina format on standard input to the `retro` program.  The `retro` program takes a single command-line argument, which is the path to a WAV file to create.  Retro will interpret the synthesis script it reads from standard input and write the synthesized results to the given WAV file path.

See `example.txt` in the `doc` directory for documentation of the Shastina synthesis script format that is provided to Retro as input.  See `cmajor.txt` in the `doc` directory for a synthesis script that renders a C-major scale and chord.

A few test programs are provided.  They all have the prefix `test_` in the main directory.

Currently, Retro only supports square wave sounds.  This gives an effect similar to the classic IBM PC speaker.

## Releases

### Beta 0.1.1

Updated to compile against libshastina 0.9.2+ beta.  Fixed a small error in the Shastina interpretation loop, where the loop didn't end when an error happened.  Renamed the test programs with the `test_` prefixes.  Successfully tested that everything compiles and that the test programs and scripts work.  Filled in this `README.md` file.

### Beta 0.1.0

Complete version of Retro.  This version has already been used successfully on several projects.  Must be compiled against libshastina 0.9.1 beta.

### Alpha versions

Versions 0.0.x are the development alphas.
