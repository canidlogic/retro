#!/usr/bin/env perl
use v5.14;
use warnings;

=head1 NAME

retro_freq.pl - Compute Retro logarithmic frequency values.

=head1 SYNOPSIS

  ./retro_freq.pl hz 440.0
  ./retro_freq.pl eq 9

=head1 DESCRIPTION

Compute Retro integer logarithmic frequency values, given either a
frequency in Hz or an equal-tempered pitch.

If the first parameter is C<hz> then the second parameter is a
floating-point value in range [0.05, 6208.0] giving the frequency in Hz
to encode.

If the first parameter is C<eq> then the second parameter is a signed
integer that gives the number of semitones away from middle C in an
equal-tempered scale with A440.

In both cases, the script will compute an integer in range [0, 117824]
which can be used for Retro C<F> channel parameters to set the specified
pitch.

=cut

# ==================
# Program entrypoint
# ==================

# Get parameters
#
($#ARGV == 1) or die "Wrong number of program arguments!\n";

my $mode = shift @ARGV;
my $freq = shift @ARGV;

# If mode is eq then convert it to a frequency value with mode hz
#
if ($mode eq 'eq') {
  # freq must be a signed integer; convert to numeric value
  ($freq =~ /^[\+\-]?[0-9]+$/) or die "Invalid semitone count!\n";
  $freq = int($freq);
  
  # Adjust freq so reference pitch of A440 will be semitone 6 in middle
  # of octave instead of semitone 9
  $freq -= 3;
  
  # If freq is negative, set negf flag and take its absolute value
  my $negf = 0;
  if ($freq < 0) {
    $negf = 1;
    $freq = 0 - $freq;
  }
  
  # Convert freq into a number of octaves and a semitone remainder
  my $octaves = int($freq / 12);
  my $semirem = $freq % 12;
  
  # If negf flag is on, then if semitone remainder is non-zero, increase
  # octave count by one and take (12 - remainder); in all cases, finish
  # by inverting octave count
  if ($negf) {
    if ($semirem > 0) {
      $semirem = 12 - $semirem;
      $octaves++;
    }
    $octaves = 0 - $octaves;
  }
  
  # Compute semitone offset from A440 disregarding register
  my $semioff = $semirem - 6;
  
  # Compute frequency within main octave
  $freq = ((2.0 ** (1.0 / 12.0)) ** $semioff) * 440.0;
  
  # Adjust frequency by octave displacement
  $freq *= (2.0 ** $octaves);
  
  # Change mode to hz
  $mode = 'hz';
}

# If we get here, make sure mode is 'hz'
#
($mode eq 'hz') or die "Unrecognized mode '$mode'!\n";

# Check range of frequency
#
$freq = $freq + 0.0;
(($freq >= 0.05) and ($freq <= 6208.0)) or
  die "Frequency out of range!\n";

# Compute the encoded logarithm value
#
my $iv = int(log($freq) * 10000) + 30488;
if ($iv < 0) {
  $iv = 0;
}
if ($iv > 117824) {
  $iv = 117824;
}

# Print the result
#
print "$iv\n";

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
