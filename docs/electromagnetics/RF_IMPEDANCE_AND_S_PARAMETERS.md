# RF Impedance and S-Parameters

## Feed impedance (RES-EM-006)

Dipole: the induced-EMF formulas (Balanis 8-60a/b) for a sinusoidal current
give the impedance referred to the current maximum; it is referred to the
feed by dividing by sin^2(k l / 2). A half-wave dipole gives
Z = (eta0 / 4 pi)(gamma + ln 2 pi - Ci 2 pi) + j (eta0 / 4 pi) Si 2 pi =
73.08 + j 42.51 ohm, exactly the textbook value. Near anti-resonance
(k l / 2 near a multiple of pi) the model's feed current vanishes and the
impedance is flagged **not valid**. A monopole over an infinite ground
plane has half the impedance of the dipole of twice its height. Small
loop: radiation resistance plus the inductive reactance of the loop.

Sine and cosine integrals are computed together (power series for small
arguments, a continued fraction for the exponential integral otherwise);
checked against mpmath values to 1e-12 in the tests.

## Matching quantities

Against the antenna's reference impedance Z0:

* reflection coefficient Gamma = (Z - Z0) / (Z + Z0) (this is S11)
* return loss -20 log10 |Gamma| (positive)
* VSWR (1 + |Gamma|) / (1 - |Gamma|), infinite for |Gamma| = 1
* mismatch efficiency 1 - |Gamma|^2

## Sweeps

The Impedance tab sweeps from / to / points (stored in the project) and
plots R and X, return loss or VSWR.

## Touchstone files (RES-EM-019)

* **Import** (Import Measured .s1p): one-port Touchstone 1.x with any of
  Hz / kHz / MHz / GHz and RI / MA / DB, reference impedance from the
  option line, comments kept. Multi-port data, Y/Z/H/G parameters,
  decreasing frequencies and empty files are refused with the line number.
  The data is stored in the project (undoable) and plotted beside the model
  as **Measured: <file>** — never merged with it.
* **Export** (Export .s1p): the calculated sweep as S11 in GHz / RI with
  comments naming the model and stating that it is not a measurement.

No instrument is controlled: nothing is transmitted (RES-EM-019).
