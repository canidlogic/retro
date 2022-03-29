# Retro music synthesizer
A standalone music synthesizer with minimal dependencies, written in C.

The only dependency is [libshastina](https://www.purl.org/canidtech/r/shastina).  Retro compiles with Shastina beta 0.9.3 or compatible.

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
