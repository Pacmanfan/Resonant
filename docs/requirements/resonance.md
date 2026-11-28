# Resonant Structures, Energy Storage and Coupled Oscillatory Systems

**Owner of:** `RES-RSN-001..014`
**Source:** REQ-CHAT-20261126-018 (Prompt 20, "Resonant Structures, Energy Storage and Coupled Oscillatory Systems")
**Refines:** `functional_structures.md` RES-FUNC-005 (analyses), RES-FUNC-006..010 (reference models), RES-FUNC-011 (mode tracking); `shape_function_energy.md` RES-SFE-004 (resonance as a shared vocabulary); `coupled_engine.md` RES-CEME-006 (coupled schemes)
**Decisions:** ADR-000222 (builds on ADR-000221, ADR-000220)
**Documents:** `docs/physics/RESONANCE_ARCHITECTURE.md`, `docs/physics/RESONANCE_SPECTROSCOPY.md`, `docs/physics/COUPLED_RESONATORS.md`, `docs/physics/RESONANCE_VALIDATION.md`, `docs/manipulators/HELMHOLTZ_RESONATORS.md`, `docs/physics/RESONANT_STRUCTURE_ENGINEERING.md` (updated)
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-RSN-001 — One description of resonance, domain physics underneath

Every analysis reports its resonances in one form: label (when the model can
label them), nature (`natural` eigenmode, `driven-peak` of a response,
`identified` from a signal), frequency, decay rate, Q and its breakdown by
loss mechanism, bandwidth, and the energies the model computes. The
equations stay in the domains; no resonance is assigned to a structure
without a model computing it. Natural frequencies and driven-response peaks
are reported as different things.
*Verification:* `ResonanceExamples.HelmholtzResonatorMatchesItsClosedFormAndTheDuctModel`, `LumpedNetwork.DrivenPeaksDifferFromNaturalFrequenciesAndPowerBalances`, `ResonanceServices.ResonanceAnalysesRunAsJobsAndReportTheirModes`.

### RES-RSN-002 — Loss mechanisms stay identifiable

Each model reports the Q (or power) of every mechanism it includes — wall
conduction, dielectric and conducting fills, a port, acoustic radiation,
viscous boundary layers, structural damping, the dampers of a lumped network
— and combines them as 1/Q = Σ 1/Q_i; no single arbitrary damping parameter.
*Verification:* `CavityResonator.*`, `HelmholtzResonator.*`, `LumpedNetwork.*`.

### RES-RSN-003 — Resonance spectroscopy

Resonances are identified from responses and signals: Lorentzian fits of
response peaks with fit quality and uncertainty, flagging overlapping modes
and asymmetric line shapes; complex poles of time signals by the matrix
pencil method (frequency, decay, Q, amplitude, phase, uncertainty) for several
overlapping modes; ring-down. Records too short or too coarse for the
requested resolution are refused with the reason; a spectral peak is not
reported as a natural mode unless the method can establish it.
*Verification:* `ResonanceSpectroscopy.*`.

### RES-RSN-004 — Lumped oscillator networks

Networks M ẍ + C ẋ + K x = f of resonators and couplings, assembled by the
domains from their physics (beam modal masses and stiffnesses, acoustic
inertances and compliances): complex natural modes with damping, mode shapes
and participation; the driven response with supplied, stored and dissipated
power; time integration with an energy ledger in which coupling energy is
the coupling element's and every term is counted once (supplied = Δstored +
dissipated, by mechanism, to integration accuracy).
*Verification:* `LumpedNetwork.*`.

### RES-RSN-005 — Coupled resonators

Two or more coupled resonators: individual frequencies, coupled
eigenfrequencies, splitting, symmetric and antisymmetric (hybridized) modes,
participation and localization with detuning, avoided crossings, energy
exchange in time (transfer efficiency, transfer time, remaining energy),
damping, and the coupled-mode approximation reported as an approximation
next to the full network solution. Splitting is computed, never assumed.
*Verification:* `LumpedNetwork.*`, `ResonanceExamples.Coupled*`.

