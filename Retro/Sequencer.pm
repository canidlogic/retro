package Retro::Sequencer;
use v5.14;
use warnings;

# Retro imports
use Retro::Assembler;
use Retro::ChannelRegisters;
use Retro::Globals;
use Retro::Instrument;
use Retro::Util qw(
  rtIsInteger
);

=head1 NAME

Retro::Sequencer - Retro sequencer module.

=head1 SYNOPSIS

  use Retro::Sequencer;
  
  # Create a sequencer with the given control rate in Hz
  my $seq = Retro::Sequencer->create(60);
  
  # Set the global parameters dictionary, using global parameter keys
  # only, values may be integers, global graphs, or null
  $seq->globals(\%params);
  
  # Set the pseudo-channels R0, R1, R2 to the given Retro::Instrument
  # objects, or null values for default parameters
  $seq->pseudo($r0_instr, $r1_instr, $r2_instr);
  
  # Add a melodic event to the sequencer
  $seq->note($offs, $reserved, $audible, $instr, $f);
  
  # Add a rhythm event to the sequencer
  $seq->drum($offs, $reserved, $audible, $p);
  
  # Sequence the whole performance and generate an OPL2 script
  $seq->sequence("path/to/file.opl2");

=head1 DESCRIPTION

The core sequencer module of Retro.

Objects of this class receive the results of interpreting a Retro
script, and then the sequencer can compile everything into an OPL2
hardware script that allows the performance to be played back.

=cut

# ===============
# Local functions
# ===============

