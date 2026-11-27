# Experiment Safety

RES-EXP-017. `src/core/experiments/Safety.h`.

## Principles

- No physical output (speaker, ultrasonic transducer, shaker, piezo
  actuator, RF transmitter, network analyser test port) starts without an
  **output authorization**, and the only way to obtain one is an explicit act
  of a named operator.
- Opening a project, loading an experiment, importing an archive or
  starting the application never authorizes anything: authorizations live in
  memory only and are never saved.
- Hardware errors and loss of control communication are safety-relevant:
  outputs stop.
- A software limit is not a hardware interlock, and a warning does not
  establish regulatory or exposure compliance.

## Limits

`SafetyLimits` per experiment: maximum command amplitude (0 = no output),
permitted band, duty cycle, longest output, permitted output channels,
whether a hardware interlock must be closed, maximum RF transmit power (none
by default: no RF transmission), the level above which an acoustic exposure
warning is shown, and a note of the regulatory or test-site conditions an RF
run relies on. `checkOutput` lists every violation (any one forbids the
output) and the warnings: RF regulatory compliance, ultrasonic and acoustic
exposure, mechanical resonance and load limits, interlock.

## Authorization

`OutputAuthority::grant(request, limits, operator, acknowledgement)` - the
operator's name and acknowledgement are required; the request (channels,
peak amplitude, band, duty cycle, total output time, transmit power) must be
within the limits; the emergency stop must not be latched.

- One run: the run consumes it when its outputs start and releases it at the
  end; it is then spent. Unused, it expires (10 minutes by default).
- It is for one experiment as it was: a run checks that the experiment still
  asks for what was authorized; a changed experiment needs a new
  authorization (the application drops it when the definition changes).
- Devices check it for every output block and stop when it is no longer
  valid.

In the application: Run > Authorize Output... opens a dialog with the
request and the warnings; Authorize is enabled only with the operator's
name, the acknowledgement ticked and the limits met. On the command line:
`--authorize-output "<operator>"`.

## Emergency stop

The red EMERGENCY STOP button and Experiments > EMERGENCY STOP
(Ctrl+Shift+Space) latch the stop: every output session stops at its next
block, every authorization is revoked, a running experiment is cancelled
(its data is kept, marked incomplete), and no authorization is granted until
an operator resets the stop.

## Events

Grants, refusals, consumption, release, revocation, emergency stop, reset,
outputs stopped and device errors are recorded with their time
(`OutputAuthority::events()`).
