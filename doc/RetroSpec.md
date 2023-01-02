# Retro Synthesizer Specification

The Retro synthesizer is based on OPL2 hardware.  OPL2 was the most common hardware for producing video game music in the late 80s and early 90s on the DOS platform, so Retro produces results that sound like they came from a DOS computer game.  OPL2 was used in AdLib and SoundBlaster cards.  AdLib Gold, SoundBlaster Pro 2, and SoundBlaster 16 cards had OPL3 chips, which offer additional capabilities but are also backwards compatible with OPL2.

Retro does not actually generate the synthesized sound samples.  Instead, Retro generates a hardware script file that indicates which values need to be written to which OPL2 hardware registers at what time.  This can be played back using various available OPL2 software emulators and run through those emulators to generate audio files.

Although Retro's generated hardware script file can theoretically be used to drive actual OPL2 hardware, Retro's generated results have not been tested against actual hardware.  In particular, actual OPL2 hardware has limits on how fast you can change hardware registers, while software emulations allow the emulated registers to be changed instantly.  Retro does not take into account the speed limits on changing hardware registers, so Retro may produce hardware scripts that work fine on software-emulated hardware but fail on actual hardware.

## Sampling and control rates

The _sampling rate_ is the constant rate at which digital sound samples are played back.  Retro supports two sampling rates:  44,100 samples per second or 48,000 samples per second.  44,100 samples per second is the standard for CD-quality audio.  48,000 samples per second is the standard for synchronizing audio and video.  Historic sound cards used a variety of sampling rates, usually because the hardware was not fast enough to handle CD-quality audio.  Some Sound Blaster cards were able to handle 44,100 samples per second.  There will probably not be any audible difference between 44,100 samples per second and 48,000 samples per second.

Historic software never specified any sampling rate for OPL2 hardware.  The software just changed the values in the hardware registers, and the hardware immediately changed what it was playing to match the state of the hardware registers.  The digital sampling rate was an internal hardware detail not available to software control.

The _control rate_ is the constant rate at which OPL2 hardware state can be modified.  The control rate is specified by an integer _i_ greater than zero.  If _i_ is one then the control rate is equal to the sampling rate.  If _i_ is two then the control rate is half the sampling rate.  If _i_ is three then the control rate is one third the sampling rate.  And so forth.

Historic software designed for OPL2 hardware used a variety of control rates.  The DOS platform had many different hardware timers that could be used for synchronizing music playback, so the control rate varied depending on what timer hardware the software was using and how the hardware timer was programmed.

For example, the classic id Software title _Doom_ used a control rate of 140 Hz.  You can get this exact control rate in Retro by setting the sampling rate to 44,100 samples per second and the control rate _i_ value to 315.  On the other hand, the classic id Software title _Wolfenstein_ used a control rate of 700 Hz.  You can get this exact control rate in Retro by setting the sampling rate to 44,100 samples per second and the control rate _i_ value to 63.

## Events

The Retro synthesizer compiles a set of _events_ into a hardware script that plays those events back on emulated OPL2 hardware.  Each event is specified independently from all other events.

Each event has an _offset_ and a _duration_ that determines when the event takes place.  The offset and duration are measured in _control units,_ with the number of control units per second determined by the control rate.  For example, with a control rate of 140 Hz, the control unit will be approximately 7.143 milliseconds, since there are 140 such units in a second.

The offset of an event is the time at which the event starts making sound.  The offset is measured as the number of control units that have elapsed since the start of the performance.  This offset must be an integer value that is zero or greater.

The duration of an event is the length in time of the event.  The duration is measured in control units, and it must be an integer that is two or greater.  The event produces sound for all control units except the last.  In the last control unit, the event is silent.  This one unit of silence is necessary so that the OPL2 hardware can determine when to release the note.  The minimum duration of two has the event producing sound for one control unit and silence for one control unit.

There are two basic kinds of events:  _melodic events_ and _rhythm events._  Melodic events use the normal method of synthesis for producing electronic instrument tones at a specific frequency.  Rhythm events use a special method of synthesis for producing five different kinds of rhythm-section effects:  bass drum, snare drum, tom tom, cymbal, and hi hat.  Each of these rhythm-section effects is its own category of event.  The full set of event types is therefore as follows:

- Melodic type
- Rhythm types:
  - Bass drum type
  - Snare drum type
  - Tom tom type
  - Cymbal type
  - Hi hat type

There are strict limits on how many simultaneous events the OPL2 hardware can handle.  At any given point in time, there may be at most one event of each rhythm type.  At any given point in time, there may be at most 9 simultaneous melodic events if no rhythm events are active, or at most 6 simultaneous melodic events if one or more rhythm events are active.

Retro will automatically figure out how to assign events to specific channels on the OPL2 so that the limits on simultaneous events are respected.  If there are too many events and Retro is unable to assign them to channels, Retro will stop on an error.

## Event parameters

