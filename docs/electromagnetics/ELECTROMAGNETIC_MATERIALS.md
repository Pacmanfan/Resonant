# Electromagnetic Materials

## Properties (RES-EM-003)

A material record carries, in its electromagnetic domain: relative
permittivity er, relative permeability mur, conductivity sigma (S/m) and an
optional loss tangent tan d. Values may be tabulated over frequency (the
value at the frequency of use is interpolated) and may state a validity
range; a frequency outside it is flagged in the result. The source of each
value is carried as provenance.

Derived at a frequency f (omega = 2 pi f):

* complex permittivity `eps_c = eps0 er (1 - j tan d) - j sigma / omega`
* loss ratio `sigma / (omega eps0 er) + tan d`
* classification: **good conductor** (loss ratio > 100), **lossy
  dielectric**, **dielectric** (< 0.01)
* wavenumber `k = omega sqrt(mu eps_c)` with Im k <= 0, impedance
  `eta = sqrt(mu / eps_c)`, skin depth `1 / |Im k|` (exact for any loss),
  surface resistance `1 / (sigma delta)` for good conductors
* normal-incidence reflection `(eta2 - eta1) / (eta2 + eta1)` and transmission

## The example library

The built-in library's electromagnetic values (copper 5.8e7 S/m, aluminium
3.5e7 S/m, brass, steels, titanium, glass er 6, acrylic er 3, polycarbonate
er 2.9, alumina er 9, water er 80, ...) are **illustrative**, labelled so
in each record with a note on what they depend on (frequency, purity,
temperature, cold work). For a real design use measured data for the
actual material at the frequency of interest. A material without
permittivity and permeability is reported as having no electromagnetic
data; nothing is invented for it.

## Where materials are used

* **Conductor loss** of wire antennas: the assigned material's surface
  resistance gives the loss resistance and the radiation efficiency.
* **Patch substrates:** the permittivity enters the resonant-length estimate.
* **Guide filling:** a non-conducting material's er and mur shift the cut-offs.
* **Yee solver:** good conductors become perfect conductors (the plan names
  them and their skin depth); other materials keep er, mur and an
  equivalent conductivity sigma + omega eps0 er tan d at the run frequency.
  Objects without a material, or with one lacking electromagnetic data, are
  treated as perfect conductors with a warning.
