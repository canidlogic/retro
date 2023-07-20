# Retro Performance File (RPF) format

Retro Performance File (RPF) stores the note events that are played back during the performance.

## Text format

RPF is a plain-text file format in ASCII character set.  It is processed line by line.  Line breaks may be LF or CR+LF.

Each line is automatically trimmed of trailing spaces and tabs.  However, leading spaces and tabs are _not_ automatically trimmed.  Sequences of one or more spaces and tabs are collapsed into single space characters within the line.

All ASCII letters within a line are automatically normalized to uppercase, so the file format is case-insensitive throughout.

Comment lines are ignored and may contain other character data besides ASCII.  However, a UTF-8 Byte Order Mark (BOM) must _not_ be present at the start of the file.

## Frequency format

RPF uses a special _frequency format_ to describe pitch settings.  The frequency format looks like this:

    4-16B

No space or tab may be present around the hyphen.  The first character must be a decimal digit in range 0 to 7 (inclusive), choosing the octave.  The last three characters must be base-16 digits in range 000 to 3FF (inclusive), choosing the F-number.

The octave and F-number determine the frequency of Hz of the pitch according to the following format:

                F-number x 49716
    frequency = ---------------- Hz
                2^(20 - octave)

## Header line

The first line in an RPF must be a header line.  For _melodic performances_ that use up to nine melodic channels and no rhythm instruments, the header line looks like this:

    RPF 60 M

The number is the control rate, in Hz.  It must be in range 1 to 1024, inclusive.

For _rhythm performances_ that use up to six melodic channels along with rhythm instruments, the header line looks like this:

    RPF 60 R B=? S=3-2AE T=4-202

The parameters `B`, `S`, and `T` must all be present and appear in the order shown above.  No spaces or tabs are allowed to around the equals signs.

These three parameters give the default frequency settings for the bass drum, snare drum, and tom-tom.  These are either a pitch setting in frequency format, or they are the special value `?` that indicates there is no default.

## Blank and comment lines

After the header line, any line that is empty or contains only spaces and tabs is a _blank line._  Blank lines are ignored.

After the header line, any line that begins with an ASCII apostrophe `'` as the first character (not preceded by any space or tab) is a _comment line._  Comment lines are also ignored.  Comment lines may have any kind of character data after the apostrophe.

## Event lines

After the header line, any line that is neither blank nor a comment must be an _event line._  Each event line defines a single event of the performance.  The following subsections define each of the event types.

Events can be in any order whatsoever.  The Retro compiler will automatically sequence the events in proper order.  An error occurs if a single melodic channel has more than one event active at any point in time, or if any specific rhythm instrument has more than one event active at any point.

### Null events

_Null events_ exist at a particular time point but do not produce any sound.  The duration of a null event is always one single control unit.

The performance lasts until all defined events complete.  If you want an interval of silence at the end of the performance, you can define a null event at the last control unit that should be part of the performance.  Without null events, there would be no way to have silence at the end of the performance.

A null event looks like this:

    N 1095

1095 is the _time offset_ of the null event, where zero is the start of the performance.  The time offset is measured in time units at the control rate, which was established by the header line.  The time offset is an unsigned decimal integer.

The duration of a null event is always one.

Apart from possibly extending the length of a performance, null events have no other effect.  There may be multiple null events in a single performance.

### Melodic events

_Melodic events_ define all notes that do not involve rhythm instruments.  A melodic event looks like this:

    529:20 1 4-16B

529 is the _time offset_ that the melodic event starts, where zero is the start of the performance.  The time offset is measured in time units at the control rate, which was established by the header line.  The time offset is an unsigned decimal integer.

20 is the _duration_ of the melodic event, measured in time units at the control rate.  The event will be active for all time units in the duration except for the last.  The duration must be at least two, to allow for at least one active control cycle and one release control cycle.  The duration is an unsigned decimal integer.

No spaces or tabs are allowed to surround the colon between the time offset and the duration.

1 is the _channel_ the melodic event is assigned to.  For melodic performances, it must be in range 1 to 9 (inclusive).  For rhythm performances, it must be in range 1 to 6 (inclusive).

4-16B is the _pitch_ of the melodic event.  This must be in frequency format.

### Rhythm events

_Rhythm events_ define all notes involving rhythm instruments.  They may only be present in rhythm performances.

Rhythm events that include a pitch look like this:

    529:20 B 4-16B

This format is the same as melodic event format, except instead of a channel number there is a letter `B` for bass drum, `S` for snare drum or `T` for tom-tom.

Rhythm events that do not include a pitch look like this:

    529:20 C

This format is the same as the previous rhythm event format, except it lacks the pitch information.  In addition to the `B` (bass drum), `S` (snare drum) and `T` (tom-tom), events of this type can also specify `H` for hi-hat or `C` for cymbal.

The bass drum, snare drum, and tom-tom may only use the event format without a pitch if a default pitch was defined for them in the header line.

The hi-hat and cymbal may never specify a pitch because those rhythm instruments do not have an associated pitch.
