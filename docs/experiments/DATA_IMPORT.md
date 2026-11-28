# Importing Measurement Files

**Status:** current (REQ-CHAT-20261126-023, RES-LAB-003, -004), `laboratory/LabImport`.

## Formats

| Format | Layout |
|---|---|
| CSV, TSV | `# key: value` metadata lines, a header `name [unit]`, rows of numbers |
| JSON | `{"format": "resonant.dataset", "metadata": {...}, "quantity", "unit", "axis": {"name", "unit", "values"}, "values" or "real" and "imag"}`, or `{"scalars": [{"name", "quantity", "unit", "value", "uncertainty"}]}` |
| WAV | PCM 8/16/24/32-bit or 32-bit float. Each channel becomes a time series in digital full scale (uncalibrated) |
| Touchstone | through the RF importer of the Experiments workspace |

NetCDF, TDMS, external HDF5 files and images are not imported.

The first column decides the kind of dataset:

- **`time [s]`:** a time series. Each other column is a channel. A column
  `<name> uncertainty [unit]` holds that channel's standard uncertainties.
- **`frequency [Hz]`:** a frequency response. The columns are `real` and
  `imag`, or `magnitude` and `phase` (deg or rad), with optional `coherence` and
  `uncertainty` columns. Without a phase column, the phase is marked not
  measured, and it is then never compared.
- **`x, y, z [m]`:** spatial points, followed by a value and an optional phase.
  The frame is taken from the `frame` metadata line.

Metadata keys: `quantity`, `provenance` (measured or synthetic), `instrument`,
`sensor`, `domain`, `frame`, `uncertainty` (a percentage or an absolute value),
`sample rate`.

## Units and conversions

Units are parsed by `foundation/Dimensions`, and the quantity follows from the
dimension:

- a quantity stated in the metadata or named by the column wins;
- an assumed quantity is reported;
- ratios such as m/N become a quantity per a quantity (Displacement per Force).

Values are stored in SI. A conversion (for example kHz to Hz, mm/N to m/N, or deg to
rad) is recorded in the record's `acquisition.importSteps`, because a raw record has
no processing history. degC is converted to K by adding 273.15, and that is
recorded too.

## The import report

Every issue has a severity, a code, a message, and its row and column:

| Code | |
|---|---|
| missing-unit, unknown-unit | a column without a unit, or one that is not a unit of a measured quantity (error) |
| malformed-row | the wrong number of fields, or a field that is not a number (error) |
| missing-value, not-finite | an empty field, NaN or infinity: kept as read (warning) |
| non-monotonic-time, duplicate-time | the axis does not increase (error) |
| inconsistent-sampling, sampling-mismatch | the time step varies, or differs from the stated rate (warning) |
| missing-column | an expected column is absent (error) |
| missing-metadata, missing-frame | no provenance, instrument or frame: no claim of traceability (warning) |
| empty, unreadable | nothing to read, or not the format (error) |

An import with errors creates no dataset. With `acceptPartial`, the rows that
could be read are kept, the rows left out are recorded on the records, and the
report is kept either way. Importing the same bytes again is recognized by their
SHA-256 and refused. The original file is kept read-only next to the project.
