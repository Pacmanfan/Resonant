# Plugins

Example plugins bundled with Resonant (REQ-CHAT-20261126-010, ADR-000215).
They are found automatically but load only when enabled in **Tools > Plugins**.

| Folder | Kind | Shows |
|---|---|---|
| `examples/spiral-array-generator` | python | a geometry generator (a Fermat-spiral acoustic array) |
| `examples/array-report` | python | an analysis of every array (spacing, grating lobes, directivity, sidelobes) |
| `examples/csv-field-importer` | python | a measurement importer compared with the model (bundled data is SYNTHETIC) |
| `examples/workflow-pack` | workflows | two workflow templates |

Plugins are programs that run with your permissions; they are not sandboxed.
Deliberately broken plugins for the tests are in `tests/data/plugins/`.

Documentation: the in-app help (topics `py.plugins`, `ref.plugin-api`),
`docs/plugins/`.