Each event has a number of parameters that control the sound the OPL2 hardware produces for the event.  Parameters may change within an event.  For example, the output frequency might smoothly change from one frequency to another, producing a sliding pitch effect.

Melodic events each have their own set of parameters that is completely independent from all other events.

Rhythm events, however, all share a unified rhythm section state.  This is because the OPL2 hardware uses the same components to handle multiple rhythm instruments, so changing a parameter for one rhythm instrument may affect other rhythm instruments.  It therefore makes sense to treat the whole rhythm section with a unified parameter state.

Parameter values for a melodic event or for the rhythm section state are specified with _graphs._  Each graph has a sequence of one or more _nodes._  Each node has an event offset value.  The event offset value is an integer zero or greater that measures the length in time from the start of the event to the node, measured in control units.  However, for the rhythm section state, event offset values measure the length in time from the start of the performance to the node, since the graph is shared across multiple events.  Each graph must have a node at event offset value zero.  The event offset value of each node must be unique.

Apart from the event offset value, each node in a graph has a parameter value, which is a signed integer.  The parameter value indicates the setting the parameter should have at the time the node occurs.  Finally, each node in a graph has a flag indicating whether the node is approached smoothly or abruptly.  The flag is ignored for the first node in a graph.  For subsequent nodes `n`, the flag indicates whether integer values between node `(n - 1)` and node `n` should be linearly interpolated (smooth), or whether the value of node `(n - 1)` should prevail until suddenly changing to the value of node `n` when node `n` arives (abrupt).

### Channels and operators

Event parameters are categorized by whether they apply to a specific _operator_ or whether they apply to a whole _channel._

Melodic events always have two operators and one channel.  Depending on the network parameter in the channel configuration (see later), either the first operator modulates the second operator in FM synthesis style, or the two operators produce sound simultaneously in additive synthesis style.

The rhythm section state has three channels, each of which has two operators, for a total of six operators.  The available documentation is unclear about exactly how these three channels map to the five types of rhythm events, and how the operators work within each channel.  Retro allows the parameters of all three rhythm section channels and all six individual operators to be modified.

### Amplitude parameter

The _amplitude parameter_ is an operator parameter that selects how much energy or loudness the operator has.  There are 64 levels of amplitude, [0, 63].  The lowest level of amplitude is not completely silent.  Amplitude in OPL2 hardware is defined in a nonlinear decibel space, so linear interpolation of amplitude parameters should produce smooth results.

### Frequency parameters

The _frequency parameter_ is a channel parameter that selects the base frequency that is performed for the channel.  OPL2 hardware supports a base frequency range of approximately 0.047 Hz to 6208.431 Hz.

For linear interpolation in frequency domain to sound smooth, it must be performed in logarithmic space.  Retro defines a logarithmic frequency scale in range [-30488, 87336].  To convert from this logarithmic frequency scale to a Hz value, divide the integer value by 10,000 in floating-point space and then take the natural exponent _e_ to the power of this floating-point value.  To convert from a Hz value to this logarithmic frequency scale, take the natural logarithm of the Hz value in floating-point space, multiply by 10,000, floor to an integer value towards negative infinity, and clamp to the range [-30488, 87336].  In short:

    hz = e^(iv / 10000.0)
    iv = max(min(floor(ln(hz) * 10000), 87336), -30488)

Following these formulas, 440 Hz (A4) would result in an integer frequency parameter of 60867 and 466.1638 (Bb4) would result in an integer frequency parameter of 61445.

### Operator scaling parameters

The actual frequency of each operator is the base frequency in Hz defined for the channel multiplied by a frequency scaling value specific to the operator.  The valid range of values is [0, 12], which select the following frequency scaling values:

     Value | Frequency scaling value
    =======+=========================
       0   |           0.5 (!)
       1   |           1.0
       2   |           2.0
       3   |           3.0
       4   |           4.0
       5   |           5.0
       6   |           6.0
       7   |           7.0
       8   |           8.0
       9   |           9.0
      10   |          10.0
      11   |          12.0 (!)
      12   |          15.0 (!)

Each operator has two settings controlling the modulation of its amplitude and frequency, respectively.  These produce vibrato-like effects.  The valid range of values is [0, 2], which have the following interpretations for amplitude and frequency:

     Value | Amplitude  |  Frequency
    =======+============+=============
       0   |   (none)   |   (none)
       1   | +/- 1.0 dB | +/-  7 cents
       2   | +/- 4.8 dB | +/- 14 cents

Operators may be programmed to scale their amplitude to a lower energy level as the frequency of the operator becomes higher.  There are four _register scaling values,_ which indicate how much the amplitude decreases each time the frequency doubles (rises an octave).  The values [0, 3] have the following interpretations:

     Value | Register scaling
    =======+==================
       0   |      (none)
       1   |      1.5 dB
       2   |      3.0 dB
       3   |      6.0 dB

