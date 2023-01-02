# Retro Synthesizer Specification

The Retro synthesizer is based on OPL2 hardware.  OPL2 was the most common hardware for producing polyphonic video game music in the late 80s and early 90s on the DOS platform, so Retro produces results that sound like they came from a DOS computer game.  OPL2 was used in AdLib and SoundBlaster cards.  AdLib Gold, SoundBlaster Pro 2, and SoundBlaster 16 cards had OPL3 chips, which offer additional capabilities but are also backwards compatible with OPL2.

Retro does not actually generate the synthesized sound samples.  Instead, Retro generates a hardware script file that indicates which values need to be written to which OPL2 hardware registers at what time.  Executing the hardware script against OPL2 hardware will cause the hardware to synthesize the sound.  You can also run the hardware script against various available OPL2 software emulators and directly capture the digital samples without any OPL2 hardware required.

## Sampling and control rates

The _sampling rate_ is the constant rate at which digital sound samples are played back.  Actual OPL2 hardware uses a sampling rate of approximately 49,716 samples per second.  For the most part, however, the OPL2 sampling rate is an internal hardware detail and not visible to software programs.  Software programs change the hardware registers of the OPL2, and the changes to the hardware registers immediately affect the sound the OPL2 produces.  The digital samples produced by the OPL2 hardware are sent directly to a Digital to Analog Converter (DAC) and there is no way for the software to read the digital samples produced by the OPL2.

There is only one case in which the OPL2's internal sampling rate of 49,716 Hz is visible to software.  When setting the frequency of a channel, the formula for computing the frequency number that is written into the hardware registers involves 49,716 as a constant.

Software written for the OPL2 is timed according to a _control rate_ that is completely independent from the sampling rate.  Software uses the control rate to time its writes to the hardware registers of the OPL2.  The OPL2, however, does not care about the control rate.  The OPL2 simply changes its output the moment its hardware registers are changed.  In DOS software, the control rate might be generated by separate timing hardware that is unrelated to the OPL2.

Historic software designed for OPL2 hardware used a wide range of control rates.  For example, Sierra games using the SCI interpreter had a control rate of 60 Hz.  On the other hand, the id Software title _Wolfenstein_ used a control rate of 700 Hz.

Retro allows you choose any control rate that is an integer value in range [1 Hz, 1024 Hz].  On software emulations of OPL2 hardware, you might choose the control rate such that the sampling rate used by the OPL2 software emulation is divisible by the control rate.  This has the useful property that the length of one cycle at the control rate has an integer number of digital samples.  For example, if the OPL2 software emulation has a sampling rate of 44,100 Hz and the control rate is 60 Hz, then each cycle at the control rate will have exactly 735 samples at the sample rate.

For actual OPL2 hardware, there is no reason to synchronize the control rate with the internal sampling rate used by the hardware.  However, actual OPL2 hardware has a limit to how fast you can change the hardware registers.  One byte can be written to OPL2 hardware at a time, and it takes about 3.3 microseconds to write the address and 23 microseconds to write the data byte.  The OPL2 synthesizer state controlled by Retro has 118 bytes, so it takes about 3103.4 microseconds to fully change the synthesizer state on OPL2 hardware.  This means the full synthesizer state can be changed at most about 322 times per second.

Consequently, if you are using Retro to target actual OPL2 hardware, you may want to choose a control rate comfortably below 322 Hz.  If your control rate is close to or above 322 Hz, then it is possible that Retro's hardware script changes hardware registers faster than the OPL2 hardware can handle.  However, you may only hit this limit if you have an unusually complex composition with lots of extremely fast synthesizer state changes.  Testing on actual hardware is the best way to check whether you are outrunning what the OPL2 is capable of handling.

Software emulations of OPL2 hardware can instantly change register states and therefore it is impossible to change software-emulated hardware registers too fast.

## Cycles

