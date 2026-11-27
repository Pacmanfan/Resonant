# ADR-000213: Experiment framework — one domain-independent core module, drivers in a separate library, HDF5 next to the project, explicit output authorization

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/experiments.md` RES-EXP-001..027
**Request:** REQ-CHAT-20261126-008
**Builds on:** ADR-000205 (layering), ADR-000210, ADR-000211, ADR-000212

## Context

Resonant had to gain experiments: instruments, acquisition, calibration,
measurement data, processing, spatial data, comparison with simulation,
uncertainty, execution, safety and reports, shared by the acoustic,
mechanical and electromagnetic domains, usable without any hardware and
never starting a physical output by itself. The machine offers HDF5 1.10,
ALSA 1.2.11, PortAudio 19, SoapySDR 0.8.1 (no SDR attached, its modules
broken) and libiio 0.25; there is no instrument attached.

## Decisions

1. **`src/core/experiments`, domain independent.** The framework names
   quantities and units but interprets nothing physically; it includes no
   physics domain (a layering rule). Domain adapters
   (`acoustics/AcousticExperiments`, `mechanics/MechanicalExperiments`,
   `electromagnetics/RfExperiments`) add interpretation, templates and
   domain metrics; a domain may include the framework, not the reverse.
2. **Drivers in `src/hardware` (`resonant_hardware`).** The core holds the
   abstraction and the simulated instruments; real drivers (ALSA, SoapySDR)
   are a separate static library the application and tests link and
   register. The core never includes it (a layering rule) and depends on no
   instrument API.
3. **Simulated instruments are first-class drivers** with physical models
   (free-field acoustic path, modal structure, coherent receiver, RLC DUT)
   and fault injection, so the whole chain is tested without hardware.
   Their data is marked generated.
4. **Measurement data in a store next to the project**
   (`<project>.data/measurements`), HDF5 (C API, behind one mutex because
   the serial library is not thread-safe) when built with it, a binary
   format otherwise; files created exclusively and made read-only. The
   project file holds only records (`extensions.experiments`), so projects
   without experiments are unchanged and need no schema migration.
5. **Raw data is never changed**; processing writes derived data sets with
   deterministic ids (a hash of sources and step) and the full history, so a
   chain re-run reproduces, and never duplicates, its results.
6. **Acquisition: reader and recorder threads with a bounded queue.** Gaps
   are stored as NaN and reported; the live view uses try-lock so a display
   can never delay recording.
7. **Output authorization is an in-memory token** from a process-wide
   authority, granted by a named operator with an acknowledgement within the
   experiment's limits; consumed by one run, released at its end, expired if
   unused, revoked by the latched emergency stop, never serialized. Devices
   check it for every output block. Commanded signals are recorded as their
   own data category.
8. **Real hardware: ALSA fully (tested on the "null" PCM), SoapySDR for
   discovery only.** PortAudio was not added (ALSA covers Linux audio
   interfaces directly); SoapySDR streaming, libiio, SCPI/VISA and UHD are
   not implemented because nothing could test them; the abstraction admits
   them.
9. **Calibration library append-only**; frequency-dependent responses are
   applied in the frequency domain and never extrapolated.
10. **Touchstone N-port** (1.1 and 2.0) in the electromagnetic domain; the
    one-port reader stays for compatibility.

## Consequences

- Offline work is unchanged; the Experimental Hardware dock and the
  Experiments menu are additions.
- Measured-versus-simulated comparisons exist where a domain provides the
  simulated side (free-field acoustic prediction, analytical antenna
  impedance, simulated modes and patterns); others are added per domain.
- Distributions must carry HDF5 and, optionally, ALSA and SoapySDR (all
  dynamically linked; licences in `docs/experiments/HARDWARE_INTEGRATIONS.md`).
- Level triggering, motorized positioners other than the simulated one, SDR
  streaming and laboratory instrument protocols are future work.
