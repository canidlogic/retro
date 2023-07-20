#!/usr/bin/env perl
use strict;
use warnings;

# Core imports
use Carp;
use Scalar::Util qw(looks_like_number);

=head1 NAME

retro_tempo.pl - Compute the actual tempo in a Retro performance.

=head1 SYNOPSIS

  retro_tempo.pl -control 60 -quarter 96 -bpm 120.0

=head1 DESCRIPTION

Retro performances are timed according to a control rate that is
somewhere in range 1 to 1024 Hz (inclusive).  MIDI files that do not use
one of the SMPTE timings are timed according to an integer subdivision
of the quarter note which occurs within a tempo that can be changed with
Set Tempo meta-events within the MIDI file.

The actual Set Tempo meta-events specify the tempo as the number of
microseconds per quarter note.  Retro is not generally able to exactly
perform the specified tempi because Retro is limited to the control
rate.  Retro will therefore approximate tempi as well as it can using
the control rate.

This script will take the control rate, the division of the quarter
note, and the tempo expressed as Beats Per Minute where the quarter note
is the beat.  It will then print out various information, including the
actual tempo that would be used in a Retro performance.  The tempo in
the Retro performance might be significantly different from the tempo
specified in the MIDI file, which is why this script is useful for
determining exactly what tempo will actually be performed.

The tempo approximation will get more accurate the higher the control
rate is, and the lower the quarter note division is.  This tool can
therefore also be helpful in determining how high of a control rate and
how low of a quarter note division you actually need.

This script is irrelevant for MIDI files that use SMPTE timings.

=head2 Program arguments

The parameters to this script form key/value pairs.

The C<-control> parameter is required.  Its value is an unsigned decimal
integer in range 1 to 1024 (inclusive) that specifies the control rate
of the Retro performance in Hz.

The <-quarter> parameter is required.  Its value is an unsigned decimal
integer in range 1 to 32767 (inclusive) that specifies the number of
delta-time units per quarter note used in the MIDI file.

The <-bpm> parameter is required.  Its value is a floating-point value
greater than zero that specifies the Beats Per Minute of the tempo that
will be set, where the beat is a quarter note.

=cut

# ==================
# Program entrypoint
# ==================

# Read program arguments
#
my $p_control = undef;
my $p_quarter = undef;
my $p_bpm     = undef;

while (scalar(@ARGV) > 0) {
  # Get the next key/value pair
  (scalar(@ARGV) >= 2) or croak("Incomplete program argument pair");
  
  my $key = shift(@ARGV);
  my $val = shift(@ARGV);
  
  # Handle the different parameters
  if ($key eq '-control') {
    (not defined $p_control) or croak("Duplicate parameter: -control");
    ($val =~ /^[0-9]{1,4}$/) or croak("Invalid value for -control");
    
    $val = int($val);
    (($val >= 1) and ($val <= 1024)) or
      croak("-control rate out of range");
    
    $p_control = $val;
    
  } elsif ($key eq '-quarter') {
    (not defined $p_quarter) or croak("Duplicate parameter: -quarter");
    ($val =~ /^[0-9]{1,5}$/) or croak("Invalid value for -quarter");
    
    $val = int($val);
    (($val >= 1) and ($val <= 32767)) or
      croak("-quarter division out of range");
    
    $p_quarter = $val;
    
  } elsif ($key eq '-bpm') {
    (not defined $p_bpm) or croak("Duplicate parameter: -bpm");
    
    looks_like_number($val) or croak("Invalid value for -bpm");
    $val = $val + 0.0;
    ($val > 0.0) or croak("-bpm value out of range");
    
    $p_bpm = $val;
    
  } else {
    croak("Unrecognized program argument: $key");
  }
}

(defined $p_control) or croak("Missing -control parameter");
(defined $p_quarter) or croak("Missing -quarter parameter");
(defined $p_bpm) or croak("Missing -bpm parameter");

# First, we want to compute the number of microseconds per quarter note
#
my $q_micro = int((1 / ($p_bpm / 60.0)) * 1000000);
if ($q_micro < 1) {
  $q_micro = 1;
}
($q_micro <= 16777215) or croak("Tempo too fast for a MIDI file");

printf "Quarter notes per minute        : %f\n", $p_bpm;
printf "Microseconds per quarter note   : %d (quantized)\n", $q_micro;

# Second, we want to compute the number of microseconds per delta time
# unit
#
my $d_micro = $q_micro / $p_quarter;

print "\n";
printf "Delta units per quarter note    : %d\n", $p_quarter;
printf "Microseconds per delta unit     : %f\n", $d_micro;

# Third, we want to compute the number of microseconds per control rate
# pulse
#
my $c_micro = (1 / $p_control) * 1000000;

print "\n";
printf "Control rate                    : %d\n", $p_control;
printf "Microseconds per control pulse  : %f\n", $c_micro;

# Fourth, we want to compute the quantized number of control pulses per
# delta unit
#
my $pulse_per_delta = int(($d_micro / $c_micro) + 0.5);
if ($pulse_per_delta < 1) {
  $pulse_per_delta = 1;
}

print "\n";
printf "Control pulses per delta unit   : %d (quantized)\n",
          $pulse_per_delta;

# Fifth, compute the performance tempo based on the control pulse
#
my $actual_quarter = ($pulse_per_delta * $p_quarter * $c_micro);
my $actual_bpm = (1 / ($actual_quarter / 1000000)) * 60.0;

print "\n";
printf "Actual microseconds per quarter : %f\n", $actual_quarter;
printf "Actual quarter notes per minute : %f\n", $actual_bpm;

=head1 AUTHOR

Noah Johnson E<lt>noah.johnson@loupmail.comE<gt>

=head1 COPYRIGHT

Copyright (c) 2023 Multimedia Data Technology Inc
  
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