Timing within Retro synthesis scripts is measured in _cycles._  The length of a cycle in seconds is derived from the control rate by the following formula:

                           1
    cycle (sec.) = -----------------
                   control rate (Hz)

For example, if the control rate is 60 Hz, then one cycle will be approximately 0.0167 seconds.  Since the range of control rates supported by Retro is [1 Hz, 1024 Hz], the range of cycle units is approximately [0.00098 sec, 1 sec].

## Channels and operators

OPL2 hardware is organized into nine _channels,_ and each channel has two _operators._  Retro always numbers channels and operators with a zero-based count, so the channel numbers are in range [0, 8] and the operator numbers are in range [0, 1].  Note that OPL2 documentation sources might use one-based numbering for channels and operators, in which case you can convert the one-based numbers to Retro's numbering style by subtracting one.

The last three channels (6, 7, 8) are special because they can also be used in _rhythm mode._  When rhythm mode is enabled, channels 6, 7, and 8 have a special, interconnected hardware configuration that allows them to simulate five percussion sounds:  bass drum, snare drum, tom-tom, cymbal, and hi-hat.  When rhythm mode is disabled, channels 6, 7, and 8 behave the same way as the other channels.

The two operators in each channel can be configured in FM synthesis mode or additive synthesis mode.  In FM synthesis mode, operator 0 does not produce any sound but instead modulates operator 1 to achieve complex instrument timbres.  In additive synthesis mode, operators 0 and 1 both produce sound simultaneously.

## Parameters and values

Each channel and each operator has _parameters_ that configure the details of how the channel and the operator work.  _Channel parameters_ are parameters that apply to a whole channel.  Each channel has its own set of channel parameters.  _Operator parameters_ are parameters that apply to a specific operator within a specific channel.  Each operator has its own set of operator parameters.

Retro gives each parameter an ASCII name.  Parameter names always have a length in range [1, 8] and only contain alphabetic letters.  All name characters after the first character are always lowercase.  The first character of a parameter name is uppercase if the parameter is a channel parameter or lowercase if the parameter is an operator parameter.

The value of each parameter is an integer.  For all parameters except the `F` parameter, the full range of allowed values is a subset of the range [0, 63].  The `F` parameter exceptionally has a range of [0, 117824].

## Events

The Retro synthesizer compiles a set of _events_ into a hardware script that plays those events back on emulated OPL2 hardware.  Each event is specified independently from all other events.

Each event has an _offset_ and a _duration_ that determines when the event takes place.  The offset of an event is the time at which the event starts making sound.  The offset is measured as the number of cycles that have elapsed since the start of the performance.  This offset must be an integer value that is zero or greater.

The duration of an event is the length in time of the event.  The duration is measured in cycles, and it must be an integer that is two or greater.  The event produces sound for all cycles except the last.  In the last cycle, the event is silent.  This one cycle of silence is necessary so that the OPL2 hardware can determine when to release the note.  The minimum duration of two has the event producing sound for one cycle and silence for one cycle.

There are two kinds of events:  _melodic events_ and _rhythm events._  Melodic events can be performed on any of the OPL2's channels.  Rhythm events can only be performed when rhythm mode is enabled.  Rhythm events use the special percussion instruments that are synthesized on a combination of channels 6, 7, and 8.

The following subsections describe melodic and rhythmic events in further detail.

### Melodic events

Melodic events must specify a full set of parameter values for one channel and the two operators within that channel.  To avoid giving a long list of parameters for each individual melodic event, melodic events inherit parameter values from an _inheritance chain._  Individual melodic events can then inherit most of their parameter values.  Melodic events only need to specify individual parameter values that are different from what was inherited.

The root of the inheritance chain for every melodic event is a set of _default parameter values._  Each parameter has a constant default value.  The set of default parameter values is a complete mapping from each channel and operator parameter to its default value.

