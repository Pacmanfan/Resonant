# G. Experimental correlation: presenter notes

This demonstration shows data import and comparison. It does not start acquisitions: outputs are only ever started by an operator who authorizes them in the Experiments panel.

## Steps

1. **Open the RF experiments example (a copy in the run folder)** (`open-project`)
2. **The Experiments panel** (`workspace`)
3. **Add the antenna whose model the measurement is compared with** (`project-action`)
4. **Import the reflection data (synthetic example file)** (`experiment-data`): Say it plainly: this file is synthetic example data, labelled as such in its header.
5. **The imported S11 is stored as a measurement record of the experiment.** (`pause`)
6. **Compare it with the simulated reflection of the dipole** (`experiment-data`)
7. **The metrics show how far the model is from the data, frequency by frequency.** (`pause`)

## The story

- **problem**: Check a design against measurements: how far is the model from what the instrument recorded?
- **configure**: An RF experiment, an antenna in the scene to compare with, and a measurement file to import.
- **calculate**: The antenna's simulated input reflection over the measured frequencies and the difference to the measured S11.
- **meaning**: Where the curves agree the model explains the measurement; the error metrics say by how much they differ.
- **inspect**: The measurement in the Experiments panel and the comparison's metrics (in the run manifest and the panel's report).
- **value**: Measured and simulated data are compared in one project, with the data's origin recorded.
- **assumptions**: The imported file is SYNTHETIC example data generated from a series RLC model, not a measurement; the dipole is the analytical model.
