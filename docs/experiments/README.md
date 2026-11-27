# Experiments: Hardware, Measurement, Calibration and Simulation Correlation

Requirements: `docs/requirements/experiments.md` (RES-EXP-001..027). Decision: ADR-000213.

| Document | What it covers |
|---|---|
| [EXPERIMENT_ARCHITECTURE.md](EXPERIMENT_ARCHITECTURE.md) | Modules, layering, the experiment object, the run |
| [HARDWARE_ABSTRACTION.md](HARDWARE_ABSTRACTION.md) | Devices, drivers, capabilities, statuses, discovery |
| [DEVICE_DRIVER_FRAMEWORK.md](DEVICE_DRIVER_FRAMEWORK.md) | Writing a driver; the simulated drivers |
| [SENSOR_AND_ACTUATOR_MODELS.md](SENSOR_AND_ACTUATOR_MODELS.md) | Sensors, actuators, channel bindings, commanded output |
| [MEASUREMENT_DATA_MODEL.md](MEASUREMENT_DATA_MODEL.md) | Records, kinds, categories, the store, provenance |
| [ACQUISITION_AND_SYNCHRONIZATION.md](ACQUISITION_AND_SYNCHRONIZATION.md) | The acquisition engine, integrity checks, clocks |
| [CALIBRATION_FRAMEWORK.md](CALIBRATION_FRAMEWORK.md) | Calibration records, history, application, procedures |
| [SIGNAL_PROCESSING_PIPELINE.md](SIGNAL_PROCESSING_PIPELINE.md) | Processing operations, conventions, reproducibility |
| [SPATIAL_MEASUREMENT.md](SPATIAL_MEASUREMENT.md) | Positions, scans, spatial data, reconstruction |
| [SIMULATION_MEASUREMENT_CORRELATION.md](SIMULATION_MEASUREMENT_CORRELATION.md) | Comparison rules and metrics, per domain |
| [EXPERIMENT_SAFETY.md](EXPERIMENT_SAFETY.md) | Limits, output authorization, emergency stop |
| [HARDWARE_INTEGRATIONS.md](HARDWARE_INTEGRATIONS.md) | ALSA, SoapySDR, simulated devices and their options; licences |
| [EXPERIMENT_VALIDATION.md](EXPERIMENT_VALIDATION.md) | The evidence: automated tests and their tolerances |

Examples: `examples/experiments/` (acoustic, mechanical and RF experiments on the
simulated devices, and a labelled synthetic two-port Touchstone file).