Each melodic event references an _instrument_ that determines its inheritance chain.  Each instrument has a _parent_ that is either a different instrument or a null reference indicating the instrument has no parent.  From each instrument that has a parent reference, it must be possible to reach another instrument that has no parent reference by following the chain of parent references; circular references and self references in derivation chains are not allowed.

Each instrument has three sets of parameter values, each of which may be empty.  The first set can only store channel parameters.  The second and third sets can only store operator parameters.  The first set determines the channel parameters, the second set determines operator zero parameters, and the third set determines operator one parameters.

For instruments that have a null parent reference, the channel parameters are inherited from the subset of default parameter values containing only the channel parameters.  The operator zero and operator one parameters are both inherited from the subset of default parameter values containing only the operator parameters.  The parameter mappings within the first, second, and third sets stored within the instrument override inherited channel, operator zero, and operator one parameter values.

For instruments that have a non-null parent reference, the channel parameters, operator zero parameters, and operator one parameters are inherited from the parent instrument.  The parameter mappings within the first, second, and third sets stored within the instrument override inherited channel, operator zero, and operator one parameter values.

Melodic events then inherit their channel, operator zero, and operator one parameters from the instrument they reference.  The individual melodic events only need to specify those parameters that need to be changed from their inherited values.

In short, melodic events contain an offset and duration measured in cycles, an instrument reference, and three sets of parameter override values for channel parameters, operator zero parameters, and operator one parameters.  Each of the three sets may be empty, indicating that the inherited values should be used as-is.

### Rhythm events

Rhythm events must specify which of the five percussion instruments they are performing:  bass drum, snare drum, tom-tom, cymbal, or hi-hat.  A rhythm event therefore contains an offset and duration measured in cycles, and a selection of one of the five percussion instruments.

Rhythm events do not include parameter values because of the way that the three special rhythm channels 6, 7, and 8 are interconnected.  Changing a single parameter value might affect more than one of the simulated percussion instruments.

Instead, the channel parameter values for channels 6, 7, and 8, and the operator parameter values for each of the six operators within those three channels must be specified at the beginning of any Retro script that includes at least one rhythm event.  This combined state of the three channels and six operators is called the _rhythm section state._  The channel parameters in each channel and the operator parameters within each operator inherit from the same set of default parameter values defined for melodic events, so the rhythm section state only needs to specify changes from these inherited defaults.

Whenever at least one rhythm event is active, Retro will ensure that channels 6, 7, 8, and their operators are loaded with the rhythm section state.

## Capacity limits

There are strict limits on how many simultaneous events the OPL2 hardware can handle.  At any given point in time, there may be at most 9 simultaneous melodic events if no rhythm events are active, or at most 6 simultaneous melodic events if one or more rhythm events are active.

At any given point in time, each percussion instrument can have at most one rhythm event active.  You can use different percussion instruments simultaneously, but you may not use the same percussion instrument more than once at a time.

Retro will automatically figure out how to assign events to specific channels on the OPL2 so that the limits on simultaneous events are respected.

## Graphs

Parameter values within instrument definitions, melodic events, and the rhythm section state definition may either be constant integer values or _graphs._  A graph allows the value of a parameter to change over time.  The same graph object may be shared across multiple parameter values.

There are two kinds of graphs:  _base graphs_ and _derived graphs._  Base graphs are a complete, standalone definition of a graph.  Derived graphs reference another graph and are identical to the referenced graph, except each graph value produced by the referenced graph is adjusted as it passes through the derived graph.

### Base graphs

Base graphs describe a function `f(t)` that takes a time offset `t` as input and produces an integer parameter value `v` as output.  The input value `t` must be an integer that is greater than or equal to zero.  The output value `v` is an integer in 17-bit unsigned integer range [0, 131072].

Base graphs are either _global_ or _local._  In a global graph, `t` value zero corresponds to the start of the performance.  In a local graph, `t` value zero corresponds to the start of the current melodic event.  Instrument definitions and melodic events may use both global and local graphs.  Rhythm section state definitions may only use global graphs.

