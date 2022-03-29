#!/usr/bin/env perl

# Core dependencies
use Math::Trig ':pi';
use POSIX ();

=head1 NAME

sinewav.pl - Generate a WAV file containing a sine wave.

=head1 SYNOPSIS

  sinewav.pl out.wav 48000 440.0 25000.0 100

=head1 DESCRIPTION

Generate a single-channel, uncompressed 16-bit PCM WAV file at a
sampling rate of 48000 Hz or 44100 Hz with a given wave frequency,
amplitude, and repetition count.  The wave function used is a sine wave.

In the example given in the synopsis, the sampling rate is 48000 Hz, the
frequency is 440 Hz, the amplitude is 25000, and there are 100
repetitions.  The result is generated in a new WAV file called out.wav

The sampling rate must either be 48000 or 44100.

The frequency must be in range [1.0, 20000.0]

The amplitude must be in range [1.0, 32767.0]

The repetition count must be an integer greater than zero.  When divided
by the frequency, the resulting count of seconds must not exceed 60 (one
minute).

=cut

# ==================
# Program entrypoint
# ==================

# Check parameter count
#
($#ARGV == 4) or die "Wrong number of program arguments, stopped";

# Get the program arguments and convert to strings
#
my $arg_path = $ARGV[0];
my $arg_rate = $ARGV[1];
my $arg_freq = $ARGV[2];
my $arg_ampl = $ARGV[3];
my $arg_repc = $ARGV[4];

$arg_path = "$arg_path";
$arg_rate = "$arg_rate";
$arg_freq = "$arg_freq";
$arg_ampl = "$arg_ampl";
$arg_repc = "$arg_repc";

# Convert sample rate argument to integer and check validity
#
if ($arg_rate =~ /^48000(?:\.0*)?$/) {
  $arg_rate = 48000;

} elsif ($arg_rate =~ /^44100(?:\.0*)?$/) {
  $arg_rate = 44100;
  
} else {
  die "Invalid sampling rate '$arg_rate', stopped";
}

# Convert frequency and amplitude arguments to floats and check ranges
#
for(my $i = 0; $i < 2; $i++) {
  # Get argument and name of argument
  my $str;
  my $aname;
  
  if ($i == 0) {
    $str = $arg_freq;
    $aname = "frequency";
  
  } elsif ($i == 1) {
    $str = $arg_ampl;
    $aname = "amplitude";
  
  } else {
    die "Unexpected";
  }
  
  # Check format
  ($str =~ /^[1-9][0-9]{0,4}(?:\.[0-9]*)?$/) or
    die "Invalid $aname argument '$str', stopped";
  
  # Convert argument to number
  $str = $str + 0;
  
  # Replace argument
  if ($i == 0) {
    $arg_freq = $str;
  
  } elsif ($i == 1) {
    $arg_ampl = $str;
    
  } else {
    die "Unexpected";
  }
}

(($arg_freq >= 1.0) and ($arg_freq <= 20000.0)) or
  die "Frequency argument must be in range [1, 20000], stopped";

(($arg_ampl >= 1.0) and ($arg_ampl <= 32767.0)) or
  die "Amplitude argument must be in range [1, 32767], stopped";

# Convert repetition argument to integer and check range
#
($arg_repc =~ /^[1-9][0-9]{0,9}(?:\.0*)?$/) or
  die "Invalid repetition argument '$arg_repc', stopped";

$arg_repc = int($arg_repc);
(($arg_repc > 0) and (($arg_repc / $arg_freq) <= 60)) or
  die "Repetition count out of range, stopped";

# Open the output file in raw mode
#
open(my $fh, "> :raw", $arg_path) or
  die "Can't create output file '$arg_path', stopped";

# Compute the total number of samples that we are going to write
#
my $samp_count = int(($arg_repc * $arg_rate) / $arg_freq);
unless ($samp_count >= 1) {
  $samp_count = 1;
}

# Compute the raw data size in bytes
#
my $data_size = $samp_count * 2;

# Compute the size of the whole output WAV file, less eight bytes for
# the RIFF chunk marker
#
my $riff_size = 4 + 24 + ($data_size + 8);

# Pack the binary WAV header, everything before the data samples
#
my $wav_head = pack(
  'A4VA4' . 'A4V' . 'vvVVvv' . 'A4V',
  'RIFF',
  $riff_size,
  'WAVE',
  'fmt ',
  16,
  1,
  1,
  $arg_rate,
  $arg_rate * 2,
  2,
  16,
  'data',
  $data_size
);

# Print the header
#
print { $fh } $wav_head;

# Compute the length in samples (floating-point) of one full wave
#
my $wave_len = $arg_rate / $arg_freq;

# Generate all the samples
#
for(my $t = 0; $t < $samp_count; $t++) {
  
  # Compute the normalized wave offset [0, 1) at this time point
  my $nwo = POSIX::fmod($t, $wave_len) / $wave_len;
  
  # Compute the wave value at this time point
  my $v = sin($nwo * 2 * pi) * $arg_ampl;
  
  # Floor and clamp range
  $v = POSIX::floor($v);
  unless ($v >= -32767) {
    $v = -32767;
  }
  unless ($v <= 32767) {
    $v = 32767;
  }
  
  # Pack and print this sample
  $v = pack('v', $v);
  print { $fh } $v;
}

# Close the output file
#
close($fh);

=head1 AUTHOR

Noah Johnson, C<noah.johnson@loupmail.com>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2022 Multimedia Data Technology Inc.

MIT License:

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction, including
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
