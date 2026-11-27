# Resonant changelog

## 0.1.0 (first Linux ZIP)

The first packaged release of Resonant, the wave field engineering studio:
everything below is in the ZIP and runs from wherever it is extracted.

- **Application foundation:** the main window, a 3D OpenGL viewport, the
  scene graph, primitives, materials, project files, undo and redo,
  background jobs and logging.
- **Acoustics:** emitters and phased arrays, analytical wave-field
  simulation, focusing and multi-target inverse design, field regions,
  probes and field visualization.
- **Passive wave manipulation:** reflectors, lenses, waveguides and
  cavities, a Kirchhoff reflector field model, ray paths and hybrid focusing.
- **Full-wave acoustics:** a staggered-grid FDTD solver with boundaries,
  material interfaces, transient sources and frequency analysis.
- **Mechanics:** structural finite elements with modal, harmonic, transient
  and static analyses.
- **Electromagnetics:** antennas, arrays and array factors, RF analysis and a
  Yee full-wave solver.
- **Compute:** deterministic multi-threading, an optional CUDA backend and
  VOLK vector kernels.
- **Experiments:** a hardware abstraction, simulated instruments,
  acquisition, calibration and correlation with simulation (physical outputs
  only ever start with an explicit authorization).
- **Optimization:** inverse design, geometry and topology optimization across
  the domains, in the Optimization Studio.
- **Python, help and demonstrations:** a Python API with a console and a
  script editor, workflows, plugins, the offline HTML help and Demo Studio.
- **Automation API:** application services behind a protobuf contract, a
  local endpoint and a gRPC bridge, events, jobs, transactions, a Python
  client and an MCP adapter.
- **Distribution:** this self-contained Linux ZIP with its own Qt, Python and
  libraries, a launcher with system checks, `--check-runtime` and
  `--diagnostics`, the help as `help/index.html` for any browser, and the
  third-party notices.