### RES-RSN-006 — Helmholtz resonator

A Helmholtz resonator (cavity, neck, end corrections, medium): the lumped
resonance with inner and outer end corrections, radiation and viscous loss
as separate mechanisms, impedance, cavity pressure gain, stored energy and
bandwidth; an exact stepped-duct (plane-wave transfer-matrix) model with the
baffled-piston load against which the lumped model and the outer end
correction are verified; the validity range (ka, kL, the cavity size against
the wavelength) reported.
*Verification:* `HelmholtzResonator.*`, `ResonanceExamples.Helmholtz*`.

### RES-RSN-007 — Microwave cavity ring-down

The cavity's full-wave model identifies its resonances from the Yee ring-down
by complex poles: frequencies and the fill's Q, compared with the closed form.
*Verification:* `ResonanceExamples.CavityRingDown*`.

### RES-RSN-008 — Mechanical cantilever as a resonator

The cantilever's modal mass and stiffness, kinetic and strain energy at an
amplitude, and its frequency response and ring-down as a single-mode
resonator with its loss factor; analytical beam theory as the reference.
*Verification:* `ResonanceExamples.Cantilever*`.

### RES-RSN-009 — Transient buildup and decay

Buildup under a harmonic drive and free decay, with the energy envelope
against e^{−ωt/Q} and the buildup time 2Q/ω, for the lumped networks;
ring-down identification of full-wave and network signals.
*Verification:* `LumpedNetwork.BuildupAndDecay*`.

### RES-RSN-010 — Nonlinear and parametric resonance interfaces

Duffing (cubic stiffness) oscillators: the backbone (amplitude-dependent
frequency) by harmonic balance and its check by time integration; parametric
excitation (Mathieu) with the first instability tongue by Floquet analysis.
These are lumped models with stated assumptions; no nonlinear behavior is
inferred from a linear sweep.
*Verification:* `NonlinearResonance.*`.

### RES-RSN-011 — Resonance through the Coupled Energy–Matter Engine

A rectangular cavity driven near resonance as a coupled system: the
electromagnetic cavity participant (resonance, port coupling, wall and fill
loss), a lumped thermal participant, a fill permittivity that depends on
temperature and walls that expand; iterated to a fixed point with the energy
ledger; the resonance shift reported; one-way and feedback distinguished.
*Verification:* `CavityHeating.*`, `ResonanceExamples.TheHeatedCavity*`.

### RES-RSN-012 — Tuning, objectives and tradeoffs

Resonance tuning by geometry (functional-structure parameters), material and
environment (temperature), with sensitivities; objectives over resonance
metrics (frequency, Q, bandwidth, stored energy, external coupling, mode
separation, transfer efficiency) through the existing design and
optimization architecture; the tradeoff between unloaded Q and extraction
efficiency is exposed, not resolved.
*Verification:* `ResonanceExamples.HelmholtzResonatorMatchesItsClosedFormAndTheDuctModel` (sensitivity), `ResonanceExamples.CoupledCantilevers*` (detuning), `ResonanceExamples.TheHeatedCavity*` (geometry), `ResonanceExamples.QAgainstExtractionIsATradeoffNotAnOptimum`.

### RES-RSN-013 — Services, contract and clients

Resonance identification of signals and spectra as a service operation
(`ResonanceService.IdentifyResonances`); the new analyses through
`FunctionalStructureService`; results' modes in the contract; Python and MCP.
*Verification:* `ResonanceServices.*`, `python/tests/test_resonance_api.py`.

### RES-RSN-014 — Reference examples and validation

Examples for a Helmholtz resonator, coupled cantilevers, coupled Helmholtz
resonators and a heated cavity run in tests against analytical references;
the panel shows the resonances' nature and losses; documentation states what
is and is not modeled.
*Verification:* `ResonanceExamples.*`, `FunctionalPanel.*`.
