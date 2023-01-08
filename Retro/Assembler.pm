package Retro::Assembler;
use v5.14;
use warnings;

# Retro imports
use Retro::Util qw(
  rtIsInteger
);

=head1 NAME

Retro::Assembler - OPL2 hardware script assembler module.

=head1 SYNOPSIS

  use Retro::Assembler;
  
  # Construct a new assembler instance with 60 Hz control rate
  my $asm = Retro::Assembler->create(60);
  
  # Define register writes in any chronological order
  $asm->reg($t, $addr, $val);
  
  # Update total duration of assembly, possbily multiple times
  $asm->dur($total_duration);
  
  # Build the whole assembly file
  $asm->build("path/to/file.opl2");

=head1 DESCRIPTION

Assemble an OPL2 hardware script given register write instructions and
total duration information in any chronological order.

When the assembler is constructed, you must specify the control rate in
Hz.  The assembler starts out empty with total duration zero and no
register writes defined.

You can then issue C<reg()> and C<dur()> calls in any chronological
order whatsoever.  C<reg()> calls schedule a hardware register write at
a certain point in time, and C<dur()> calls increase the total duration
of the performance if necessary.

When all the information has been specified, call C<build()> to create
the completed hardware script.  All the register writes that were
scheduled will be sorted in chronological order, and the hardware script
will proceed through time sequentially.

Within each time point, the order of the hardware register writes
follows a few rules to keep the state of the OPL2 consistent.  See the
documentation of C<build()> for further information.

=cut

# ===============
# Local functions
# ===============

# _order_writes($fh, \%reg_map)
# -----------------------------
#
# Output a sequence of "r" instructions at the same time point, with the
# registers in the proper order within the time point.
#
# The hash map has keys that are two lowercase base-16 digits.  The
# values are integers in range 0-255.
#

# Maps all the keys of registers having special ordering rules to value
# of one.
#
my %ORDERED_REGISTERS = (
  '01' => 1,
  '08' => 1,
  'b0' => 1, 'b1' => 1, 'b2' => 1, 'b3' => 1, 'b4' => 1,
  'b5' => 1, 'b6' => 1, 'b7' => 1, 'b8' => 1,
  'bd' => 1
);

