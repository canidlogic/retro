package Retro::ChannelRegisters;
use v5.14;
use warnings;

use Retro::Util qw(
  rtIsInteger
);

=head1 NAME

Retro::ChannelRegisters - OPL2 channel register state representation.

=head1 SYNOPSIS

  use Retro::ChannelRegisters;
  use Retro::ChannelState;
  use Retro::Assembler;
  
  # Create a new ChannelRegisters instance with everything zero
  my $regs = Retro::ChannelRegisters->create;
  
  # Set channel registers according to a given ChannelState and a flag
  # indicating the state of the key-down flag
  my $cs = Retro::ChannelState->create;
  my $key_down = 1;
  $regs->set($cs, $key_down);
  
  # Compute the cost of going from a given register state to this one
  my $other_regs = Retro::ChannelRegisters->create;
  my $cost = $regs->cost($other_regs);
  
  # Compute the cost of going from an undefined register state to this
  # one
  my $cost = $regs->cost(undef);
  
  # Output assembly instructions at time $t in channel $c that change
  # from the given current register state to this one
  my $asm = Retro::Assembler->create(60);
  $regs->assemble($asm, $t, $c, $other_regs);
  
  # Output assembly instructions at time $t in channel $c that change
  # from an undefined register state to this one
  $regs->assemble($asm, $t, $c, undef);

=head1 DESCRIPTION

Represents a snapshot of all the register byte values for a specific
OPL2 channel.

After construction, each channel hardware register tracked by this
object will be set to zero.  Use the C<set()> function to compute all
the correct hardware register settings, given a C<Retro::ChannelState>
object and the setting of the key-down flag.

In order to decide which channel an event should be assigned to, it
makes sense to use the channel which requires the fewest register writes
to get it to the desired state.  Use the C<cost()> function to compute
how many register writes are required to get from another
C<ChannelRegisters> state to this one.  You can also pass C<undef> to
compute how many register writes are needed to fully set the state
(necessary if the channel state is undefined).

If you are ready to send a C<ChannelRegisters> object to output, use the
C<assemble()> method with a C<Retro::Assembler> instance, a time offset,
a channel index, and another C<ChannelRegisters> object representing the
current state of the channel registers, or C<undef> if the current state
of channel registers is undefined.

=cut

# =========
# Constants
# =========

# Array mapping each of the nine channels to their base operator
# offsets.
#
# The base operator offsets are used for sets of registers describing
# individual operator state.  In each of these register sets, channel 0
# operator 0 is always at offset zero from the base of the register
# bank.  Operator 1 is always at an offset three greater than the offset
# of operator 0, so channel 0 operato 1 would be at offset 3.
#
# To determine the offset of an operator register within such a register
# bank, use the channel number as an index into this @OP_OFFS array.
# The returned value is the offset of operator zero for this channel.
# To get the offset of operator one, add three.
#
my @OP_OFFS = (
  0x00,
  0x01,
  0x02,
  0x08,
  0x09,
  0x0a,
  0x10,
  0x11,
  0x12
);

# Array mapping indices of byte values stored within the
# ChannelRegisters object to their hardware register offset for channel
# zero.
#
# The first ten indices are pairs for channel zero and operator one.  
# The last three indices are channel-wide registers for channel zero.
#
# The first ten indices can be converted to offsets for channel N by
# adding to each of them $OP_OFFS[N].
#
# The last three indices can be converted to offsets for channel N by
# adding N to each of them.
#
my @REG_OFFS = (
  0x20, 0x23,   # 0, 1: Modulator frequency multiple &c.
  0x40, 0x43,   # 2, 3: Operator amplitude &c.
  0x60, 0x63,   # 4, 5: Attack/Decay
  0x80, 0x83,   # 6, 7: Sustain/Release
  0xe0, 0xe3,   # 8, 9: Waveform
  0xa0,         #   10: F-number, least significant bits
  0xb0,         #   11: F-number, most significant bits &c.
  0xc0,         #   12: Feedback &c.
);