A global graph is appropriate for effects that span multiple events over time.  For example, a long crescendo can be modeled as a global graph for amplitude.  Each individual event would then have its amplitude value synchronized with all the other events participating in the crescendo effect.

A local graph is appropriate for effects localized to an individual event.  For example, a pitch bend at the start of a note can be modeled as a local graph for frequency.  The same pitch bend graph can be used for multiple events, and each event will then have its own, localized pitch bend.

The graph function `f(t)` is defined by a sequence of _blocks_ and a _sustain value._  Each block has a length in cycles `n` that must be a finite value greater than zero.  The blocks define a finite subdomain of the function starting at `t` zero, with the start of the first block at `t` zero, and the start of each subsequent block immediately after the preceding block.  Therefore, the first block with length `n` defines the graph function for `t` in range `[0, n-1]`, the second block with length `m` defines the graph function for `t` in range `[n, n+m-1]`, and so forth.

The sustain value is a constant integer value that is returned for any `t` value that is outside the subdomain covered by the blocks.  It is possible to have a graph with no blocks, in which case the function returns the sustain value for all values `t`.  In this case, though, it would be more efficient to just use a constant parameter value rather than a graph object.

Retro supports two kinds of blocks:  _planes_ and _ramps._  A plane block simply returns a constant value throughout its subdomain.  A ramp block has different values at the start and end of the block, and uses linear interpolation within the block.

More specifically, ramp blocks have a start value, a goal value, and a step count.  The start value and goal value may both have any 17-bit unsigned integer value.  For the first `t` covered by the ramp block's subdomain, the start value will be returned.  The last `t` covered by the ramp block's subdomain does _not_ reach the goal value, however.  Instead, the remaining `t` values in the ramp block are linearly interpolated such that the `t` value immediately following the ramp block would be the goal value.  This allows ramp blocks to be smoothly connected to each other.

The step count of a ramp block must be an integer value that is one or greater.  With a value of one, linear interpolation happens each cycle.  With a value of two, linear interpolation happens once every two cycles, and each pair of cycles repeats the same linear interpolation value.  With a value of three, linear interpolation happens once every three cycles, and each group of three cycles repeats the same linear interpolation value.

The step count is intended for ramps that have a long duration, such as might occur for an amplitude ramp on a long crescendo.  Increasing the step count decreases the number of times the parameter value changes during the ramp, which decreases the number of hardware registers writes that need to be made.

Ramp blocks that have the same start and goal value are automatically replaced by plane blocks with that constant value.  Ramp blocks that have a step count greater than or equal to the length of the ramp block are automatically replaced by plane blocks with the start value.

### Derived graphs

Derived graphs must reference an existing base or derived graph.  (No circular or self references are allowed.)  Derived graphs are exactly the same as the graph they reference, except that each value `v` generated by the referenced graph is changed to a different value `w` when it passes through the derived graph according to the following formula:

    w = max(min(floor((s * v) / d) + p, b), a)

First, the integer value `s` is multiplied to the input integer value.  Second, the result of this is divided by the integer value `d`.  Third, the result of that is added to `p`.  Finally, the result is clamped to the integer range `[a, b]`.  `s` must be in range [0, 32767], `d` must be in range [1, 32767], `p` must be in range [-117824, 117824], and `a` and `b` must be in range [0, 117824].

## OPL2 parameters

This section documents all the OPL2 channel and operator parameters, along with their default values.

Amplitude and frequency parameters:

    "amp" - Operator amplitude, default 63.
    "F"   - Channel base frequency (logarithmic), default 91355.

Operator scaling parameters:

    "fscale" - Operator frequency scaling value, default 1.
    "amod"   - Operator amplitude modulation, default 0.
    "fmod"   - Operator frequency modulation (vibrato), default 0.
    "rscale" - Operator register scaling value, default 0.
    "wave"   - Operator waveform select, default 0.

