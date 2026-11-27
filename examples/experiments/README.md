# Experiment examples

Experiments on Resonant's **simulated instruments** (`docs/experiments/HARDWARE_INTEGRATIONS.md`):
their data is generated from stated synthetic models and marked as such; it
tests the measurement chain and the procedures, it is not a measurement.

| File | Experiments |
|---|---|
| `acoustic_experiments.resonant` | speaker characterization, field mapping (2 kHz), focusing validation, ultrasonic element A |
| `mechanical_experiments.resonant` | modal test, vibration characterization (120 Hz), shaker calibration |
| `rf_experiments.resonant` | antenna impedance (VNA), array channel calibration, radiation pattern |
| `rlc_antenna_and_line.s2p` | synthetic two-port data for the Touchstone import |

To run one: save the project somewhere writable (its data goes next to it),
open the Experimental Hardware dock, select the experiment, Run > Authorize
Output... (operator name and acknowledgement), then Run. Or:

```
resonant my_copy.resonant --run-experiment ex-exp-field-map --authorize-output "your name"
```