# Array mapping Retro fscale parameter values to the value that should
# be written into hardware registers.
# 
my @FSCALE_MAP = (
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
  0xa,
  0xc,
  0xe
);

# ===============
# Local functions
# ===============

# encode_freq(f)
# --------------
#
# Given a Retro integer frequency number in range [0, 117824], return a
# list of two integers, the first being an OPL2 f-num in range [0, 1023]
# and the second being an OPL2 octave-block in range [0, 7].
#

# Array mapping the integer index to the power-of-two value for that
# integer, for indices up to and including 20.
#
# This is computed on the first call to encode_freq().
#
my @POWER_TWO = ();

# For each of the octave-blocks [0, 7], this array holds the frequency
# in Hz of the highest f-num in the octave-block.
#
# This is computed on the first call to encode_freq().
#
my @BLOCK_MAX = ();

sub encode_freq {
  # Get parameter
  ($#_ == 0) or die;
  my $f = shift;
  
  rtIsInteger($f) or die;
  (($f >= 0) and ($f <= 117824)) or die;
  
  # If BLOCK_MAX and POWER_TWO haven't been computed, compute them now
  if (scalar(@BLOCK_MAX) == 0) {
    # Compute all powers of 2 from 2^0 ... 2^20, with the array index
    # selecting the power it is raised to
    my $pt = 1;
    for(my $i = 0; $i <= 20; $i++) {
      if ($i > 0) {
        $pt *= 2;
      }
      push @POWER_TWO, ($pt);
    }
    
    # Fill the BLOCK_MAX array
    for(my $bi = 0; $bi <= 7; $bi++) {
      push @BLOCK_MAX, (
        (1023 * 49716) / $POWER_TWO[20 - $bi]
      );
    }
  }
  
  # Convert the given integer f number into a frequency in Hz
  $f = exp(($f - 30488) / 10000.0);
  ($f > 0) or die;
  
  # Find the least octave-block for which f is in range, or fall back to
  # the maximum octave-block
  my $block;
  for($block = 0; $block <= 7; $block++) {
    if ($f <= $BLOCK_MAX[$block]) {
      last;
    }
  }
  if ($block > 7) {
    $block = 7;
  }
  
  # Within this block, convert f from a Hz value to an f_num
  $f = ($f * $POWER_TWO[20 - $block]) / 49716.0;
  
  # Floor to integer and clamp to [0, 1023]
  $f = int($f);
  if ($f < 0) {
    $f = 0;
  }
  if ($f > 1023) {
    $f = 1023;
  }
  
  # Return frequency number and block
  return ($f, $block);
}

=head1 CONSTRUCTOR

=over 4

=item B<create()>

Create a new channel registers instance.

The object is initialized with all hardware registers for the channel
set to zero.

=cut

sub create {
  # Get parameters
  ($#_ == 0) or die;
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # "_st" stores an array of hardware registers; see REG_OFFS and
  # OP_OFFS for further information about indexing
  $self->{'_st'} = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  
  # Return the new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<set(cs, key_down)>

Set the complete state of channel hardware registers.

C<cs> is a C<Retro::ChannelState> object that defines the whole channel
state, except for the key-down flag.

C<key_down> is 1 if the key-down flag should be set, 0 if it should not
be set.

=cut

sub set {
  # Get self and parameters
  ($#_ == 2) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $cs = shift;
  (ref($cs) and $cs->isa('Retro::ChannelState')) or die;
  
  my $key_down = shift;
  (not ref($key_down)) or die;
  if ($key_down) {
    $key_down = 1;
  } else {
    $key_down = 0;
  }
  
  # Define value variable
  my $val = 0;
  
  # Index zero and one are register bank 0x20-0x35 for operators zero
  # and one
  for(my $op = 0; $op < 2; $op++) {
    $val = 0;
    
    # Bit 7 is set for amplitude vibrato enabled
    if ($cs->op($op, 'amod')) {
      $val |= 0x80;
    }
    
    # Bit 6 is set for frequency vibrato enabled
    if ($cs->op($op, 'fmod')) {
      $val |= 0x40;
    }
    
    # Bit 5 is set for sustain enable
    if ($cs->op($op, 'suse')) {
      $val |= 0x20;
    }
    
    # Bit 4 is set for envelope scaling
    if ($cs->op($op, 'escale')) {
      $val |= 0x10;
    }
    
    # Bits 3-0 encode the frequency scaling value
    $val |= $FSCALE_MAP[$cs->op($op, 'fscale')];
    
    # Store the register
    $self->{'_st'}->[0 + $op] = $val;
  }
  
  # Index two and three are register bank 0x40-0x55 for operators zero
  # and one
  for(my $op = 0; $op < 2; $op++) {
    $val = 0;
    
    # Bits 7-6 are the register scaling value
    $val |= (($cs->op($op, 'rscale') << 6);
    
    # Bits 5-0 are the amplitude level
    $val |= $cs->op($op, 'amp');
    
    # Store the register
    $self->{'_st'}->[2 + $op] = $val;
  }
  
  # Index four and five are register bank 0x60-0x75 for operators zero
  # and one
  for(my $op = 0; $op < 2; $op++) {
    $val = 0;
    
    # Bits 7-4 are the attack rate (inverted in hardware register)
    $val |= ((15 - $cs->op($op, 'attack')) << 4);
    
    # Bits 3-0 are the decay rate (inverted in hardware register)
    $val |= (15 - $cs->op($op, 'decay'));
    
    # Store the register
    $self->{'_st'}->[4 + $op] = $val;
  }
  
  # Index six and seven are register bank 0x80-0x95 for operators zero
  # and one
  for(my $op = 0; $op < 2; $op++) {
    $val = 0;
    
    # Bits 7-4 are the sustain level (inverted in hardware register)
    $val |= ((15 - $cs->op($op, 'sustain')) << 4);
    
    # Bits 3-0 are the decay rate (inverted in hardware register)
    $val |= (15 - $cs->op($op, 'decay'));
    
    # Store the register
    $self->{'_st'}->[6 + $op] = $val;
  }
  
  # Index eight and nine are register bank 0xe0-0xf5 for operators zero
  # and one
  for(my $op = 0; $op < 2; $op++) {
    $val = 0;
    
    # Bits 1-0 are the waveform
    $val |= $cs->op($op, 'wave');
    
    # Store the register
    $self->{'_st'}->[8 + $op] = $val;
  }
  
  # Get the f-num and block of the frequency
  my ($f_num, $block) = encode_freq($cs->ch('F'));
  
  # Index ten is channel register bank 0xa0-0xa8, the eight least
  # significant bits of the f_num
  $val = ($f_num & 0xff);
  $self->{'_st'}->[10] = $val;
  
  # Index eleven is channel register bank 0xb0-0xb8
  $val = 0;
  
  # ... Bit 5 is the key-down flag
  if ($key_down) {
    $val |= 0x20;
  }
  
  # ... Bits 4-2 are the block number of the frequency
  $val |= ($block << 2);
  
  # ... Bits 1-0 are the two most significant bits of the f-num
  $val |= ($f_num >> 8);
  
  # ... Write the register value
  $self->{'_st'}->[11] = $val;
  
  # Index twelve is channel register bank 0xc0-0xc8
  $val = 0;
  
  # ... Bits 3-1 is the feedback strength
  $val |= ($cs->ch('Feedback') << 1);
  
  # ... Bit 0 is inverse of the network style
  unless ($cs->ch('Network')) {
    $val |= 0x1;
  }
  
  # ... Write the register value
  $self->{'_st'}->[12] = $val;
}

# @@TODO:

=back

=head1 AUTHOR

Noah Johnson E<lt>noah.johnson@loupmail.comE<gt>

=head1 COPYRIGHT

Copyright 2023 Multimedia Data Technology, Inc.

This program is free software.  You can redistribute it and/or modify it
under the same terms as Perl itself.

This program is also dual-licensed under the MIT license:

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

=cut

# End with something that evaluates to true
1;