# _render_channel(asm, ch, instr, f, offs, dur, note, cs)
# -------------------------------------------------------
#
# Render all the hardware register changes for a channel and its
# operators for a given time space.
#
# asm is the Retro::Assembler to use for output.
#
# ch is the channel to render to.  This must be an integer in range
# [0, 8].
#
# instr is the Retro::Instrument to use for parameters.  If note flag is
# set, then both global and local graphs can be used in the instrument.
# If note flag is clear, then only global graphs can be used in the
# instrument.
#
# f is the F channel parameter override, or undef if there is no
# override.
#
# offs is the absolute time offset to start rendering at.  This must
# pass rtIsInteger() and be greater than or equal to zero.
#
# dur is the duration in cycles to render for.  This must pass
# rtIsInteger() and be greater than or equal to one if note flag is
# clear, or greater than or equal to two if note flag is set.  The
# (offs + dur) must be in integer range.
#
# note is the note flag.  If set, then dur is at least two, the key-down
# flag will be set for all cycles but the last in the duration, and the
# key-down flag will be clear for the last cycle in the duration.  If
# clear, then dur is at least one, and the key-down flag will be clear
# for all cycles.
#
# cs is the current ChannelState of this channel, or undef if the
# channel is currently in undefined state
#
# This function can both be used for rendering melodic events (if note
# flag is set) and for rendering pseudo-channels into channels 6-8 for
# rhythm mode (if note flag is clear).
#
# This function returns a Retro::ChannelRegisters object that represents
# the hardware register state of the channel after the last cycle.
#
sub _render_channel {
  # Get parameters
  ($#_ == 7) or die;
  
  my $asm   = shift;
  my $ch    = shift;
  my $instr = shift;
  my $f     = shift;
  my $offs  = shift;
  my $dur   = shift;
  my $note  = shift;
  my $cs    = shift;
  
  # Check types
  (ref($asm) and $asm->isa('Retro::Assembler')) or die;
  (ref($instr) and $instr->isa('Retro::Instrument')) or die;
  
  (rtIsInteger($ch) and rtIsInteger($offs) and rtIsInteger($dur)) or
    die;
  
  if (defined $f) {
    rtIsInteger($f) or die;
  }
  
  if (defined $cs) {
    (ref($cs) and $cs->isa('Retro::ChannelRegisters')) or die;
  }
  
  (not ref($note)) or die;
  if ($note) {
    $note = 1;
  } else {
    $note = 0;
  }
  
  # Check ranges
  (($ch >= 0) and ($ch <= 8)) or die;
  if (defined $f) {
    (($f >= 0) and ($f <= 117824)) or die;
  }
  ($offs >= 0) or die;
  if ($note) {
    ($dur >= 2) or die;
  } else {
    ($dur >= 1) or die;
  }
  rtIsInteger($offs + $dur) or die;
  
  # If not in note mode, then instrument may not have local graphs
  unless ($note) {
    (not $instr->has_local) or die;
  }
  
  # If in note mode, reduce duration by one so we can handle the
  # note-off separately
  if ($note) {
    $dur--;
  }
  
  # Define another ChannelRegisters for the new state and set t to zero
  my $ns = Retro::ChannelRegisters->create;
  my $t = 0;
  
  # Processing loop
  while (1) {
    # Set new state to resolved state at t, and get repeat value
    my $repeat;
    if ($note) {
      # Note mode, so both absolute and local time, and key-down
      $repeat = $ns->set($instr, $t + $offs, $t, 1, $f);
      
    } else {
      # Not note mode, so only absolute time and no key-down
      $repeat = $ns->set($instr, $t + $offs, undef, 0, $f);
    }
    
    # Schedule instructions for moving to this new state
    $ns->assemble($asm, $t + $offs, $ch, $cs);
    
    # If current state undefined, define it
    unless (defined $cs) {
      $cs = Retro::ChannelRegisters->create;
    }
    
    # Copy new state into current state
    $cs->copy($ns);
    
    # If repeat count is -1 meaning no further changes, or t+r exceeds
    # the span of t values covered by this rendering request, leave the
    # loop
    if (($repeat < 0) or ($t + $repeat >= $dur)) {
      last;
    } 
    
    # Update t to next change in parameters
    $t += $repeat;
  }
  
  # If we are in note mode, then we reduced duration by one earlier so
  # now we need to write one after the updated end of the event, turning
  # the note off
  if ($note) {
    $ns->set($instr, $offs + $dur, $dur, 0, $f);
    $ns->assemble($asm, $offs + $dur, $ch, $cs);
  }
  
  # Return the new hardware state
  return $ns;
}

=head1 CONSTRUCTOR

=over 4

=item B<create(rate)>

Create a new sequencer instance.

C<rate> is the control rate in Hz.  It must pass C<rtIsInteger()> from
C<Retro::Util> and it must be in range [1, 1024].

The sequencer starts out with global parameters set to the default
values defined by the Retro specification, pseudo-channels R0, R1, and
R2 set to null values indicating use default channel and operator
parameters, and no melodic events or rhythm events defined.

After definition, modify the state of the sequencer until all the
performance data has been loaded.  Finally, call C<sequence()> to
sequence all the data loaded in sequencer into an OPL2 hardware script.

=cut

sub create {
  # Get parameters
  ($#_ == 1) or die;
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $rate = shift;
  (rtIsInteger($rate) and ($rate >= 1) and ($rate <= 1024)) or die;
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # '_closed' will be set once the sequence() function is called, to
  # prevent further use of the object after that
  $self->{'_closed'} = 0;
  
  # '_rate' stores the control rate
  $self->{'_rate'} = $rate;
  
  # '_global' is a Retro::Globals object that tracks global parameter
  # values and also stores the rhythm map
  $self->{'_global'} = Retro::Globals->create;
  
  # '_pseudo' is an array reference to the R0, R1, and R2 instruments,
  # or undef values to use default channel and operator parameters
  $self->{'_pseudo'} = [undef, undef, undef];
  
  # '_nm' is the array of all melodic events defined, with the records
  # in no particular order; records are subarrays storing:
  #
  #   0. Time offset
  #   1. Reserved duration
  #   2. Audible duration
  #   3. Retro::Instrument reference
  #   4. F parameter, or undef
  #
  $self->{'_nm'} = [];
  
  # '_rm' is the array of all rhythm events defined, with the records in
  # no particular order; records are subarrays storing:
  #
  #   0. Time offset
  #   1. Reserved duration
  #   2. Audible duration
  #   3. Percussion instrument index
  #
  $self->{'_rm'} = [];
  
  # Return the new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE METHODS

=over 4

=item B<globals(dict)>

Update global parameters with the given hash reference.

This call is passed through to the C<update()> method of an internal
C<Retro::Globals> object.  See the documentation of that function for
further information.

=cut

sub globals {
  # Get self and parameters
  ($#_ == 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $dict = shift;
  (ref($dict) eq 'HASH') or die;
  
  # Check that sequencer not closed
  (not $self->{'_closed'}) or die;
  
  # Update global state
  $self->{'_global'}->update($dict);
}

=item B<pseudo(r0, r1, r2)>

Set the instruments used to define pseudo-channels R0, R1, and R2.

Each parameter must either be a C<Retro::Instrument> object or C<undef>.
Any current state of the three pseudo-channels is completely overwritten
by each call to this function.  Passing C<undef> values stores a null
reference within the pseudo-channel, which will be interpreted as all
default channel and operator parameters for that channel.

The passed instruments may not include any local graphs.  An error with
a user-friendly message will be thrown if the instrument includes local
graphs.

This function may not be used after any rhythm events have been defined.
An error with a user-friendly message is thrown if this happens.

=cut

sub pseudo {
  # Get self
  ($#_ >= 0) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  # Check that three parameters remaining, and that each are either
  # instruments with no local graphs or undef
  ($#_ == 2) or die;
  for my $p (@_) {
    if (defined $p) {
      (ref($p) and $p->isa('Retro::Instrument')) or die;
      (not $p->has_local) or
        die "Pseudo-channels may not have local graphs!\n";
    }
  }
  
  # Check that sequencer not closed
  (not $self->{'_closed'}) or die;

  # Check that no rhythm events defined yet
  (scalar(@{$self->{'_rm'}}) < 1) or
    die "Can't change pseudo-channels after rhythm event defined!\n";
  
  # Update pseudo-channels
  $self->{'_pseudo'}->[0] = shift;
  $self->{'_pseudo'}->[1] = shift;
  $self->{'_pseudo'}->[2] = shift;
}

=item B<note(offs, reserved, audible, instr, f)>

Add a melodic event to the sequencer.

C<offs> is the time offset of the start of the melodic event.  It must
pass C<rtIsInteger()> from C<Retro::Util> and be greater than or equal
to zero.  If the time offset is negative, an error with a user-friendly
message will be thrown.  This is an absolute time, where t=0 is the
start of the performance.

C<reserved> is the reserved duration of the melodic event.  It must pass
C<rtIsInteger()> and it must be greater than the audible duration.  An
error with a user-friendly message will be thrown if it is not longer
than the audible duration.

C<audible> is the audible duration of the melodic event.  If must pass
C<rtIsInteger()> and it must be greater than zero.  An error with a
user-friendly message will be thrown if it is less than one.

The C<reserved> duration added to C<offs> must not exceed the range of
C<rtIsInteger()> or an error with a user-friendly message will be
thrown.

C<instr> is the C<Retro::Instrument> object used for this melodic event.
It may use both local and global graphs.

C<f> is the C<F> channel parameter override for this event, or C<undef>.
If defined, it must pass C<rtIsInteger()> and it must be in the range
[0, 117824].  An error with a user-friendly message will be throw if an
integer with an out-of-range value is passed.

Melodic events may be defined in any order.  They do not have to be
chronological.

=cut

sub note {
  # Get self and parameters
  ($#_ == 5) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $offs     = shift;
  my $reserved = shift;
  my $audible  = shift;
  my $instr    = shift;
  my $f        = shift;
  
  (rtIsInteger($offs) and rtIsInteger($reserved) and
    rtIsInteger($audible)) or die;
  
  (ref($instr) and $instr->isa('Retro::Instrument')) or die;
  
  if (defined $f) {
    rtIsInteger($f) or die;
  }
  
  ($offs >= 0) or die "Event offset must be zero or greater!\n";
  ($audible >= 1) or die "Audible duration must be at least one!\n";
  ($reserved > $audible) or
    die "Reserved duration must be greater than audible!\n";
  rtIsInteger($offs + $reserved) or
    die "Event duration exceeds integer space!\n";
  
  if (defined $f) {
    (($f >= 0) and ($f <= 117824)) or
      die "Event frequency is out of range!\n";
  }
  
  # Check that sequencer not closed
  (not $self->{'_closed'}) or die;
  
  # Add event and make sure size of melodic array does not exceed
  # integer size
  push @{$self->{'_nm'}}, ([
    $offs, $reserved, $audible, $instr, $f
  ]);
  rtIsInteger(scalar(@{$self->{'_nm'}})) or
    die "Too many melodic events defined!\n";
}

=item B<drum(offs, reserved, audible, p)>

Add a rhythm event to the sequencer.

C<offs> is the time offset of the start of the rhythm event.  It must
pass C<rtIsInteger()> from C<Retro::Util> and be greater than or equal
to zero.  If the time offset is negative, an error with a user-friendly
message will be thrown.  This is an absolute time, where t=0 is the
start of the performance.

C<reserved> is the reserved duration of the rhythm event.  It must pass
C<rtIsInteger()> and it must be greater than the audible duration.  An
error with a user-friendly message will be thrown if it is not longer
than the audible duration.

C<audible> is the audible duration of the rhythm event.  If must pass
C<rtIsInteger()> and it must be greater than zero.  An error with a
user-friendly message will be thrown if it is less than one.

The C<reserved> duration added to C<offs> must not exceed the range of
C<rtIsInteger()> or an error with a user-friendly message will be
thrown.

C<p> is the rhythm instrument to play.  It is an integer with one of the
following values:

  0 - Bass Drum
  1 - Snare Drum
  2 - Tom-tom
  3 - Cymbal
  4 - Hi-Hat

An error with a user-friendly message is thrown if the C<p> value is not
a recognized value.

Rhythm events may be defined in any order.  They do not have to be
chronological.

=cut

sub drum {
  # Get self and parameters
  ($#_ == 4) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $offs     = shift;
  my $reserved = shift;
  my $audible  = shift;
  my $p        = shift;
  
  (rtIsInteger($offs) and rtIsInteger($reserved) and
    rtIsInteger($audible) and rtIsInteger($p)) or die;
  
  ($offs >= 0) or die "Event offset must be zero or greater!\n";
  ($audible >= 1) or die "Audible duration must be at least one!\n";
  ($reserved > $audible) or
    die "Reserved duration must be greater than audible!\n";
  rtIsInteger($offs + $reserved) or
    die "Event duration exceeds integer space!\n";
  
  (($p >= 0) and ($p <= 4)) or
    die "Unrecognized percussion instrument index!\n";
  
  # Check that sequencer not closed
  (not $self->{'_closed'}) or die;
  
  # Add event and make sure size of rhythm array does not exceed integer
  # size
  push @{$self->{'_rm'}}, ([
    $offs, $reserved, $audible, $p
  ]);
  rtIsInteger(scalar(@{$self->{'_rm'}})) or
    die "Too many rhythm events defined!\n";
}

=item B<sequence(path)>

Sequence all the events and parameter changes and build a full OPL2
assembly file for the performance.

C<path> is the path to the file that should be created or overwritten
with the generated OPL2 hardware script.  The file format of this
hardware script is documented in the Retro Specification.

The sequencer object is closed and can not be used further after this
function is called.

=cut

sub sequence {
  # Get self and parameters
  ($#_ == 1) or die;
  
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or die;
  
  my $path = shift;
  (not ref($path)) or die;
  
  # Check that sequencer not closed
  (not $self->{'_closed'}) or die;
  
  # Close the sequencer
  $self->{'_closed'} = 1;
  
  # Replace undef pseudo-channel instruments with default instruments
  for my $pci (@{$self->{'_pseudo'}}) {
    unless (defined $pci) {
      $pci = Retro::Instrument->create(undef, undef, undef, undef);
    }
  }
  
  # Construct an assembler that will store output
  my $asm = Retro::Assembler->create($self->{'_rate'});
  
  # Sort melodic and rhythmic events in chronological order
  my @mev = sort { $a->[0] <=> $b->[0] } @{$self->{'_nm'}};
  $self->{'_nm'} = undef;
  
  my @rev = sort { $a->[0] <=> $b->[0] } @{$self->{'_rm'}};
  $self->{'_rm'} = undef;
  
  # regs is an array of nine elements for channels 0-8 storing the
  # current hardware register state for each channel; each element is
  # either a ChannelRegisters object, or it is undef if the channel is
  # currently in undefined state
  my @regs = (
    undef, undef, undef,
    undef, undef, undef,
    undef, undef, undef
  );
  
  # occupy is an array of nine elements for channels 0-9 storing the
  # last reserved time point t used for the most recent event in this
  # channel; initialized with everything -1 as if most recent event
  # ended before start of performance
  my @occupy = (
    -1, -1, -1,
    -1, -1, -1,
    -1, -1, -1
  );
  
  # The initial object will be used for assigning melodic events to
  # channels
  my $initial = Retro::ChannelRegisters->create;
  
  # The instrument map is used for rendering groups of rhythm events;
  # each value is the last t value for the most recent use of the
  # instrument, or -1 if the instrument hasn't been used yet
  my @inmap = (-1, -1, -1, -1, -1);
  
  # Keep processing events until there are no events left to process
  while ((scalar(@mev) > 0) or (scalar(@rev) > 0)) {
    # Decide whether we will be processing a melodic or rhythm event
    # next based on which array is non-empty and, if that is not
    # sufficient to decide, which event comes first
    my $process_melodic = 0;
    if ((scalar(@mev) > 0) and (scalar(@rev) > 0)) {
      # Both melodic and rhythm events left, so get time of each
      my $melodic_next = $mev[0]->[0];
      my $rhythm_next  = $rev[0]->[0];
      
      if ($melodic_next <= $rhythm_next) {
        # Next melodic event is before or at the same time as next
        # rhythm event, so process melodic next
        $process_melodic = 1;
        
      } else {
        # Next rhythm event is before next melodic event, so do not
        # process melodic next
        $process_melodic = 0;
      }
      
    } elsif (scalar(@mev) > 0) {
      # Only melodic events left, so process melodic
      $process_melodic = 1;
      
    } elsif (scalar(@rev) > 0) {
      # Only rhythm events left, so do not process melodic
      $process_melodic = 0;
      
    } else {
      die;
    }
    
    # Process either a melodic or rhythm event
    if ($process_melodic) { # ==========================================
      # Process melodic event -- get the event and parameters
      my $me = shift @mev;
      my $me_offs = $me->[0];
      my $me_res  = $me->[1];
      my $me_aud  = $me->[2];
      my $me_inst = $me->[3];
      my $me_f    = $me->[4];
      
      # Get the initial channel registers configuration for the first
      # cycle of this event so we can determine which is the best
      # channel to assign this event to
      $initial->set($me_inst, $me_offs, 0, 1, $me_f);
      
      # Figure out the time offset t of the next rhythm event, or undef
      # if there is no rhythm event; this will tell us whether we can
      # assign this event to channels 6-8
      my $next_rhythm_t = undef;
      if (scalar(@rev) > 0) {
        $next_rhythm_t = $rev[0]->[0];
      }
      
      # Maximum channel for this event is 8, unless the next rhythm
      # event t is defined and it is within the reserved duration of
      # this melodic event, in which case the maximum channel for this
      # event is 5, so that we won't overlap the rhythm channels
      my $max_channel = 8;
      if (defined $next_rhythm_t) {
        unless ($me_offs + $me_res <= $next_rhythm_t) {
          $max_channel = 5;
        }
      }
      
      # Figure out the best channel to assign this event to, based on
      # what is available due to current occupancy, the maximum channel
      # we just computed, and what has the lowest cost to change state
      my $best_channel = undef;
      my $lowest_cost  = undef;
      for(my $i = 0; $i <= $max_channel; $i++) {
        # Skip if occupancy value is not below the start of this event
        unless ($occupy[$i] < $me_offs) {
          next;
        }
        
        # Compute cost of using this channel
        my $cost = $initial->cost($regs[$i]);
        
        # Update statistics
        if (defined $lowest_cost) {
          if ($cost < $lowest_cost) {
            $best_channel = $i;
            $lowest_cost  = $cost;
          }
        } else {
          $best_channel = $i;
          $lowest_cost  = $cost;
        }
      }
      
      # Check that we figured out a channel assignment
      (defined $best_channel) or
        die sprintf("Failed to schedule melodic event at t=%u!\n",
                    $me_offs);
      
      # Update the occupy value for this channel so we don't reuse it
      # until the event is done
      $occupy[$best_channel] = $me_offs + $me_res - 1;
      
      # Render the melodic event in the channel and update the hardware
      # state of the channel
      $regs[$best_channel] = _render_channel(
        $asm, $best_channel, $me_inst, $me_f,
        $me_offs, $me_aud + 1, 1, $regs[$best_channel]
      );
      
      # Update the total duration of the performance if necessary to
      # include the whole reserved duration of the event
      $asm->dur($me_offs + $me_res);
      
    } else { # =========================================================
      # Process rhythm event -- we will actually process a sequence of
      # overlapping rhythm events at the same time, so our first task is
      # to determine how many rhythm events we are going to handle this
      # time
      my $t     = $rev[0]->[0] + $rev[0]->[1];
      my $count = 1;
      for(my $i = 1; $i < scalar(@rev); $i++) {
        # If the start of this rhythm event is more than one cycle
        # beyond the end of the greatest rhythm event extent so far, we
        # are done and have the count value so leave the loop
        if ($rev[$i]->[0] > $t) {
          last;
        }
        
        # This event either overlaps another rhythm event in this group
        # or it comes immediately after, so increase count by one and
        # compute the t value immediately after this event
        $count++;
        my $next_t = $rev[$i]->[0] + $rev[$i]->[1];
        
        # Update t to the maximum of its current value and next_t
        if ($next_t > $t) {
          $t = $next_t;
        }
      }
      
      # Now we know the t value one beyond the composite rhythm extent
      # in this group, define a composite offset and composite duration
      # covering the whole rhythm group we are processing
      my $comp_offs = $rev[0]->[0];
      my $comp_dur  = $t - $comp_offs;
      
      # Update the total duration of the performance if necessary to
      # include this whole rhythm group
      $asm->dur($comp_offs + $comp_dur);
      
      # Rhythm channels shouldn't be occupied at the start
      (($occupy[6] < $comp_offs) and
        ($occupy[7] < $comp_offs) and
        ($occupy[8] < $comp_offs)) or die;
      
      # Update occupancy of rhythm channels for the full rhythm group
      $occupy[6] = $comp_offs + $comp_dur - 1;
      $occupy[7] = $occupy[6];
      $occupy[8] = $occupy[6];
      
      # Reset the instrument map to nothing defined yet
      for my $imv (@inmap) {
        $imv = -1;
      }
      
      # Initialize time buffer to undefined; if defined, it has a range
      # of t values that need the pseudo-channels rendered, along with
      # one following t value
      my $time_buf = undef;
      
      # Initialize the event map to empty; this maps keys that are time
      # offsets in decimal to values storing an XOR mask for the
      # percussion instrument bits (excluding the rhythm mode flag)
      my $ev_map = {};
      
      # Process the group of rhythm events
      for(my $i = 0; $i < $count; $i++) {
        # Get next rhythm event and unpack
        my $re = shift @rev;
        my $re_offs = $re->[0];
        my $re_res  = $re->[1];
        my $re_aud  = $re->[2];
        my $re_p    = $re->[3];
        
        # Check that offset of this event is greater than most recent
        # time this instrument was used, or error
        ($inmap[$re_p] < $re_offs) or
          die "Percussion instrument used simultaneous with itself!\n";
        
        # Update instrument map for this instrument
        $inmap[$re_p] = $re_offs + $re_res - 1;
        
        # Add the span of this event to the time buffer, flushing the
        # time buffer first and rendering it if there is a gap between
        # its current value and the start of this event
        if (defined $time_buf) {
          # Time buffer defined; check whether there is a gap between
          # the end of the time buffer and the start of this event
          if ($re_offs > $time_buf->[1]) {
            # Gap so first render the pseudo-channels for all but the
            # last t value in the time buffer and update the hardware
            # state for channels 6-8
            for(my $j = 6; $j <= 8; $j++) {
              $regs[$j] = _render_channel(
                $asm, $j, $self->{'_pseudo'}->[$j - 6], undef,
                $time_buf->[0],
                $time_buf->[1] - $time_buf->[0],
                0, $regs[$j]
              );
            }
            
            # Start new time buffer using the new event
            $time_buf->[0] = $re_offs;
            $time_buf->[1] = $re_offs + $re_aud;
            
          } else {
            # No gap so we can just expand the time buffer if necessary
            # to also include this event
            my $next_offs = $re_offs + $re_aud;
            if ($next_offs > $time_buf->[1]) {
              $time_buf->[1] = $next_offs;
            }
          }
          
        } else {
          # Time buffer not defined yet, so initialize it with the
          # audible span of this event plus one
          $time_buf = [$re_offs, $re_offs + $re_aud];
        }
        
        # Get the string keys for the t_start and t_end of this rhythm
        # event
        my $t_start = sprintf("%d", $re_offs);
        my $t_end   = sprintf("%d", $re_offs + $re_aud);
        
        # Make sure event map is defined at t_start and t_end, adding
        # zero values if not defined yet
        unless (defined $ev_map->{$t_start}) {
          $ev_map->{$t_start} = 0;
        }
        unless (defined $ev_map->{$t_end}) {
          $ev_map->{$t_end} = 0;
        }
        
        # Get the flag value for this instrument
        my $instr_flag;
        if ($re_p == 0) {
          $instr_flag = 0x10;
        } elsif ($re_p == 1) {
          $instr_flag = 0x08;
        } elsif ($re_p == 2) {
          $instr_flag = 0x04;
        } elsif ($re_p == 3) {
          $instr_flag = 0x02;
        } elsif ($re_p == 4) {
          $instr_flag = 0x01;
        } else {
          die;
        }
        
        # Add the instrument flag to the t_start and t_end in the event
        # map so the instrument is toggled on at the start and toggled
        # off at the end
        $ev_map->{$t_start} |= $instr_flag;
        $ev_map->{$t_end  } |= $instr_flag;
      }
      
      # If time buffer is defined, render the pseudo-channels for all
      # but the last t value in the time buffer and update the hardware
      # state for channels 6-8
      if (defined $time_buf) {
        for(my $j = 6; $j <= 8; $j++) {
          $regs[$j] = _render_channel(
            $asm, $j, $self->{'_pseudo'}->[$j - 6], undef,
            $time_buf->[0],
            $time_buf->[1] - $time_buf->[0],
            0, $regs[$j]
          );
        }
      }
      
      # Rhythm flags value starts out with just the rhythm mode flag set
      my $r_flags = 0x20;
      
      # Go through the event map and add the necessary drum control
      # records to the global state
      for my $k (sort { $a <=> $b } keys %$ev_map) {
        # Get integer value of this key
        my $kt = int($k);
        
        # XOR rhythm flags with current value of event map
        $r_flags ^= $ev_map->{$k};
        
        # Add record to rhythm map
        $self->{'_global'}->drums($kt, $r_flags);
      }
      
      # After whole rhythm group, add a zero value to rhythm map to turn
      # off all the percussion
      $self->{'_global'}->drums($comp_offs + $comp_dur, 0);
      
    } # ================================================================
  }
  
  # We finished processing all events, we've scheduled all the hardware
  # writes for the channel and operator registers, and we've set the
  # total duration to go through the full reserved duration of all
  # events; now we need to schedule hardware writes for the global
  # registers and drum control flags from the rhythm map we've built
  $self->{'_global'}->assemble($asm);
  
  # If we got here without error, write the completed assembly file
  $asm->build($path);
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
