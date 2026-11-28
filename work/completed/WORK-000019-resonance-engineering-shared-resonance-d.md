# WORK-000019: Resonance engineering: shared resonance description, spectroscopy, lumped oscillator networks, coupled resonators, Helmholtz resonator, nonlinear models, resonant cavity in the coupled engine

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-20-resonant-structures--req-chat-20261126-018.md
**Requirements:** RES-RSN-001, RES-RSN-002, RES-RSN-003, RES-RSN-004, RES-RSN-005, RES-RSN-006, RES-RSN-007, RES-RSN-008, RES-RSN-009, RES-RSN-010, RES-RSN-011, RES-RSN-012, RES-RSN-013, RES-RSN-014
**Decision:** ADR-000222
**Evidence:** VER-000019
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261127T214044-358952

## Scope

Resonance engineering: shared resonance description, spectroscopy, lumped oscillator networks, coupled resonators, Helmholtz resonator, nonlinear models, resonant cavity in the coupled engine

## Implementation

- Resonance description: `functional::Mode` gains its nature (natural, driven-peak, identified), decay rate, bandwidth and Q by loss mechanism (`functional/Analysis.{h,cpp}`); every analysis fills them; avoided crossings in mode tracking (`functional/FunctionalStudy.cpp`).
- Spectroscopy (`dsp/Resonance`): matrix pencil with sub-window uncertainty, boxcar-decimation correction, refusals for insufficient duration or sampling; Lorentzian fits with covariance, overlap and asymmetry flags, refusal for insufficient resolution.
- Lumped oscillator networks (`oscillators/LumpedNetwork`, `Nonlinear`, layering rule in `tools/check_layering.py`): complex modes with Q by mechanism, driven response, RK4 transient with an energy ledger that keeps coupling energy its own, energy exchange, coupled-mode theory as an approximation; Duffing backbone and forced response, Mathieu Floquet tongue.
- Domains: `mechanics/LumpedResonators` (Euler-Bernoulli cantilever modes as tip-referred oscillators, coupled cantilevers); `manipulators/Helmholtz` (lumped resonator with end corrections, radiation and viscous loss, plane-wave stepped duct, coupled Helmholtz network) and the `resonator.helmholtz` geometry kind (`ManipulatorGeometry.cpp`) with its solid model (`geometry/SolidModel.cpp`); `electromagnetics/CavityResonator` keeps the Yee probe record.
- Coupling: `ResonanceAnalyses` (Helmholtz, cantilever resonator, coupled cantilevers, coupled Helmholtz), the cavity analysis's loss breakdown, extraction efficiency and Yee ring-down (`FunctionalAnalyses.cpp`), the coupled analysis for cavity systems; the coupled engine's `rectangular-cavity` kind (`multiphysics/PhysicalSystem`, `Orchestrator.cpp`), participants (`coupling/CavityParticipants`, `MultiphysicsParticipants.cpp`) and the coupling model `thermal.uniform-expansion` (`CouplingModels.cpp`).
- Services and clients: `ResonanceService` (`Services.h`, `ResonanceServices.cpp`, `ProjectServices.cpp`), the contract `proto/resonant/v1/resonance/*.proto` and `FunctionalResult.modes`, handlers (`src/automation_api/Handlers.cpp`), `docs/reference/PROTOBUF_API.md` regenerated, the MCP tool; the panel's mode table with nature and losses (`FunctionalPanel.{h,cpp,ui}`).
- Examples: `examples/resonance/a-helmholtz-resonator` ... `d-heated-cavity` and their README.

## Tests

- `tests/core/test_resonance_spectroscopy.cpp` (4), `test_lumped_network.cpp` (7), `test_nonlinear_resonance.cpp` (3), `test_helmholtz.cpp` (4), `test_cavity_heating.cpp` (3): against closed forms, second-order Mathieu boundaries, the plane-wave stepped duct, an independent scalar fixed point.
- `tests/core/test_resonance_examples.cpp` (7): every example against its reference, the cavity ring-down against wε/sigma, the avoided crossing, the Q-versus-extraction tradeoff; `test_resonance_services.cpp` (2); `python/tests/test_resonance_api.py` (3); `tests/app/test_functional_panel.cpp` extended (nature and losses); `tests/core/test_multiphysics.cpp` initializers updated for the region dimensions.

## Regression impact and documentation

- A new geometry kind (`resonator.helmholtz`, with its solid model), a new physical-system kind (layered-slab systems unchanged), new mode fields (additive in JSON and the contract); the full core (514) and app (121) suites pass.
- Documents: `docs/requirements/resonance.md`, ADR-000222, `docs/physics/RESONANCE_ARCHITECTURE.md`, `RESONANCE_SPECTROSCOPY.md`, `COUPLED_RESONATORS.md`, `RESONANCE_VALIDATION.md`, `docs/manipulators/HELMHOLTZ_RESONATORS.md`; updated `docs/physics/RESONANT_STRUCTURE_ENGINEERING.md`, `COUPLING_ARCHITECTURE.md`, `docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`, `FUNCTIONAL_STRUCTURE_ARCHITECTURE.md`, `PHYSICAL_SYSTEM_MODEL.md`, `ENERGY_MATTER_RESEARCH_ROADMAP.md`, `overview.md`, `README.md`, `roadmap.md` (M15), `docs/requirements/product.md`, `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `examples/README.md`, the help page and `docs/help/DOCUMENTATION_COVERAGE.md`.
- Scope recorded in ADR-000222 and `IMPLEMENTATION_STATUS.md`: coupled resonators are lumped networks; fluid resonances beyond lumped acoustics, Q of open radiating structures and nonlinear field solvers are planned.

## Traceability

REQ-CHAT-20261126-018 -> RES-RSN-001, RES-RSN-002, RES-RSN-003, RES-RSN-004, RES-RSN-005, RES-RSN-006, RES-RSN-007, RES-RSN-008, RES-RSN-009, RES-RSN-010, RES-RSN-011, RES-RSN-012, RES-RSN-013, RES-RSN-014 -> WORK-000019 -> evidence (pending)

## Transitions

- 2026-11-28T01:58:58.903656+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T02:46:05.449990+00:00 active -> completed by agent-dev-20261126151713-76f632
