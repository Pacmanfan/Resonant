# Coupled Engine Roadmap

The phases of REQ-CHAT-20261126-016 and where they stand. The broader
research plan is [the energy–matter roadmap](ENERGY_MATTER_RESEARCH_ROADMAP.md).

| Phase | Content | Status |
|---|---|---|
| A — architecture and interfaces | audit, responsibilities, quantities, material state, coupling contracts, geometry state, conservation | done (`COUPLED_ENGINE_AUDIT.md`, ADR-000220, the documents) |
| B — core infrastructure | physical system, domain and coupling registries, material state, geometry versioning, orchestration, provenance | done for layered-slab systems |
| C — accounting | transfer records, ledgers, residuals, attribution, reporting through the API | done (no ledger visualization in the GUI) |
| D — first coupling | electromagnetic → thermal | done, validated |
| E — two-way coupling | temperature-dependent loss, iterative convergence | done, validated |
| F — thermomechanical | thermal expansion and stress; temperature-dependent modulus; geometry feedback | done, validated |
| G — further interactions | acoustic ↔ mechanical through the engine, Lorentz and Maxwell forces, acoustic heating | planned (models registered with equations; refused by validation) |
| H — workflow and automation | services, contract, Python, MCP | done; GUI planned |
| I — advanced coupling | nonmatching unstructured meshes, multirate time coupling, moving geometry, nonlinear coupling, more domains | planned |

## Next steps

1. **Participants for existing solvers:** the Yee solver (harmonic absorbed power by DFT of a steady-state run), 3D conduction on the Yee grid, finite-element plates and solids built from scene objects; `geometry.kind = "scene"` systems whose regions are objects.
2. **Structure ↔ sound in the engine:** wrap `StructuralRadiation` as a participant pair; then acoustic pressure on structures (two-way, added mass).
3. **Time coordination:** synchronization points, sub-cycling, temporal interpolation, checkpoints with resume.
4. **GUI:** a physical-system panel showing the coupling graph, the pass history and the ledger; multi-quantity probing in the viewport.
5. **Optimization adapter:** coupled objectives (a resonance that does not drift with temperature).
