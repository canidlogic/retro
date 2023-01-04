package Retro::Util;
use v5.14;
use warnings;
use parent qw(Exporter);

# Core imports
use Scalar::Util qw(looks_like_number);

=head1 NAME

Retro::Util - Utility functions for Retro.

=head1 SYNOPSIS

  use Retro::Util qw(
    rtIsLong
    rtIsInteger
  );
  
  # Check if a given scalar is a valid long integer
  if (rtIsLong(29)) {
    ...
  }
  
  # Check if a given scalar is a valid integer
  if (rtIsInteger(-11)) {
    ...
  }

=head1 DESCRIPTION

Various Retro utility functions.  See the function documentation for
further information.

=head1 FUNCTIONS

=over 4

=item B<rtIsLong(value)>

Returns 1 if C<value> is a scalar long integer.  Else, returns 0.

This succeeds only if C<value> passes C<looks_like_number> from
<Scalar::Util>, its C<int()> conversion is equivalent, and its absolute
value does not exceed C<2^53 - 1> (the largest integer that can be
exactly stored within a double-precision floating point value).

=cut

use constant MAX_LONG =>  9007199254740991;
use constant MIN_LONG => -9007199254740991;

sub rtIsLong {
  # Get parameter
  ($#_ == 0) or die;
  my $val = shift;
  
  # Check that it is a number
  looks_like_number($val) or return 0;
  
  # Check that it is an integer
  ($val == int($val)) or return 0;
  
  # Check that it is in range
  if (($val >= MIN_LONG) and ($val <= MAX_LONG)) {
    return 1;
  } else {
    return 0;
  }
}

=item B<rtIsInteger(value)>

Returns 1 if C<value> is a scalar integer.  Else, returns 0.

This succeeds only if C<value> passes C<rtIsLong()> and C<value> can be
stored within a Retro integer type.  That is, the range must be
[-2147483647, 2147483647].

=cut

use constant MAX_INTEGER =>  2147483647;
use constant MIN_INTEGER => -2147483647;

sub rtIsInteger {
  # Get parameter
  ($#_ == 0) or die;
  my $val = shift;
  
  # Check it is a long integer
  (rtIsLong($val)) or return 0;
  
  # Check that it is in range
  if (($val >= MIN_INTEGER) and ($val <= MAX_INTEGER)) {
    return 1;
  } else {
    return 0;
  }
}

=back

=cut

# ==============
# Module exports
# ==============

our @EXPORT_OK = qw(
  rtIsLong
  rtIsInteger
);

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
