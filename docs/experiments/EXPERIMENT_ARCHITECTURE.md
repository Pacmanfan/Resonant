# Experiment Architecture

RES-EXP-001, RES-EXP-002, RES-EXP-016, RES-EXP-025. ADR-000213.

## Modules

```
src/core/experiments    domain-independent framework (no physics domain included)
  ExperimentTypes       quantities and units, data categories, sample formats, poses, processing steps
  Uncertainty           budgets of standard-uncertainty components
  Safety                limits, output authorization, emergency stop
  Hardware              devices, drivers, sessions, registry (the abstraction)
  SimulatedHardware     simulated instruments with physical models and fault injection
  Experiment            the experiment definition and its validation; stimulus generation
  Spatial               positions, scans, spatial data, plane reconstruction
  MeasurementRecord     the record of any data set
  MeasurementStore      HDF5 / binary storage next to the project
  Acquisition           reader + recorder threads, integrity checks, live view
  Calibration           records, the append-only library, application, procedures
  Processing            reproducible processing operations and chains
  Comparison            measurement versus simulation, generic metrics
  ExperimentRunner      the thirteen-step execution engine; the background job
  ExperimentReport      HTML reports
  ExperimentArchive     export, import, reproduction of processing
  ExperimentProject     the project's experiments (extensions.experiments)
src/core/dsp            Windows, SpectralEstimation, CrossCorrelation, Filters (shared signal processing)
src/core/acoustics/AcousticExperiments       acoustic interpretation and templates
src/core/mechanics/MechanicalExperiments     mechanical interpretation and templates
src/core/electromagnetics/RfExperiments      RF interpretation and templates; Touchstone (N-port)
src/hardware            resonant_hardware: ALSA, SoapySDR; registration of the drivers that were built
src/app                 ExperimentsController, ExperimentsPanel (forms/ExperimentsPanel.ui),
                        OutputAuthorizationDialog (forms/OutputAuthorizationDialog.ui)
```

Layering (checked by `tools/check_layering.py`): the core includes nothing
from `src/hardware`; `src/hardware` includes no Qt; `src/core/experiments`
includes no physics domain. Domains may include the framework.

## The experiment

A serializable definition (Experiment.h): identity, name, description,
domain, template, project, revision; device configurations; sensors;
actuators; channel bindings; acquisition settings; stimulus (with per-channel
delays and gains); trigger; synchronization (clock domains, strategy, timing
calibrations); positions and the positioner with its probe binding; network
analyser sweep; environment (recorded or assumed); safety limits; whether
calibration is required; the processing chain; execution history; results;
notes and annotations. It is stored in the project and validated without
hardware (`validateExperiment`).

## The run

`ExperimentRunner::run` (synchronous; `ExperimentJob` runs it in the
JobManager):

| Step | | Fails when |
|---|---|---|
| 1 | validate | the definition has errors |
| 2 | devices | a device is not found or cannot be opened |
| 3 | capabilities | a channel, rate or role does not exist on the device |
| 4 | calibrations | a selected record is missing, or unusable while calibration is required |
| 5 | synchronization | phase comparison is asked without a common clock or timing calibration |
| 6 | safety | the output violates the limits, or no valid authorization for this experiment |
| 7 | configure | a device refuses its configuration |
| 8 | arm | the trigger is not available (level triggering is not implemented) |
| 9 | execute | a device error or a lost connection (the partial data is kept, not processed) |
| 10 | record | - (the data is already on disk) |
| 11 | integrity | - (quality problems are warnings and are in every record) |
| 12 | process | an operation is not applicable (raw data kept) |
| 13 | report | - |

States: Draft, Validating, Ready, Armed, Running, Stopping, then Completed,
Failed or Cancelled. Cancellation and the emergency stop stop outputs first,
then acquisition; the recorded data sets are closed as incomplete.

## In the application

The Experimental Hardware dock (Devices, Experiments, Calibration, Run,
Measurements), the Experiments menu (panel, discover devices, EMERGENCY
STOP, Ctrl+Shift+Space), viewport markers (sensors green, actuators orange,
scan positions grey, measured points coloured by magnitude; nothing is drawn
between measured points), and the command line:

```
resonant project.resonant --run-experiment <id|first> [--authorize-output "<operator>"] [--screenshot file.png]
```

A run needs a saved project (its data is stored next to it). An experiment
with outputs runs only with `--authorize-output`.
