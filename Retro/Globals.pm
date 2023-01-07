package Retro::Globals;
use v5.14;
use warnings;

# Retro imports
use Retro::Util qw(
  rtIsInteger
);

=head1 NAME

Retro::Globals - Retro global state and drum control.

=head1 SYNOPSIS

  use Retro::Globals;
  
  # Create a new Globals object that has all global parameters set to
  # their default values and the rhythm map empty
  my $gs = Retro::Globals->create;
  
  # Update global parameters according to a dictionary
  $gs->update(\%dict);
  
  # Schedule a drum control flag state setting at a given time
  $gs->drums($t, $flags);
  
  # Output assembly instructions for all the global and drum control
  # register writes throughout the whole performance
  $gs->assemble($asm);

=head1 DESCRIPTION

Tracks global parameters and drum control throughout the performance and
generates all the OPL2 assembly instructions necessary for all registers
except the channel and operator register banks.

=cut

# =========
# Constants
# =========

# Keys in this mapping are all the recognized global parameter names.
# Values are scalars indicating the maximum allowed integer value for
# this parameter.  (The minimum allowed value is always zero.)
#
my %PARAM_RANGE = (
  '_avib' => 1,
  '_fvib' => 1,
  '_csm'  => 1,
  '_kspl' => 1
);

# ===============
# Local functions
# ===============