Operator envelope parameters:

    "suse"    - Operator sustain enable, default 1.
    "escale"  - Operator envelope scaling, default 0.
    "attack"  - Operator ADSR attack, default 8.
    "decay"   - Operator ADSR decay, default 8.
    "sustain" - Operator ADSR sustain, default 8.
    "release" - Operator ADSR release, default 8.

Channel configuration parameters:

    "Feedback" - Channel feedback level, default 0.
    "Network"  - Channel network setting, default 1.

The following subsections discuss these parameters in further detail.

### Amplitude parameter

The _amplitude parameter_ (`amp`) is an operator parameter that selects how much energy or loudness the operator has.  There are 64 levels of amplitude, [0, 63].  The lowest level of amplitude is not completely silent.  Amplitude in OPL2 hardware is defined in a nonlinear decibel space, so linear interpolation of amplitude parameters should produce smooth results.

### Frequency parameters

The _frequency parameter_ (`F`) is a channel parameter that selects the base frequency that is performed for the channel.  OPL2 hardware supports a base frequency range of approximately 0.047 Hz to 6208.431 Hz.  In order to select the frequency, OPL2 uses an integer `f_num` and an integer `block` number, with the following formula:

    frequency = (f_num * 49716) / 2^(20 - block)

Retro automatically handles computing the `f_num` and the `block` number.

For linear interpolation in frequency domain to sound smooth, it must be performed in logarithmic space.  Retro defines a logarithmic frequency scale in range [0, 117824].  To convert from this logarithmic frequency scale to a Hz value, subtract 30488 from the integer value, divide by 10,000 in floating-point space and then take the natural exponent _e_ to the power of this floating-point value.  To convert from a Hz value to this logarithmic frequency scale, take the natural logarithm of the Hz value in floating-point space, multiply by 10,000, floor to an integer value towards negative infinity, add 30488, and clamp to the range [0, 117824].  In short:

    hz = e^((iv - 30488) / 10000.0)
    iv = max(min(floor(ln(hz) * 10000) + 30488, 117824), 0)

Following these formulas, 440 Hz (A4) would result in an integer frequency parameter of 91355 and 466.1638 (Bb4) would result in an integer frequency parameter of 91933.

### Operator scaling parameters

The actual frequency of each operator is the base frequency in Hz defined for the channel multiplied by a frequency scaling value (`fscale`) specific to the operator.  The valid range of values is [0, 12], which select the following frequency scaling values:

     Value |  fscale
    =======+==========
       0   |  0.5 (!)
       1   |  1.0
       2   |  2.0
       3   |  3.0
       4   |  4.0
       5   |  5.0
       6   |  6.0
       7   |  7.0
       8   |  8.0
       9   |  9.0
      10   | 10.0
      11   | 12.0 (!)
      12   | 15.0 (!)

Each operator has two settings controlling the modulation of its amplitude (`amod`) and frequency (`fmod`), respectively.  These produce vibrato-like effects.  The valid range of values is [0, 2], which have the following interpretations for amplitude and frequency:

     Value |    amod    |    fmod
    =======+============+=============
       0   |   (none)   |   (none)
       1   | +/- 1.0 dB | +/-  7 cents
       2   | +/- 4.8 dB | +/- 14 cents

Operators may be programmed to scale their amplitude to a lower energy level as the frequency of the operator becomes higher.  There are four _register scaling values,_ (`rscale`) which indicate how much the amplitude decreases each time the frequency doubles (rises an octave).  The values [0, 3] have the following interpretations:

     Value | rscale
    =======+========
       0   | (none)
       1   | 1.5 dB
       2   | 3.0 dB
       3   | 6.0 dB

