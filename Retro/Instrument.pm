package Retro::Instrument;
use v5.14;
use warnings;

# Retro imports
use Retro::Util qw(
  rtIsInteger
);

=head1 NAME

Retro::Instrument - Retro instrument object.

=head1 SYNOPSIS

  use Retro::Instrument;
  
  # Create a new Instrument object that inherits default values for all
  # channel an operator parameters, and sets the given parameters for
  # channel and operators, with integers, graphs, and null allowed for
  # dictionary values
  my $instr = Retro::Instrument->create(undef, \%ch, \%op0, \%op1);
  
  # This time, inherit initial values from another instrument
  my $instr = Retro::Instrument->create($parent, \%ch, \%op0, \%op1);
  
  # Check whether instrument has any local graphs
  if ($instr->has_local) {
    ...
  }
  
  # Generate a full set of channel and operator settings all as integers
  # at a given local time and absolute time, and return repeat count
  my ($ch, $op0, $op1, $r) = $instr->solve($absolute_t, $local_t);
  
  # Same as above, except no local t; it is an error if there are any
  # local graphs in the instrument
  my ($ch, $op0, $op1, $r) = $instr->solve($absolute_t, undef);

=head1 DESCRIPTION

Instruments represent the full set of channel and operator parameter
values for a single channel at its two operators.

Since parameters may include graph objects, the value of parameters may
change over time.  To get the specific values of an instrument at a
given time C<t>, use the C<solve()> function.

=cut

# =========
# Constants
# =========

# Keys in this mapping are all the recognized operator and channel
# parameter names.  Values are scalars indicating the maximum allowed
# integer value for this parameter.  (The minimum allowed value is
# always zero.)
#
my %PARAM_RANGE = (
  'F'        => 117824,
  'Feedback' =>      7,
  'Network'  =>      1,
  'amp'      =>     63,
  'fscale'   =>     12,
  'amod'     =>      1,
  'fmod'     =>      1,
  'rscale'   =>      3,
  'wave'     =>      3,
  'suse'     =>      1,
  'escale'   =>      1,
  'attack'   =>     15,
  'decay'    =>     15,
  'sustain'  =>     15,
  'release'  =>     15
);

# Keys in this mapping are all the recognized channel parameters.
# Values are all one.
#
my %CHANNEL_PARAM = (
  'F'        => 1,
  'Feedback' => 1,
  'Network'  => 1
);

# Keys in this mapping are all the recognized operator parameters.
# Values are all one.
#
my %OP_PARAM = (
  'amp'      => 1,
  'fscale'   => 1,
  'amod'     => 1,
  'fmod'     => 1,
  'rscale'   => 1,
  'wave'     => 1,
  'suse'     => 1,
  'escale'   => 1,
  'attack'   => 1,
  'decay'    => 1,
  'sustain'  => 1,
  'release'  => 1
);

=head1 CONSTRUCTOR

=over 4

=item B<create(parent, ch, op0, op1)>

Create a new instrument.

C<parent> is another instrument that the new instrument inherits values
from.  If C<parent> is C<undef> then the new instrument will inherit the
default values for all channel and operator parameters.  These default
values are defined in the Retro specification.

C<ch>, C<op0>, and C<op1> are either hash references or C<undef>.
Passing C<undef> is equivalent to passing a reference to an empty hash.

All keys in the C<ch> hash must be channel parameter names.  All keys in
the C<op0> and C<op1> hashes must be operator parameter names.  A fatal
error with a user-friendly error message will be thrown if invalid keys
are present in any dictionary.

The values stored in all three hashes must either be scalar integers,
C<Retro::Graph> objects, or C<undef>.  A key that has an C<undef> value
is equivalent to not defining the key at all.  A fatal error with a
user-friendly error message will be thrown if the value has an
unsupported type.

Scalar integer values must pass C<rtIsInteger()> from C<Retro::Util> and
they must also be in a valid range for their respective parameter.  A
fatal error with a user-friendly error message will be thrown if the
integer value has an unsupported range.

You can use any sort of graph object for values.  The constructor will
check that a graph specified for a parameter has a range of values that
is within the valid range for the parameter.

=cut

