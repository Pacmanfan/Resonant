# Current Architecture

This directory holds the current architecture documents of Resonant: one
document per subsystem, plus this index. It is the "current system
explanation" layer of the authority chain in `AGENTS.md` (below
requirements, decisions and evidence). The architecture audit
(`./tools/audit`, `docs/verification/architecture-audit.md`) checks that the
paths these documents name exist.

## Documents

* `overview.md` — layers, core modules, data flow, conventions.
* `roadmap.md` — milestones, the request that delivers each, and the
  product requirements they serve.
* `SHAPE_FUNCTION_ENERGY_AUDIT.md` — how the Shape–Function–Energy direction maps to the current subsystems, the gaps and the proposed abstractions; `COUPLED_MULTIPHYSICS_ARCHITECTURE.md` (the direction of the Coupled Energy–Matter Engine); `ENERGY_MATTER_RESEARCH_ROADMAP.md` (twelve research stages). The principle: `docs/vision/SHAPE_FUNCTION_ENERGY.md`; the physics: `docs/physics/`, `docs/materials/`, `docs/geometry/`.
* `COUPLED_ENGINE_ARCHITECTURE.md` — the Coupled Energy–Matter Engine as implemented (modules, data flow, participants, services); `COUPLED_ENGINE_AUDIT.md` (the audit before it), `PHYSICAL_SYSTEM_MODEL.md` (the system document), `COUPLED_ENGINE_ROADMAP.md` (its phases and next steps).
* `FUNCTIONAL_STRUCTURE_ARCHITECTURE.md` — functional structures: geometry as a physical design variable, analyses, studies, services, the panel; `FUNCTIONAL_STRUCTURE_AUDIT.md` (the audit before them).
* Resonance engineering (ADR-000222): `docs/physics/RESONANCE_ARCHITECTURE.md`, `docs/physics/COUPLED_RESONATORS.md`, `docs/physics/RESONANCE_SPECTROSCOPY.md`.
* Metamaterials (ADR-000223): `docs/materials/METAMATERIAL_ARCHITECTURE.md`, `docs/materials/EFFECTIVE_PROPERTIES.md`, `docs/physics/HOMOGENIZATION_AND_VALIDITY.md`.
* Unified accounting (ADR-000226): `docs/architecture/ACCOUNTING_ARCHITECTURE.md`, `docs/architecture/ACCOUNTING_AUDIT.md`, `docs/physics/FORCE_AND_MOMENTUM.md`, `docs/physics/CONSERVATION_MONITORING.md`, `docs/physics/ACCOUNTING_VALIDATION.md`.
* Dynamic material state (ADR-000225): `docs/materials/DYNAMIC_MATERIAL_STATE.md`, `docs/physics/CONSTITUTIVE_MODELS.md`, `docs/physics/STATE_EVOLUTION.md`, `docs/physics/MATERIAL_STATE_VALIDATION.md`.
* Multiscale materials (ADR-000224): `docs/materials/MULTISCALE_ARCHITECTURE.md`, `docs/materials/MATERIAL_LINEAGE.md`, `docs/physics/RVE_HOMOGENIZATION.md`, `docs/physics/ATOMISTIC_MODELS.md`.
* `AUTOMATION_ARCHITECTURE.md` — the application services, the protobuf contract, transports, threads; `AUTOMATION_DECISIONS.md` (the choices and alternatives); `MCP_INTEGRATION.md` (AI assistants).
