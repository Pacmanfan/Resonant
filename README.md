# Resonant — Wave Field Engineering Studio

*Design. Simulate. Focus. Measure.*

Resonant is a native desktop engineering application for designing,
simulating, visualizing, optimizing and experimentally investigating wave
propagation in physical environments: acoustic, mechanical and
electromagnetic, on one architecture.

What exists today is recorded, capability by capability, in
`IMPLEMENTATION_STATUS.md`. Read it before relying on anything: it separates
what is implemented and tested from what is partial, missing or deferred.

## Build and run

```sh
cmake -S . -B build/cmake-debug -G Ninja && cmake --build build/cmake-debug
./build/cmake-debug/src/app/resonant
ctest --test-dir build/cmake-debug --output-on-failure
```

qmake builds the same targets (`project.pro`). `BUILDING.md` has both build
systems, the dependencies and the tests.

## Using the application

* **Geometry** menu: add a box, sphere, cylinder, cone, plane, disc or
  ellipsoid; **Scene > New Group** makes a group.
* **Viewport:** left drag orbits, right or middle drag pans, the wheel
  zooms, a click selects (Ctrl+click adds or removes).
* **Scene** and **Properties** panels: rename, hide, transform, geometry
  parameters, material. Lengths are shown in the unit chosen under
  **Tools**; projects (`.resonant`, JSON) always store SI values.
* **Materials > Material Library:** the example values are illustrative,
  not authoritative.
* Every edit is undoable. `examples/README.md` lists example projects.

## Source layout

`src/core` (data model and physics; no Qt, no OpenGL), `src/render` (OpenGL
4.5 Core), `src/app` (the Qt application; forms in `src/app/forms`),
`src/main` (entry point), `tests/core` and `tests/app`, `third_party`
(vendored GoogleTest and nlohmann-json).

## How changes are made here

The repository carries a development-control system for coding agents:

* **Requests** — each piece of work is a record under `requests/`.
* **Claims** — an agent claims one request and registers every path first.
* **Gated completion** — a request completes only with a requirement,
  tests, evidence, documentation and a passing completion gate.
* **Workers** and **agent coordination** — unattended agent sessions, a
  roster, messages and live panels (web control panel, terminal dashboard,
  Qt control panel).

```bash
python3 scripts/devsys.py doctor      # environment and configuration
python3 scripts/devsys.py status      # claims, inbox, processing, work by state
python3 scripts/create_request.py --source chat --text "what you want"
python3 scripts/devsys.py workers start
python3 scripts/msg.py panels         # where the panels are right now
```

## Documents

* `docs/README.md` — documentation map (canonical entry point).
* `docs/requirements/product.md` — product vision and requirements.
* `docs/architecture/overview.md` — architecture; `DECISIONS.md` — decisions.
* `AGENTS.md` — the operating rules every agent follows;
  `docs/process/agent-coordination.md` — roster, messages and panels.
