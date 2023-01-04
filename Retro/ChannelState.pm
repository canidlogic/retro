package Retro::ChannelState;
use v5.14;
use warnings;

# Retro imports
use Retro::Util qw(
  rtIsInteger
);

=head1 NAME

Retro::ChannelState - OPL2 channel state representation.

=head1 SYNOPSIS

  use Retro::ChannelState;
  
  # Create a new ChannelState object with all channel and operator
  # parameters set to default values
  my $state = Retro::ChannelState->create;
  
  # Create a new ChannelState object and copy all the parameter values
  # from an existing object
  my $state = Retro::ChannelState->create($source);
  
  # Set and get channel parameters
  $state->ch($param_name, $param_val);
  my $val = $state->ch($param_name);
  
  # Set and get operator parameters
  $state->op($op_index, $param_name, $param_val);
  my $val = $state->op($op_index, $param_name);

=head1 DESCRIPTION

Represents a snapshot of all channel and operator parameters within a
single OPL2 channel.

Each instance of this object stores a complete set of channel parameter
values, and two complete sets of operator parameter values for operator
zero and operator one.  After construction, all channel parameters will
be set to the default values defined by the Retro specification and all
operator parameters for both of the operators will be set to the default
values defined by the Retro specification.  Alternatively, you can pass
an existing C<ChannelState> object and copy its state into the new
instance.

Individual parameter values get be read and written using the C<ch()>
function for channel parameters and the C<op()> function for operator
parameters.  Graph objects are not allowed as parameter values in this
object.  All values passed in are checked for valid range.

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

=item B<create([source])>

Create a new channel state instance.

If a parameter is given, it must be a C<ChannelState> object.  In that
case, the new state object will be initialized with all the settings
from the given source object.

If no parameter is given, all parameters for the channel and the two
operators start out with their default values.

=cut

sub create {
  # Get parameters
  (($#_ == 0) or ($#_ == 1)) or die;
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $source = undef;
  if ($#_ >= 0) {
    $source = shift;
    (ref($source) and $source->isa(__PACKAGE__)) or die;
  }
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # Different initialization depending on whether we got a source object
  if (defined $source) {
    # Source object, so begin by defining empty _ch and _op properties
    $self->{'_ch'} = {};
    $self->{'_op'} = [{}, {}];
    
    # Copy all channel values over
    for my $k (keys %{$source->{'_ch'}}) {
      $self->{'_ch'}->{$k} = $source->{'_ch'}->{$k};
    }
    
    # Copy all the operator values over
    for(my $i = 0; $i < 2; $i++) {
      for my $k (keys %{$source->{'_op'}->[$i]}) {
        $self->{'_op'}->[$i]->{$k} = $source->{'_op'}->[$i]->{$k};
      }
    }
    
  } else {
    # No source object, so we are going to use default values
    
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
  
  # Return the new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<ch(param_name[, value])>

Get or set the value of a channel parameter.

C<param_name> is the name of a channel parameter.

If a C<value> is given, it must pass the C<rtIsInteger()> function of
<Retro::Util> and it must be in the valid range for the parameter
selected by C<param_name>.  In this case, the named parameter is changed
to the new given value.

If no C<value> is given, then this method returns the current value of
the named parameter.

=cut

sub ch {
  # Get self and parameter name
  ($#_ >= 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $param_name = shift;
  (not ref($param_name)) or die;
  (defined $CHANNEL_PARAM{$param_name}) or die;
  
  # Different handling depending on remaining parameter count
  if ($#_ < 0) {
    # GET operation -- return requested parameter value
    return $self->{'_ch'}->{$param_name};
    
  } elsif ($#_ == 0) {
    # SET operation -- get value parameter
    my $val = shift;
    rtIsInteger($val) or die;
    
    # Check range of given value
    (($val >= 0) and ($val <= $PARAM_RANGE{$param_name})) or die;
    
    # Set requested parameter value
    $self->{'_ch'}->{$param_name} = $val;
    
  } else {
    die;
  }
}

=item B<op(op_index, param_name[, value])>

Get or set the value of an operator parameter.

C<op_index> selects which operator to modify within this channel.  This
can either be zero for operator zero or one for operator one.

C<param_name> is the name of an operator parameter.

If a C<value> is given, it must pass the C<rtIsInteger()> function of
<Retro::Util> and it must be in the valid range for the parameter
selected by C<param_name>.  In this case, the named parameter is changed
to the new given value.

If no C<value> is given, then this method returns the current value of
the named parameter of the indicated operator.

=cut

sub op {
  # Get self, operator index, and parameter name
  ($#_ >= 2) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $op_i = shift;
  rtIsInteger($op_i) or die;
  (($op_i == 0) or ($op_i == 1)) or die;
  
  my $param_name = shift;
  (not ref($param_name)) or die;
  (defined $OP_PARAM{$param_name}) or die;
  
  # Different handling depending on remaining parameter count
  if ($#_ < 0) {
    # GET operation -- return requested parameter value
    return $self->{'_op'}->[$op_i]->{$param_name};
    
  } elsif ($#_ == 0) {
    # SET operation -- get value parameter
    my $val = shift;
    rtIsInteger($val) or die;
    
    # Check range of given value
    (($val >= 0) and ($val <= $PARAM_RANGE{$param_name})) or die;
    
    # Set requested parameter value
    $self->{'_op'}->[$op_i]->{$param_name} = $val;
    
  } else {
    die;
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