The waveform produced by each operator may also be changed by the `wave` parameter.  All possible waveforms are scaled derivations of a sine wave.  The four possible waveforms are:  _sine,_ _half,_ _absolute,_ and _pulse._  The sine waveform is a normal sine wave.  The half waveform replaces all negative values of the sine wave with zero.  The absolute waveform replaces all negative values of the sine wave with their absolute value.  The pulse waveform is a modification of the absolute waveform that replaces any point on a downward slope with zero.  The valid range of values is [0, 3], which have the following mapping to waveforms:

     Value |   wave
    =======+==========
       0   |   sine
       1   |   half
       2   | absolute
       3   |  pulse

### Operator envelope parameters

The amplitude of each operator is affected by an Attack-Decay-Sustain-Release (ADSR) envelope.  Each operator has a set of parameters that control the ADSR envelope.

The _sustain enable_ setting (`suse`) determines whether the amplitude is sustained during the sustain phase of the envelope.  It has two values, [0, 1], where 0 disables sustain and 1 enables sustain.  Sustain enable is appropriate for instruments like organs, clarinets, and bowed violins.  Sustain enable should be turned off for effects like pizzicato, where the sound should not be sustained.

The _envelope scaling_ setting (`escale`) determines whether the ADSR envelope is shortened as the frequency rises in pitch.  It has two values, [0, 1], where 0 disables envelope scaling and 1 enables it.

Finally, there are the four main envelope settings:  `attack`, `decay`, `sustain`, and `release`.  Attack, Decay, and Release are durations which indicate how quickly that portion of the envelope is performed.  Sustain is a scaling value in range [0.0, 1.0] that determines at what level of the attack amplitude the sustain phase holds.  A sustain scaling value of 0.5 means that the sustain level is half the amplitude of the attack, for example.  If you want sustain to be significant, you should probably set the sustain enable.

Each ADSR setting has a range of values [0, 15].  For duration parameters, lower values are faster and higher values are longer.  For the sustain parameter, lower values are softer (closer to 0.0) and higher values are louder (closer to 1.0).  (This numeric scale is the opposite of what is used in OPL2 register values.)

### Channel configuration parameters

The _feedback level_ (`Feedback`) determines how much of the first operator's output is directed back into itself.  The range of values is [0, 7], with zero being the least feedback and 7 being the most feedback.

The _network_ (`Network`) determines how operators are configured within the channel.  It has two values, [0, 1].  If set to zero, both operators produce sound simultaneously (additive synthesis).  If set to one, the first operator modulates the second operator in FM synthesis style, and the second operator produces sound.

## Retro synthesis script

This section describes the format of the script given to Retro for synthesis.

Retro synthesis scripts are in Shastina format.  See [libshastina](https://github.com/canidlogic/libshastina) for a specification and a parsing library.

@@TODO:

## OPL2 output script

This section describes the format of the hardware script file generated as output by Retro.

The hardware script file is a US-ASCII text file where line breaks may be either LF or CR+LF.  The script file is parsed line by line.  Trailing whitespace (tabs and spaces) is allowed on each line without any difference in meaning.  Leading whitespace is _not_ allowed.

The first line of the hardware script file must be:

    OPL2 [rate]

The `[rate]` is the control rate, in Hz.  It must be an integer in range [1, 1024].

Each following line must be blank, a comment, or an instruction.  Blank lines are empty or consist only of tabs and spaces.  Comment lines have an apostrophe `'` as their first character.  Both blank lines and comment lines are ignored.

There are two types of instruction lines:

    r [offset] [value]
    w [count]

The `r` instruction immediately writes `[value]` into the OPL2 hardware register at offset `[offset]`.  Both `[value]` and `[offset]` must be exactly two base-16 digits.  Letter digits may be in either uppercase or lowercase (or both).  Zero padding is used for values that only require a single base-16 digit.

The `w` instruction waits `[count]` cycles at the control rate.  The OPL2 will produce sound using the current state of the hardware registers while this instruction is waiting.  `[count]` must be an unsigned decimal integer that is in range [0, 2147483647].

At the start of interpretation, no assumptions are made about the initial state of the OPL2 hardware registers.