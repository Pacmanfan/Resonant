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
* `AUTOMATION_ARCHITECTURE.md` — the application services, the protobuf contract, transports, threads; `AUTOMATION_DECISIONS.md` (the choices and alternatives); `MCP_INTEGRATION.md` (AI assistants).