# _rhythm_read(\@rm, $i)
# ----------------------
#
# Return the next event from a sorted rhythm map.
#
# rm is a reference to a *sorted* rhythm map array.  i is the index
# within that array that reading should start.  If i is greater than or
# equal to the length of the array, the function assumes that all rhythm
# map events have been read.
#
# The return value is a list of three elements:  the time of the rhythm
# map record, the flags within the rhythm map record, and the number of
# records that were read during this operation.  If there are no more
# rhythm map records to read, two C<undef> values and a zero count are
# returned.
#
# If there are multiple duplicate records, this function will only
# return the record once and use the record count to indicate how many
# records were read.  An error is thrown if there are duplicate records
# at the same time point that have different flag values.
#
sub _rhythm_read{
  # Get parameters
  ($#_ == 1) or die;
  
  my $rm = shift;
  (ref($rm) eq 'ARRAY') or die;
  
  my $i = shift;
  (rtIsInteger($i) and ($i >= 0)) or die;
  
  # If i has exceeded the rhythm map, return no further records
  if ($i >= scalar(@$rm)) {
    return (undef, undef, 0);
  }
  
  # Get the current record and set count to one
  my $rec   = $rm->[$i];
  my $count = 1;
  
  # Parse record into offset and flags
  my $offs  = int($rec / 64);
  my $flags = $rec % 64;
  
  # Look for duplicate offset records
  for(my $j = $i + 1; $j < scalar(@$rm); $j++) {
    # Get this record
    my $rb = $rm->[$j];
    
    # Leave loop if offset of this record doesn't match
    unless (int($rb / 64) == $offs) {
      last;
    }
    
    # We found a duplicate -- check that it is same as original record
    ($rec == $rb) or
      die "Conflicting records within the rhythm map!\n";
    
    # Increase the count
    $count++;
  }
  
  # Return result
  return ($offs, $flags, $count);
}

# _reg_compute(\%gs, $drum, $t)
# -----------------------------
#
# gs is a reference to a hash defining global parameter values.
#
# drum is a scalar integer in range [0, 63] that defines the drum
# control flags.
#
# t is a scalar integer that passes rtIsInteger() and is greater than or
# equal to zero, specifying the absolute time against which graphs in
# the gs hash are resolved.
#
# Returns three values.  The first two are the register 08 value and the
# register BD value.  The third is the repeat value from all graphs that
# were solved.
#
sub _reg_compute {
  # Get parameters
  ($#_ == 2) or die;
  
  my $gs = shift;
  (ref($gs) eq 'HASH') or die;
  
  my $drum = shift;
  (rtIsInteger($drum) and ($drum >= 0) and ($drum <= 63)) or die;
  
  my $t = shift;
  (rtIsInteger($t) and ($t >= 0)) or die;
  
  # Define local dictionary to hold the specific resolved global
  # parameter values
  my %vdict;
  
  # Define r to hold the repeat return value
  my $r = -1;
  
  # Resolve the global parameters
  for my $k (keys %PARAM_RANGE) {
    # Make sure global parameter exists
    defined($gs->{$k}) or die;
    
    # Get the value
    my $val = $gs->{$k};
    
    # Handle the different value types
    if (ref($val)) {
      # Reference must be to a global graph
      $val->isa('Retro::Graph') or die;
      (not $val->is_local) or die;
      
      # Solve the graph at the given global time
      my ($gv, $gr) = $val->get($t);
      
      # Check that computed value is in range
      (($gv >= 0) and ($gv <= $PARAM_RANGE{$k})) or die;
      
      # Add value to local dictionary
      $vdict{$k} = $gv;
      
      # Update r value
      if ($gr > 0) {
        if ($r > 0) {
          if ($gr < $r) {
            $r = $gr;
          }
        } else {
          $r = $gr;
        }
      }
      
    } else {
      # Scalar value must be an integer in range
      rtIsInteger($val) or die;
      (($val >= 0) and ($val <= $PARAM_RANGE{$k})) or die;
      
      # Add value to local dictionary
      $vdict{$k} = $val;
    }
  }
  
  # Compute the register 08 value
  my $reg08 = 0;
  
  if ($vdict{'_csm'}) {
    $reg08 |= 0x80;
  }
  if ($vdict{'_kspl'}) {
    $reg08 |= 0x40;
  }
  
  # Compute the register BD value
  my $regBD = $drum;
  
  if ($vdict{'_avib'}) {
    $regBD |= 0x80;
  }
  if ($vdict{'_fvib'}) {
    $regBD |= 0x40;
  }
  
  # Return results
  return ($reg08, $regBD, $r);
}

=head1 CONSTRUCTOR

=over 4

=item B<create()>

Create a new global state object.

All global parameters will be set to their default values and the rhythm
map will be empty.

=cut

sub create {
  # Get parameters
  ($#_ == 0) or die;
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # '_global' is a hash mapping global parameters to their values;
  # initialize to their default values
  $self->{'_global'} = {
    '_avib' => 0,
    '_fvib' => 0,
    '_csm'  => 0,
    '_kspl' => 0
  };
  
  # '_rmap' is an array storing the rhythm map; each record is a scalar
  # integer that encodes the time offset and the drum control flags as
  # follows:
  #
  # Bits: 36 - 6 |  5  |  4  |  3  |  2  |  1  |  0
  #      --------+-----+-----+-----+-----+-----+-----
  #       Offset |  R  | BD  | SD  | TOM | CYM | HH
  #
  #   Offset: time offset of map entry, t=0 is start of performance
  #   R     : rhythm mode flag
  #   BD    : bass drum flag
  #   SD    : snare drum flag
  #   TOM   : tom-tom flag
  #   CYM   : cymbal flag
  #   HH    : hi-hat flag
  #
  # The rhythm map is not sorted in chronological order
  $self->{'_rmap'} = [];
  
  # Return the new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<update(dict)>

Update the global parameters.

C<dict> is a hash reference storing new parameter values.  You can also
pass C<undef>, which is equivalent to passing an empty hash.

The keys in the dictionary must be global parameter names.  A fatal 
error with a user-friendly error message will be thrown if invalid keys
are present in any dictionary.  Global parameters that are not named in
the dictionary do not have their state modified by this function.

The values may be scalar integers, C<Retro::Graph> objects, or C<undef>.
Using an C<undef> value is equivalent to leaving out the corresponding
dictionary record.  Graphs may not be local graphs.  A fatal error with
a user-friendly error message will be thrown if a value has an
unsupported type, or if a local graph is included.

Scalar integers must be in the valid range for the global parameter.
Graph ranges must be within the range for the parameter.  A fatal error
with a user-friendly error message will be thrown if an integer value
or graph has an unsupported range.

=cut

sub update {
  # Get self and parameters
  ($#_ == 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $dict = shift;
  if (defined $dict) {
    (ref($dict) eq 'HASH') or die;
  }
  
  # If dict parameter was defined, apply changes to state, checking
  # values along the way
  if (defined $dict) {
    for my $k (keys %$dict) {
      # Get the value
      my $val = $dict->{$k};
      
      # Skip if value is not defined
      (defined $val) or next;
      
      # Make sure this is a global parameter
      (defined $PARAM_RANGE{$k}) or
        die "Global parmeter dictionary has invalid key '$k'!\n";
      
      # Check value
      if (ref($val)) {
        ($val->isa('Retro::Graph')) or
          die "Unsupported global parameter value type!\n";
        
        (not $val->is_local) or
          die "Local graph may not be used for global parameter!\n";
        
        my ($min_v, $max_v) = $val->bounds;
        (($min_v >= 0) and ($max_v <= $PARAM_RANGE{$k})) or
          die "Graph for parameter '$k' has out of range values!\n";
      } else {
        rtIsInteger($val) or
          die "Unsupported global parameter value type!\n";
        
        (($val >= 0) and ($val <= $PARAM_RANGE{$k})) or
          die "Value for parameter '$k' has out of range value!\n";
      }
      
      # Update global parameter setting
      $self->{'_global'}->{$k} = $val;
    }
  }
}

=item B<drums(t, flags)>

Add a drum control state to the rhythm event map.

C<t> is the time offset.  It must pass C<rtIsInteger()> from the
C<Retro::Util> module and it must be greater than or equal to zero.
A C<t> value of zero is the start of the performance.

The C<flags> encode the drum control state at the indicated time.  This
must pass C<rtIsInteger()> and be in range [0, 63].  It encodes flags
according to the following scheme:

  Bits:  5  |  4  |  3  |  2  |  1  |  0
       -----+-----+-----+-----+-----+-----
         R  | BD  | SD  | TOM | CYM | HH
  
  R  : rhythm mode flag
  BD : bass drum flag
  SD : snare drum flag
  TOM: tom-tom flag
  CYM: cymbal flag
  HH : hi-hat flag

Drum control states may be added to the rhythm map in any order.  They
do not have to be added in chronological order.

=cut

sub drums {
  # Get self and parameters
  ($#_ == 2) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $t = shift;
  (rtIsInteger($t) and ($t >= 0)) or die;
  
  my $flags = shift;
  rtIsInteger($flags) or die;
  (($flags >= 0) and ($flags <= 63)) or die;
  
  # Pack the offset and flags into a single integer
  my $iv = ($t * 64) + $flags;
  
  # Add the packed record to the rhythm map
  push @{$self->{'_rmap'}}, ($iv);
  
  # Check that total length of rhythm map can still be expressed in an
  # integer
  rtIsInteger(scalar(@{$self->{'_rmap'}})) or
    die "Too many records in the rhythm map!\n";
}

=item B<assemble(asm)>

Schedule all the necessary hardware register writes in the given
assembler object for all registers except the channel and operator
register banks.

C<asm> must be a C<Retro::Assembler> instance, to which all the
scheduled register writes will be made.

In addition to all the OPL2 registers storing the global parameters and
the drum control flags, this function will also always schedule a write
to register 0x01 at C<t> zero to set a flag enabling waveform control
for operators.

If more than one record is in the rhythm map at the same time, then all
records at the same time must have the same flags value or the function
fails.

=cut

sub assemble {
  # Get self and parameters
  ($#_ == 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $asm = shift;
  (ref($asm) and $asm->isa('Retro::Assembler')) or die;
  
  # Sort the rhythm map in chronological order
  my @rm = sort { $a <=> $b } @{$self->{'_rmap'}};
  
  # Schedule the write to register 0x01 enabling waveform control
  $asm->reg(0, 0x01, 0x20);
  
  # The cs is an array reference to registers 08 and BD holding their
  # current byte values, or undef if state not defined yet
  my $cs = undef;
  
  # The t value starts at zero and rhythm map index also starts at zero
  my $t  = 0;
  my $ri = 0;
  
  # Load rev with first rhythm event, or undef if no rhythm events
  my $reof;
  my $refl;
  my $recv;
  
  ($reof, $refl, $recv) = _rhythm_read(\@rm, $ri);
  $ri += $recv;
  
  my $rev = undef;
  if (defined $reof) {
    $rev = [$reof, $refl];
  }
  
  # Rhythm flags start zero
  my $rflags = 0;
  
  # Processing loop
  while (1) {
    # If rev is loaded and t has reached its offset, update the rhythm
    # flags and load rev with the next rhythm event (if there is one)
    if ((defined $reof) and ($t >= $reof)) {
      $rflags = $refl;
      ($reof, $refl, $recv) = _rhythm_read(\@rm, $ri);
      $ri += $recv;
    }
    
    # Determine the register states at this t value, as well as the
    # repeat count for global parameters
    my ($r08, $rBD, $repeat) = _reg_compute(
                                  $self->{'_global'},
                                  $rflags,
                                  $t);
    
    # Schedule necessary writes with assembler to get the registers to
    # their current values
    if (defined $cs) {
      # Current state defined, so only update the parts of the state
      # that have changed
      unless ($cs->[0] == $r08) {
        $asm->reg($t, 0x08, $r08);
        $cs->[0] = $r08;
      }
      unless ($cs->[1] == $rBD) {
        $asm->reg($t, 0xbd, $rBD);
        $cs->[1] = $rBD;
      }
      
    } else {
      # Current state is undefined, so write both register values and
      # then set the current state with these new values
      $asm->reg($t, 0x08, $r08);
      $asm->reg($t, 0xbd, $rBD);
      $cs = [$r08, $rBD];
    }
  
    # Update t or leave loop
    if (defined $reof) {
      # We have another rhythm event -- it should have a greater time
      # value
      ($reof > $t) or die;
      
      # Check whether there is a repeat count indicating further
      # parameter changes
      if ($repeat > 0) {
        # Further parameter changes -- advance t to the next change in
        # global parameters
        $t = $t + $repeat;
        
        # If the next rhythm event occurs before this next change in
        # global parameters, move t back to this next rhythm event
        if ($t > $reof) {
          $t = $reof;
        }
        
      } else {
        # No further parameter changes, so just move t to the next
        # rhythm event
        $t = $reof;
      }
      
    } else {
      # No further rhythm events, so update t according to repeat count
      # or leave loop
      if ($repeat > 0) {
        # Further parameter changes -- advance t to the next change in
        # global parameters
        $t = $t + $repeat;
        
      } else {
        # No further parameter changes, so we can leave the loop
        last;
      }
    }
  }
}

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
