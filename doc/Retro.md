# Retro synthesizer specification

Retro is a software synthesizer that is based on Yamaha OPL2 (YM3812) FM synthesis.  This hardware was used on the AdLib and SoundBlaster sound cards, and by extension in many classic DOS games released in the late 1980s and early 1990s.

Later sound cards such as the AdLib Gold, SoundBlaster Pro II, and SoundBlaster 16 used Yamaha OPL3 (YMF262) for FM synthesis.  The OPL3 is backwards compatible with OPL2, so anything that plays on the OPL2 should play on the OPL3 too.

Retro does not directly handle the actual sound synthesis.  Instead, Retro generates the appropriate OPL2 register writes needed to get the OPL2 chip to synthesize the sound.  These register writes can be played back through actual OPL2 hardware, or a software simulator can be used.  The [retro-opl](https://github.com/canidlogic/retro-opl) project is a software simulator for the OPL2 which can turn the register writes into a synthesized WAV file.

It is possible to compose music by directly writing a synthesis script that sets the OPL2 hardware registers.  This gives you maximum flexibility.  However, this is a long and tedious process akin to programming in machine language.  The Retro synthesizer defined by this project adds some layers of abstraction to make music composition more practical, while preserving as much control and flexibility of the hardware as possible.

## Stage and performance

The first abstraction that Retro imposes on the OPL2 hardware is the concept of a _stage_ versus a _performance._  A _stage_ is a specific configuration of the OPL2 hardware registers.  A _performance_ is a set of timed note events that perform the music.

The register settings established by the stage are fixed and do not change throughout the performance.  This is the main limitation of the Retro synthesizer.  The OPL2 is capable of changing these settings during the performance, which allows for complex effects that Retro can not produce on account of its fixed stage.

The only register settings that can be changed during the performance are the frequency setting of each channel, the note-on flag of each channel, and the drum flags when the rhythm section is enabled.

## Stage definition

The stage is defined by the _global set,_ a set of _timbres,_ a set of _instruments,_ a _drum set,_ and an _orchestra._

A stage consists of an orchestra and a global set.

### Global set

The _global set_ defines stage settings that apply globally across all instruments and all channels.

The global set consists of two flags which may be either on or off:

If the _wide AM flag_ is set, then amplitude modulation depth is 4.8 dB.  If it is clear, then amplitude modulation depth is 1 dB.

If the _wide vibrato flag_ is set, then vibrato depth is 14 cents.  If it is clear, then vibrato depth is 7 cents.

The wide AM and wide vibrato flags only apply to timbres that have amplitude modulation and/or vibrato enabled.  The OPL2 is unable to set the AM or vibrato depth for individual timbres.

### Timbres

A _timbre_ defines the settings for the 18 operators of the OPL2.  Note that melodic channels have two operators each, so each melodic channel is a composite of two timbres.

A timbre is composed of the following settings:

The _waveform_ defines the shape of the sound wave.  There are four possible waveforms in the OPL2.  The `wave` shape is a sine wave.  The `half` shape is a sine wave with all negative values flattened to zero.  The `double` shape is the absolute value of a sine wave.  The `pulse` shape is the absolute value of a sine wave, with all sections of the shape that are not ascending flattened to zero.

The _multiplier_ defines the frequency of the operator in relationship to the frequency of the melodic channel.  The multiplier can be 0.5 or any integer value in range 1 to 15 (inclusive), excluding the values 11, 13, and 14.

The _amplitude_ defines the output level of the operator.  The amplitude has two components.  The first is the base amplitude, which is a value in range 0 to 63 (inclusive), which 0 being the least output and 63 being the most.  The second is the attenuation, which controls how much the output of the operator decreases as the frequency octave increases.  The attenuation is a value in range 0 to 3 (inclusive), with zero being no attenuation and 3 being most attenuation.

The _envelope_ defines an ADSR envelope that adjusts the amplitude over the duration of a note.  The attack, decay, and release settings are values in range 0 to 15 (inclusive) where zero is the least amount of time and 15 is the most amount of time.  The sustain setting is a value in range 0 to 15 (inclusive) where zero is the softest sustain and 15 is the loudest sustain.  There is also an attenuation flag, which if set causes the envelope to foreshorten as the pitch increases.  Finally, there is a sustain mode flag, which if set causes the sustain phase to contain indefinitely; otherwise, the sustain begins decaying immediately.

The _modulation_ defines two flags that control amplitude and frequency modulation.  The AM enable flag turns on amplitude modulation.  The vibrato enable flag turns on frequency vibrato.  The depth of amplitude and vibrato is determined by settings in the global set.

### Instruments

An _instrument_ defines a synthetic instrument that can be assigned to a channel.

Each instrument has two _operators:_  an inner operator and an outer operator.  Each operator is configured with a specific timbre.

Each instrument uses one of two _synthesis types._  FM synthesis has the inner operator modulating the outer operator and the outer operator producing the sound.  Additive synthesis has both the inner and outer operators producing sound.

Each instrument has a _feedback intensity._  At a level of zero, there is no feedback.  Levels 1 to 7 (inclusive) add increasing amounts of feedback to the inner operator.

### Drum sets

A _drum set_ defines the rhythm section.

When enabled, the rhythm section occupies three channels and a total of six operators in those three channels.

The first rhythm channel is the _bass channel._  This rhythm channel is only used for the bass drum.

The second rhythm channel is the _beat channel._  This rhythm channel has the snare drum and the hi-hat.  The snare drum is primary and the hi-hat is secondary.

The third rhythm channel is the _splash channel._  This rhythm channel has the tom-tom and the cymbal.  The tom-tom is primary and the cymbal is secondary.

For rhythm channels that have more than one sound within them, the primary sound is what channel-wide settings such as frequency apply to.  Therefore, the bass drum, snare drum, and tom-tom have specific pitches associated with them from their channel, while the hi-hat and cymbal do not have a specific pitch.

The drum set is defined by three instruments, one associated with each of the three rhythm channels.  However, for the second and third rhythm channels, a single instrument definition covers both the primary and secondary sound within that channel.

### Orchestras

An _orchestra_ is a collection of instruments and optionally a drum set.  The orchestra determines how channels map to instruments.

If an orchestra has a drum set, then up to six melodic instruments can be defined in channel range 1 to 6 (inclusive).

If an orchestra does not have a drum set, then up to nine melodic instruments can be defined in channel range 1 to 9 (inclusive).
