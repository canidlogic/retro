# Retro MIDI format

Retro accepts performances in both its native RPF format and in MIDI format.  However, Retro is not able to support General MIDI due to limitations of the OPL2 hardware.  Instead, Retro supports only a subset of MIDI.  This document describes the MIDI subset that can be used with Retro.

Internally, Retro will convert MIDI to RPF before compilation.  There is some configuration information that Retro requires to know how to correctly perform the conversion.  This configuration information is also described in this document.

## MIDI file formats

Standard MIDI files come in three formats:  Format 0, Format 1, and Format 2.  Format 0 stores a linear performance in a single track.  Format 1 stores a linear performance in multiple tracks that occur simultaneously.  Format 2 is a non-linear format that stores independent sequences in separate tracks.

Retro only supports linear performances, so it will refuse to process standard MIDI files in format 2.

When Retro encounters a Format 1 MIDI file, it will automatically transform the data into Format 0 while loading it by flattening all tracks into a single track.  The flattening process works like this:

(1) The time length of each track is extended if necessary to match the track with the longest time length in the file.

(2) All events are combined into a single track in chronological order.

(3) When different tracks have events occuring at the exact same time, the chronological order in the flattened track will have events from lower-numbered tracks occuring before events from higher-numbered tracks.

(4) Only a single End Of Track meta-event will be included in the flattened track, with extra End Of Track meta-events from other tracks discarded.

Finally, Retro supports directly loading Format 0 MIDI files without any transformation needed.  If you have a choice, Format 0 is Retro's preferred format.

## Timing systems

Standard MIDI files support three different timing systems:  variable, fixed, and drop-frame.

In _variable timing,_ events are timed according to rhythmic values such as quarter notes and eighth notes.  The duration of these rhythmic values depends on a specific tempo.  Set Tempo meta-events within the track establish the tempo, which then determines the physical timing of the events.

The tempo may change multiple times within the performance, so the duration of rhythmic units is not constant across the performance.  The default tempo at the start of the performance is 120 quarter notes per minute, which remains in effect until changed with a Set Tempo meta-event.

In _fixed timing,_ events are timed according to fixed time units which remain constant throughout the performance.  (MIDI documentation calls this "SMPTE" timing.)  The slowest fixed time unit that can be specified is at a rate of 24 Hz, and the fastest is at a rate of 7650 Hz.

In _drop-frame timing,_ events are timed according to NTSC frames with drop-frame adjustments.  (MIDI documentation calls this SMPTE timing with the special time code format of -29.)  Drop-frame timing is described in the subsection below.

### Drop-frame timing

In drop-frame timing, events are timed by _frames._  Frames in drop-frame timing occur at a rate of 30 Hz.  However, at regular intervals, certain frames are delayed by an integer number of frame lengths.  The delays are designed so that the average frame rate over time is almost exactly the NTSC rate of:

        1000
   30 x ---- Hz (= approx. 29.97 Hz)
        1001

More specifically, let _w_ be the time offset of an event, measured in frames of drop-frame time.  We want to compute the equivalent time offset _t_ of the event, measured in seconds.  We first decompose _w_ into an integer frame offset and an offset within that frame, as follows:

    w = v + z
    where:
      w >= 0.0,
      v is an integer,
      v >= 0,
      0.0 <= z < 1.0

We can then express the time offset in seconds _t_ as follows:

    t = (v + D(v) + z) * F
    where:
      D(v) is the delay function,
      F = (1 / 30) seconds

The _delay function_ takes an integer frame offset that is zero or greater and returns an integer zero or greater that indicates how many cumulative frame-lengths of delay have occurred up to that frame.

To define the delay function for a value _v_ first decompose _v_ into blocks, minutes, and remainder as follows:

    v = (b * 18000) + (m * 1800) + r
    where:
      b, m, r are integers,
      b >= 0,
      0 <= m < 10,
      0 <= r < 1800

The delay function can then be defined as:

    D(v) = (b * 18) + (max(0, m - 1) * 2)

