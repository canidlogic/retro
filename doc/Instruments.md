# Retro instrument architecture

Retro instrument definitions may be either embedded in the synthesizer script or held in external files.  The option to hold instrument definitions in external files allows for easy sharing of instrument definitions across multiple projects.

## Square wave instrument definitions

Square wave instrument definitions (also sometimes called "legacy instrument definitions") are always embedded within the synthesizer script.  These are the instrument definitions that were used with the original beta 0.1.0 version of Retro, and were the only instruments available in that first version.

Square wave instrument definitions use the `instr` operation in the synthesizer script, as demonstrated in the `example.txt` synthesis script.

## Embedded instrument definitions

Embedded instrument definitions are the modern way to embed instruments within synthesizer scripts.  Embedded instrument definitions are able to create any kind of instrument, except for square wave instruments.

To create an embedded instrument definition, use a Shastina string literal with curly braces `{}` and an unsigned numeric decimal prefix identifying the instrument number, which must be one or greater.  The string embedded within the string literal will be interpreted as a separate Shastina script that defines the instrument, and the instrument will then be placed in the indicated instrument register number, overwriting anything that is currently there.

Newly defined instruments will have a default stereo position of center, a default minimum intensity of zero, and a default maximum intensity of 1024/1024.  After the embedded instrument has been defined, you may change the intensity range and stereo position using Retro synthesis operations such as `instr_maxmin` `instr_stereo` and `instr_field`.

Each embedded instrument definition begins with a metacommand signature that identifies the type of instrument.  This allows for embedded instrument definitions to have multiple formats, and forward compatibility with new instrument formats that may be defined in the future.

## External instrument definitions

External instrument definitions hold the instrument definition in a file that is external to the synthesis script.

External instrument definitions use a Shastina string literal with double quotes and an unsigned numeric decimal prefix identifying the instrument number, which must be one or greater.  The double-quoted data is an instrument call number (see below), which tells Retro how to find the instrument definition file.  Retro will load the instrument definition script from the external file, and thereafter the operation works the same way as an embedded instrument definition.  The format used in external instrument definitions is the same as the format used in embedded instrument definitions.

In short, the only differences between embedded and external instrument definitions are as follows:

1. Embedded use `{}` string entity, external use double-quoted.
2. Embedded string data is the instrument, external string data is a call number.
3. External instruments are loaded from external files.

The call numbers used in external instrument string data have a special format that allows Retro to find the external instrument definition file.  A _name element_ is a non-empty string containing only ASCII lowercase letters, decimal digits, and underscore.  A _call number_ is a sequence of one or more name elements, separated from each other by periods.

Retro has a _search chain_ that is a sequence of one or more directories in the local file system.  Retro always initializes this list to contain the `retro_lib` subdirectory of the current working directory, followed by the `retro_lib` subdirectory within the user's home directory.  You can prefix additional directories to this chain by using the `-L` option on the Retro command line followed by a directory to put at the start of the search chain.

Retro searches for instrument files by going through the search chain from first to last element, and returning the first instrument file that matches.  An instrument file matches if its filename equals the last name element in the call number, and any preceding name elements match containing directory names.  The file extension of a Retro instrument must be `.iretro`

For example, supposing the `-L mypath` has been given as a command-line parameter to Retro, the following locations would be searched for the instrument with call number `com.example.new`:

1. `mypath/com/example/new.iretro`
2. `./retro_lib/com/example/new.iretro`
3. `~/retro_lib/com/example/new.iretro`

The first match is chosen as the instrument file.  If there are no matching files, an error occurs.

It is recommended that instrument names begin with a domain name in reverse order, to ensure that there are no instrument name clashes.