sub _order_writes {
  # Get parameters
  ($#_ == 1) or die;
  
  my $fh = shift;
  
  my $reg_map = shift;
  (ref($reg_map) eq 'HASH') or die;
  
  # Write registers 01 and 08 first if they are within the hash
  if (defined $reg_map->{'01'}) {
    printf { $fh } "r 01 %02x\n", $reg_map->{'01'};
  }
  if (defined $reg_map->{'08'}) {
    printf { $fh } "r 08 %02x\n", $reg_map->{'08'};
  }
  
  # Write register BD here only if its rhythm mode flag is OFF
  if (defined $reg_map->{'bd'}) {
    if (($reg_map->{'bd'} & 0x20) == 0) {
      printf { $fh } "r bd %02x\n", $reg_map->{'bd'};
    }
  }
  
  # For the registers B0-B8, write them here only if their key-down flag
  # is OFF
  for(my $reg = 0xb0; $reg <= 0xb8; $reg++) {
    my $addr = sprintf("%02x", $reg);
    if (defined $reg_map->{$addr}) {
      if (($reg_map->{$addr} & 0x20) == 0) {
        printf { $fh } "r %s %02x\n", $addr, $reg_map->{$addr};
      }
    }
  }
  
  # Write all registers except 01, 08, B0-B8, and BD here
  for my $k (keys %$reg_map) {
    # Skip keys with special ordering rules
    if (defined $ORDERED_REGISTERS{$k}) {
      next;
    }
    
    # Make sure key has proper format
    ($k =~ /^[0-9a-f]{2}$/) or die;
    
    # Write the register
    printf { $fh } "r %s %02x\n", $k, $reg_map->{$k};
  }
  
  # For the registers B0-B8, write them here only if their key-down flag
  # is ON
  for(my $reg = 0xb0; $reg <= 0xb8; $reg++) {
    my $addr = sprintf("%02x", $reg);
    if (defined $reg_map->{$addr}) {
      if (($reg_map->{$addr} & 0x20) != 0) {
        printf { $fh } "r %s %02x\n", $addr, $reg_map->{$addr};
      }
    }
  }
  
  # Write register BD here only if its rhythm mode flag is ON
  if (defined $reg_map->{'bd'}) {
    if (($reg_map->{'bd'} & 0x20) != 0) {
      printf { $fh } "r bd %02x\n", $reg_map->{'bd'};
    }
  }
}

=head1 CONSTRUCTOR

=over 4

=item B<create(rate)>

Create a new OPL2 assembler instance.

C<rate> is the control rate to declare in the header of the hardware
script that will be generated by C<build()>.  It must be an integer in
range [1, 1024], specifying the rate in Hz.

The assembler starts out with no scheduled writes and a total duration
of zero.  Both C<reg()> and C<dur()> calls will possibly lengthen the
total duration.  C<reg()> will add a scheduled write.  Scheduled writes
are buffered in memory, with each scheduled write packed into a single
integer value.

When all writes have been scheduled and the total duration is correct,
use C<build()> to write a sequential hardware script and reset the state
of the assembler to its initial state.

=cut

sub create {
  # Get parameters
  ($#_ == 1) or die;
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $rate = shift;
  rtIsInteger($rate) or die;
  (($rate >= 1) and ($rate <= 1024)) or die;
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # '_rate' stores the control rate in Hz.
  $self->{'_rate'} = $rate;
  
  # '_dur' is the total duration in cycles.
  $self->{'_dur'} = 0;
  
  # '_comp' is an array storing all the scheduled writes in the
  # composition.  Each element is a scalar integer that encodes the
  # write command in the following manner.  The eight least significant
  # bits are the binary value to write into the register.  The eight
  # bits above that are the register address.  The 31 bits above that
  # are the time offset of the write event in cycles:
  #
  # Bits: 46-16        | 15-8 | 7-0
  #       -------------+------+----
  #       Time offset  | Addr | Val
  #
  # The events are recorded in this array in the order they were
  # defined, NOT in chronological order!
  $self->{'_comp'} = [];
  
  # Return the new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<reg(t, addr, val)>

Schedule a register write.

C<t> is the offset in time of the register write, expressed in the
number of cycles at the control rate that have elapsed since the start
of the performance.  It must pass C<rtIsInteger()> from C<Retro::Util>
and it must be zero or greater.  It also must be one less than the
maximum integer value, for a full range of [0, 2147483646].

You may schedule register write events in any order.  They do not have
to be added chronologically.

C<addr> is the OPL2 register address to write.  It must pass
C<rtIsInteger()> and its range must be [0, 255].

C<val> is the value to write into the OPL2 register at address C<addr>
at time C<t>.  It must pass C<rtIsInteger()> and its range must be
[0, 255].

All the scheduled register writes will be buffered in memory until the
C<build()> function is called.  Each scheduled write takes up one scalar
integer worth of memory in an array.

This function will automatically extend the total duration of the
composition if necessary to include the given C<t> value, as if C<dur>
was called with a value one greater than C<t>.

=cut

sub reg {
  # Get self and parameters
  ($#_ == 3) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $t = shift;
  rtIsInteger($t) or die;
  (($t >= 0) and ($t <= 2147483646)) or die;
  
  my $addr = shift;
  rtIsInteger($addr) or die;
  (($addr >= 0) and ($addr <= 255)) or die;
  
  my $val = shift;
  rtIsInteger($val) or die;
  (($val >= 0) and ($val <= 255)) or die;
  
  # Pack the parameter values together into a single integer
  my $iv = ($t * 65536) + ($addr * 256) + $val;
  
  # Add the packed integer to the composition
  push @{$self->{'_comp'}}, ($iv);
  
  # Extend the duration if necessary
  unless ($self->{'_dur'} > $t) {
    $self->{'_dur'} = $t + 1;
  }
}

=item B<dur(d)>

Extend the length of the performance if necessary to the given duration.

C<d> is the total duration of the performance, expressed in a number of
cycles at the control rate.  C<d> must pass C<rtIsInteger()> from the
C<Retro::Util> module, and it must be zero or greater.

If the current total duration of the performance is less than C<d>, it
will be updated to C<d>.  However, if the current total duration of the
performance is greater than or equal to C<d>, this method call is
ignored.

In other words, this method can only extend the length of a performance.
It can not shorten the length.

=cut

sub dur {
  # Get self and parameters
  ($#_ == 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $d = shift;
  rtIsInteger($d) or die;
  ($d >= 0) or die;
  
  # Extend the duration if necessary
  unless ($self->{'_dur'} >= $d) {
    $self->{'_dur'} = $d;
  }
}

=item B<build(path)>

Build a full OPL2 assembly file.

C<path> is the path to the file that should be created or overwritten
with the generated OPL2 hardware script.  The file format of this
hardware script is documented in the Retro Specification.

Before using this function, schedule all necessary hardware writes with
C<reg()> and make sure the total performance duration is as you want it,
possibly using C<dur()> one or more times to update the total duration.

This function will sort all the scheduled writes in chronological order
and then build the hardware script so that all writes take place at the
scheduled time and the total of the hardware script matches the total
duration established in this assembler object.

The state of the assembler object will be cleared back to initial state
after this function completes.

Fatal errors occur if there are any problems building the script.  It is
not an error if two or more writes to the same register are scheduled at
the same time, provided that all the writes have the same value.  If any
of the writes have a different value, however, an error occurs.  In the
output hardware script, redundant writes will be dropped.

To ensure a consistent OPL2 state within each time point, hardware
writes scheduled at the same point will be ordered according to the
following system:

  1. Register 01
  2. Register 08
  3. Register BD if rhythm mode flag is OFF
  4. Registers B0-B8 which have their key-down flag OFF
  5. All registers except 01, 08, B0-B8, and BD
  6. Registers B0-B8 which have their key-down flag ON
  7. Register BD if rhythm mode flag is ON

=cut

sub build {
  # Get self and parameters
  ($#_ == 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $path = shift;
  (not ref($path)) or die;
  
  # Open the output file at the given path
  open(my $fh, ">", $path) or
    die "Failed to open output file '$path'!\n";
  
  # Wrap rest of function in an eval that closes the file on the way out
  # and attempts to delete the file, too, in case of error
  eval {
    
    # First we need to get all the scheduled writes in chronological
    # order; we can do this by sorting them in ascending numerical
    # order, since the time offset is the most significant part of the
    # packed write instructions
    my @wlist = sort { $a <=> $b } @{$self->{'_comp'}};
    
    # Grab the total duration and then reset the object state
    my $total_dur = $self->{'_dur'};
    
    $self->{'_dur'}  = 0;
    $self->{'_comp'} = [];
    
    # Print the OPL2 header line
    printf { $fh } "OPL2 %u\n", $self->{'_rate'};
    
    # The t value starts out at zero
    my $t = 0;
    
    # The reg_map dictionary maps lowercase two-digit base-16 addresses
    # to scalar values 0-255 within a time point, or undef if not
    # defined yet
    my $reg_map = undef;
    
    # Process all the sorted register write instructions
    for my $rec (@wlist) {
      # Unpack this record
      my $offs = int($rec / 65536);
      my $lo   = $rec % 65536;
      
      my $addr = $lo >>   8;
      my $val  = $lo & 0xff;
      
      # If our current t value is less than the offset of this record,
      # then output current register map if defined and insert a wait
      # instruction bringing the t value up to the current record's
      # offset
      if ($t < $offs) {
        if (defined $reg_map) {
          _order_writes($fh, $reg_map);
          $reg_map = undef;
        }
        printf { $fh } "w %u\n", ($offs - $t);
        $t = $offs;
      }
      
      # If register map not defined, define a new one
      unless (defined $reg_map) {
        $reg_map = {};
      }
      
      # Convert address to string format
      $addr = sprintf("%02x", $addr);
      
      # Check whether address already in register map
      if (defined $reg_map->{$addr}) {
        # Register write already defined; make sure value is the same
        ($reg_map->{$addr} == $val) or
          die "Inconsistent hardware writes in assembler!\n";
        
      } else {
        # Register write not already defined, so define it
        $reg_map->{$addr} = $val;
      }
    }
    
    # If register map is defined, write it
    if (defined $reg_map) {
      _order_writes($fh, $reg_map);
      $reg_map = undef;
    }
    
    # If the t value is less than the total duration, add a wait
    # instruction bringing the performance up to the total duration
    if ($t < $total_dur) {
      printf { $fh } "w %u\n", ($total_dur - $t);
    }
    
  };
  if ($@) {
    # Failed to build the file, so close it and attempt to delete it
    close($fh) or warn "Failed to close file";
    unlink $path or warn "Failed to remove incomplete output file";
    
    # Rethrow the error
    die $@;
  }
  
  # If we got here, everything was OK so close the file
  close($fh) or warn "Failed to close file";
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