This delay function results in two frame-lengths of delay inserted at the start of every minute, except for the first minute of each 10-minute block.

Suppose that you want a MIDI file to be synchronized with the frames of a video running at the NTSC rate of approximately 29.97 Hz.  You can do this by pretending that the video is running at exactly 30 Hz and composing the MIDI file according to that fixed rate.  Then, simply change the timing system declared in the file header from a fixed rate that is some multiple of 30 Hz to drop-frame timing using the same multiple.  The resulting performance will be synchronized with the video, without having to adjust any of the individual event timings.

Note that the name "drop-frame" is misleading because no frames of information are actually dropped.

## Time mapping algorithms

In order to convert the MIDI timing information into RPF timing, a _time mapping algorithm_ is used.  There are two time mapping algorithms:  exact and floor.

### Exact time mapping

The _exact_ time mapping algorithm exactly preserves the timing in the MIDI file.  However, it can only be used on MIDI files that satisfy certain conditions.

The exact algorithm requires the MIDI file to use fixed timing or drop-frame timing, and it requires that the product of the division multiple and the frame rate not exceed 1024.  (For drop-frame timing, a frame rate of 30 is used for this calculation.)

The exact algorithm then directly maps all MIDI time offsets at fixed rates to equal RPF time offsets and sets the RPF control rate to the product of the MIDI frame rate and the multiplier.

For drop-frame timing, let _M_ be the multiplier of the frame rate declared in the MIDI header.  The RPF control rate is then set to 30 times _M_.  Given a MIDI time offset _v_ the RPF time offset is first computed by splitting the time offset into a frame offset and a remainder like this:

    v = (f * 30 * M) + r
    where:
      f, r are integers
      f >= 0
      0 <= r < 30 * M

The RPF time offset _t_ can then be computed as follows:

    t = ((f + D(f)) * 30 * M) + r
    where:
      D(f) is the delay function

See the "Drop-frame timing" subsection for the definition of the delay function.

### Floor time mapping

The _floor_ time mapping algorithm approximates the timing in the MIDI file.  It is not as precise as exact time mapping, but it can be used with all supported timing systems.

The floor algorithm requires the desired RPF control rate to be specified.  The time offset of each MIDI event is then computed as a floating-point time offset in seconds.  Next, the time offset is rounded down to the control rate time grid using the `floor()` function.  Finally, the durations of all resulting notes are adjusted if necessary so that they are at least a duration of 2 control units, which is required by RPF files.

The rounding of the floor algorithm may cause events to overlap that did not overlap in the original MIDI file.  This can be avoided by using a higher frequency RPF control rate.

For MIDI files using fixed timing, it is straightforward to compute the floating-point time offset in seconds.

For MIDI files using drop-frame timing, the conversion into a floating-point time offset in seconds is described in the earlier "Drop-frame timing" subsection.

For MIDI files using variable timing, the first step is to build a section map.  The section map is initialized with a single node at time offset zero that has a tempo setting equivalent to 120 quarter notes per minute.  The section map is then filled in by going through all MIDI events in chronological order and recording each Set Tempo event in the section map.  If multiple Set Tempo events occur at the same time offset, later Set Tempo events overwrite previous ones.

Once the section map has been filled in, the starting time offset of each section is determined.  The time offset of each section is computed as follows:

    offset(0) = 0
    offset(n) = offset(n - 1) + ((s[n] - s[n-1]) * q[n-1])
    where:
      n > 0,
      s[n] is MIDI variable offset of section n
      q[n] is duration of MIDI variable unit in section n

After all the starting time offsets of the section table are determined, a MIDI time offset _v_ can be converted into an absolute time offset _t_ in seconds by finding the greatest integer _i_ where the MIDI variable section offset `s[i]` is less than or equal to _v_ and then using the following formula:

    t = offset(i) + ((v - s[i]) * q[i])
    where:
      s[i] is MIDI variable offset of section i
      q[i] is duration of MIDI variable unit in section i