sub create {
  # Get parameters
  ($#_ == 4) or die;
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $parent = shift;
  if (defined $parent) {
    (ref($parent) and $parent->isa(__PACKAGE__)) or die;
  }
  
  my $ch = shift;
  if (defined $ch) {
    (ref($ch) eq 'HASH') or die;
  }
  
  my $op0 = shift;
  if (defined $op0) {
    (ref($op0) eq 'HASH') or die;
  }
  
  my $op1 = shift;
  if (defined $op1) {
    (ref($op1) eq 'HASH') or die;
  }
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # Different initialization depending on whether we got a parent object
  if (defined $parent) {
    # Parent object, so begin by defining empty _ch and _op properties
    $self->{'_ch'} = {};
    $self->{'_op'} = [{}, {}];
    
    # Copy all channel values over
    for my $k (keys %{$parent->{'_ch'}}) {
      $self->{'_ch'}->{$k} = $parent->{'_ch'}->{$k};
    }
    
    # Copy all the operator values over
    for(my $i = 0; $i < 2; $i++) {
      for my $k (keys %{$parent->{'_op'}->[$i]}) {
        $self->{'_op'}->[$i]->{$k} = $parent->{'_op'}->[$i]->{$k};
      }
    }
    
  } else {
    # No parent object, so we are going to use default values for
    # initialization
    
    # '_ch' stores the channel parameters, initialized here with default
    # values
    $self->{'_ch'} = {
      'F'        => 91355,
      'Feedback' => 0,
      'Network'  => 1
    };
    
    # '_op' is an array reference storing the parameter sets for operator
    # zero and operator one, with all operator parameters initialized to
    # their default values
    $self->{'_op'} = [
      {
        'amp'      => 63,
        'fscale'   => 1,
        'amod'     => 0,
        'fmod'     => 0,
        'rscale'   => 0,
        'wave'     => 0,
        'suse'     => 1,
        'escale'   => 0,
        'attack'   => 8,
        'decay'    => 8,
        'sustain'  => 8,
        'release'  => 8
      },
      {
        'amp'      => 63,
        'fscale'   => 1,
        'amod'     => 0,
        'fmod'     => 0,
        'rscale'   => 0,
        'wave'     => 0,
        'suse'     => 1,
        'escale'   => 0,
        'attack'   => 8,
        'decay'    => 8,
        'sustain'  => 8,
        'release'  => 8
      }
    ];
  }
  
  # If ch parameter was defined, apply changes to state, checking values
  # along the way
  if (defined $ch) {
    for my $k (keys %$ch) {
      # Get the value
      my $val = $ch->{$k};
      
      # Skip if value is not defined
      (defined $val) or next;
      
      # Make sure this is a channel parameter
      (defined $CHANNEL_PARAM{$k}) or
        die "Channel parmeter dictionary has invalid key '$k'!\n";
      
      # Check value
      if (ref($val)) {
        ($val->isa('Retro::Graph')) or
          die "Unsupported channel parameter value type!\n";
        
        my ($min_v, $max_v) = $val->bounds;
        (($min_v >= 0) and ($max_v <= $PARAM_RANGE{$k})) or
          die "Graph for parameter '$k' has out of range values!\n";
      } else {
        rtIsInteger($val) or
          die "Unsupported channel parameter value type!\n";
        
        (($val >= 0) and ($val <= $PARAM_RANGE{$k})) or
          die "Value for parameter '$k' has out of range value!\n";
      }
      
      # Update channel parameter setting
      $self->{'_ch'}->{$k} = $val;
    }
  }
  
  # Handle any operator parameter sets that were defined
  for(my $i = 0; $i < 2; $i++) {
    # Get the relevant set argument
    my $op;
    if ($i == 0) {
      $op = $op0;
    } elsif ($i == 1) {
      $op = $op1;
    } else {
      die;
    }
    
    # Only proceed if set is defined
    if (defined $op) {
      for my $k (keys %$op) {
        # Get the value
        my $val = $op->{$k};
        
        # Skip if value is not defined
        (defined $val) or next;
        
        # Make sure this is an operator parameter
        (defined $OP_PARAM{$k}) or
          die "Operator parameter dictionary has invalid key '$k'!\n";
        
        # Check value
        if (ref($val)) {
          ($val->isa('Retro::Graph')) or
            die "Unsupported operator parameter value type!\n";
          
          my ($min_v, $max_v) = $val->bounds;
          (($min_v >= 0) and ($max_v <= $PARAM_RANGE{$k})) or
            die "Graph for parameter '$k' has out of range values!\n";
        } else {
          rtIsInteger($val) or
            die "Unsupported operator parameter value type!\n";
          
          (($val >= 0) and ($val <= $PARAM_RANGE{$k})) or
            die "Value for parameter '$k' has out of range value!\n";
        }
        
        # Update operator parameter setting
        $self->{'_op'}->[$i]->{$k} = $val;
      }
    }
  }
  
  # '_local' will be set to one if any value in the channel or operator
  # parameter dictionaries is a local graph
  $self->{'_local'} = 0;
  for(my $i = 0; $i < 3; $i++) {
    # Get relevant set
    my $s;
    if ($i == 0) {
      $s = $self->{'_ch'};
    } elsif ($i == 1) {
      $s = $self->{'_op'}->[0];
    } elsif ($i == 2) {
      $s = $self->{'_op'}->[1];
    } else {
      die;
    }
    
    # Check if any value is a local graph
    for my $v (values %$s) {
      if (ref($v) and $v->isa('Retro::Graph')) {
        if ($v->is_local) {
          $self->{'_local'} = 1;
          last;
        }
      }
    }
    
    # We can leave loop early if local flag is set
    if ($self->{'_local'}) {
      last;
    }
  }
  
  # Return the new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<has_local()>

Return 1 if this instrument has at least one value that is a local
graph.  Otherwise, return 0.

=cut

sub has_local {
  # Get self
  ($#_ == 0) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  # Answer query
  return $self->{'_local'};
}

=item B<solve(absolute_t, local_t)>

Get all the integer channel and operator parameter values for an
instrument at a given time.

C<absolute_t> is the absolute time, where C<t> zero is the start of the
performance.  It must pass C<rtIsInteger()> from C<Retro::Util> and it
must be zero or greater.

C<local_t> is the local time, where C<t> zero is the start of a melodic
event.  This parameter may also be set to C<undef> if there is no local
time in the current context.  If defined, it must pass C<rtIsInteger()>
and it must be zero or greater.

If C<local_t> is C<undef>, then an error occurs if this instrument
contains any local graphs.  A fatal error with a user-friendly error
message will be raised in this case.

The return value has four elements in list context.  The first three
elements are hash references to the channel parameters, operator zero
parameters, and operator one parameters, respectively.

The fourth returned element is a repeat count.  This value is an integer
that is either one or greater, or -1.  If the value is one or greater,
it counts how many cycles, including the current cycle, this instrument
will return the exact same set of parameters for.  If the value is -1,
it means the instrument will return the exact same same set of
parameters for this cycle and all cycles that follow it.

=cut

sub solve {
  # Get self and parameters
  ($#_ == 2) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $abs_t = shift;
  (rtIsInteger($abs_t) and ($abs_t >= 0))  or die;
  
  my $loc_t = shift;
  if (defined $loc_t) {
    (rtIsInteger($loc_t) and ($loc_t >= 0)) or die;
  }
  
  # If local t is not defined, make sure we don't have any local graphs
  unless (defined $loc_t) {
    (not $self->{'_local'}) or
      die "Local graph used in context without a local time!\n";
  }
  
  # Define the result sets and the r value
  my $ch  = {};
  my $op0 = {};
  my $op1 = {};
  my $r   = -1;
  
  # Resolve the instrument at the given times
  for(my $i = 0; $i < 3; $i++) {
    # Get relevant source set and destination set
    my $src;
    my $dest;
    
    if ($i == 0) {
      $src  = $self->{'_ch'};
      $dest = $ch;
    
    } elsif ($i == 1) {
      $src  = $self->{'_op'}->[0];
      $dest = $op0;
      
    } elsif ($i == 2) {
      $src  = $self->{'_op'}->[1];
      $dest = $op1;
    
    } else {
      die;
    }
    
    # Resolve all keys
    for my $k (keys %$src) {
      # Get copy of value
      my $x = $src->{$k};
      
      # If value is a graph, resolve it and update r
      if (ref($x)) {
        my $gv;
        my $gr;
        
        # Query the graph with the correct time measurement
        if ($x->is_local) {
          ($gv, $gr) = $x->get($loc_t);
        } else {
          ($gv, $gr) = $x->get($abs_t);
        }
        
        # Resolve and update r
        $x = $gv;
        if ($gr != -1) {
          # Graph return something other than -1, so we need to update
          if ($r == -1) {
            # Current r is -1, so update it to graph value
            $r = $gr;
          } else {
            # Current r is not -1, so set it to minimum of r and gr
            if ($r > $gr) {
              $r = $gr;
            }
          }
        }
      }
      
      # Store resolved value in destination
      $dest->{$k} = $x;
    }
  }
  
  # Return the resolved values
  return ($ch, $op0, $op1, $r);
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
