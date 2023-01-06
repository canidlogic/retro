package Retro::Graph;
use v5.14;
use warnings;

=head1 NAME

Retro::Graph - Abstract graph object superclass.

=head1 SYNOPSIS

  # Create one of the graph subclasses
  my $graph = ...
  
  # Determine whether this is a local graph
  if ($graph->is_local) {
    ...
  }
  
  # Get the value of the graph at t, along with a count of how many
  # cycles this value is repeated, or -1 if repeated indefinitely
  my ($val, $repeat) = $graph->get($t);
  
  # Get the minimum and maximum of all the values produced by this graph
  my ($min_val, $max_val) = $graph->bounds;

=head1 DESCRIPTION

Abstract interface that all the different graph objects support.

All graphs must support a function C<is_local()> that returns whether
the graph is a local or global graph.  All graphs must also support a
function C<bounds()> that returns the boundaries of the range of values
produced by this graph.

All graphs must support a function C<get()> that computes the graph at a
given time point.

You must construct one of the subclasses, and subclasses must override
these two base methods.

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<is_local()>

Return 1 if this is a local graph, or 0 if this is a global graph.

For local graphs, a C<t> value of zero passed to the C<get()> function
means the first cycle of a melodic event.

For global graphs, a C<t> value of zero passed to the C<get()> function
means the first cycle of the whole performance.

=cut

sub is_local {
  die "Abstract base class";
}

=item B<get(t)>

Get the value of the graph at a time point C<t>.

C<t> must pass the C<rtIsInteger()> function of C<Retro::Util> and it
must be zero or greater.

For local graphs, C<t> zero means the start of a melodic event.  For
global graphs, C<t> zero means the start of the whole performance.

The return value in list context has two values.  The first value is the
value of the graph at the requested C<t> value.  This will be an integer
in range [0, 131072].

The second return value is an integer C<r>.  If C<r> has the special
value -1, then the first value will also be returned for all C<t> values
greater than or equal to the C<t> value just computed.  Otherwise, C<r>
is an integer one or greater, and the graph will return the first return
value for all C<t> in range C<[t, t+r-1]>.

In other words, the second return value indicates how many times the
graph will repeat this specific return value.  This is an optimization
to avoid having to consult the graph every cycle.

=cut

sub get {
  die "Abstract base class";
}

=item B<bounds()>

Get the boundaries of the full range of values produced by the graph.

Returns two integer values.  The first is the minimum value and the
second is the maximum value.

=cut

sub bounds {
  die "Abstract base class";
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