The waveform produced by each operator may also be changed.  All possible waveforms are scaled derivations of a sine wave.  The four possible waveforms are:  _sine,_ _half,_ _absolute,_ and _pulse._  The sine waveform is a normal sine wave.  The half waveform replaces all negative values of the sine wave with zero.  The absolute waveform replaces all negative values of the sine wave with their absolute value.  The pulse waveform is a modification of the absolute waveform that replaces any point on a downward slope with zero.  The valid range of values is [0, 3], which have the following mapping to waveforms:

     Value | Waveform
    =======+==========
       0   |   sine
       1   |   half
       2   | absolute
       3   |  pulse

### Operator envelope parameters

The amplitude of each operator is affected by an Attack-Decay-Sustain-Release (ADSR) envelope.  Each operator has a set of parameters that control the ADSR envelope.

The _sustain enable_ setting determines whether the amplitude is sustained during the sustain phase of the envelope.  It has two values, [0, 1], where 0 disables sustain and 1 enables sustain.  Sustain enable is appropriate for instruments like organs, clarinets, and bowed violins.  Sustain enable should be turned off for effects like pizzicato, where the sound should not be sustained.

The _envelope scaling_ setting determines whether the ADSR envelope is shortened as the frequency rises in pitch.  It has two values, [0, 1], where 0 disables envelope scaling and 1 enables it.

Finally, there are the four main envelope settings for Attack, Decay, Sustain, and Release.  Attack, Decay, and Release are durations which indicate how quickly that portion of the envelope is performed.  Sustain is a scaling value in range [0.0, 1.0] that determines at what level of the attack amplitude the sustain phase holds.  A sustain scaling value of 0.5 means that the sustain level is half the amplitude of the attack, for example.  If you want sustain to be significant, you should probably set the sustain enable.

Each ADSR setting has a range of values [0, 15].  For duration parameters, lower values are faster and higher values are longer.  For the sustain parameter, lower values are softer (closer to 0.0) and higher values are louder (closer to 1.0).  (This numeric scale is the opposite of what is used in OPL2 register values.)

### Channel configuration parameters

The _feedback level_ determines how much of the first operator's output is directed back into itself.  The range of values is [0, 7], with zero being the least feedback and 7 being the most feedback.

The _network_ determines how operators are configured within the channel.  It has two values, [0, 1].  If set to zero, both operators produce sound simultaneously (additive synthesis).  If set to one, the first operator modulates the second operator in FM synthesis style, and the second operator produces sound.

### Parameter names and defaults

This section gives Retro's name for each of the parameters described in the preceding sections along with a default value for each:

    "amp"      - Operator amplitude, default 63.
    "f"        - Channel base frequency, logarithmic space, default 60867.
    "fscale"   - Operator frequency scaling value, default 1.
    "amod"     - Operator amplitude modulation, default 0.
    "fmod"     - Operator frequency modulation (vibrato), default 0.
    "rscale"   - Operator register scaling value, default 0.
    "wave"     - Operator waveform select, default 0.
    "suse"     - Operator sustain enable, default 1.
    "escale"   - Operator envelope scaling, default 0.
    "attack"   - Operator ADSR attack, default 8.
    "decay"    - Operator ADSR decay, default 8.
    "sustain"  - Operator ADSR sustain, default 8.
    "release"  - Operator ADSR release, default 8.
    "feedback" - Channel feedback level, default 0.
    "network"  - Channel network setting, default 1.

## Retro synthesis script

This section describes the format of the script given to Retro for synthesis.

Retro synthesis scripts are in Shastina format.  See [libshastina](https://github.com/canidlogic/libshastina) for a specification and a parsing library.

@@TODO:

## OPL2 output script

This section describes the format of the hardware script file generated as output by Retro.  The hardware script file is intended to be used with software-emulated OPL2 devices rather than actual OPL2 hardware, as explained in the introduction of this specification.

The hardware script file is a US-ASCII text file where line breaks may be either LF or CR+LF.  The script file is parsed line by line.  Trailing whitespace (tabs and spaces) is allowed on each line without any difference in meaning.  Leading whitespace is _not_ allowed.

The first line of the hardware script file must be:

    OPL2 [rate]

The `[rate]` must be either `44100` or `48000`.  It specifies the sampling rate.

Each following line must be blank, a comment, or an instruction.  Blank lines are empty or consist only of tabs and spaces.  Comment lines have an apostrophe `'` as their first character.  Both blank lines and comment lines are ignored.

There are two types of instruction lines:

    r [offset] [value]
    w [count]

The `r` instruction writes `[value]` into the OPL2 hardware register at offset `[offset]`.  Both `[value]` and `[offset]` must be exactly two base-16 digits.  Letter digits may be in either uppercase or lowercase (or both).  Zero padding is used for values that only require a single base-16 digit.

The `w` instruction causes `[count]` samples to be generated and written to output, using the current state of the OPL2 hardware.

At the start of interpretation, no assumptions are made about the initial state of the OPL2 hardware registers.
