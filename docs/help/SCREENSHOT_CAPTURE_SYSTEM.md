# Screenshot capture system

**Requirement:** RES-HELP-009. **Decision:** ADR-000215.

Every screenshot in the help is captured from the running application by
its own commands. None is drawn, generated or edited by hand.

## The manifest: help/screenshots/manifest.json

| Field | Meaning |
|---|---|
| `outputDirectory` | where captures go (`help/src/images/screenshots`, committed so the help works offline) |
| `environment` | variables set before the window exists (paths relative to the source tree): the test plugins on the plugin path, a screenshot plugin configuration |
| `screenshots[]` | `name` (stable file name, e.g. `acoustic-focal-field.png`), `title`, `page` (the help topic it belongs to), `description`, `caption`, `steps`, `capture` |

`steps` are Demo Studio steps (`docs/demos/DEMO_SEQUENCE_FORMAT.md`): open a
designated example project, choose the workspace (docks, tab, window size,
dock sizes), set the camera, select an object, set the visualization, run a
simulation and wait for it, show a result. `capture` names the `target`
(`window`, `viewport`, a dock, `help`, `script-editor`, `plugins`,
`workflows`, `materials`), an optional `size`, `open` (a script for the editor)
and `annotations`.

## Running it

```bash
xvfb-run -a -s "-screen 0 1920x1200x24" build/debug/src/main/resonant \
    --capture-screenshots help/screenshots/manifest.json [--only name1.png,name2.png]
```

Each entry runs as its own sequence from the default layout at 1600 x 1000;
the run prints one line per screenshot and exits with status 0 only when all
were captured. An automated run never saves the window layout.

## Capture

`DemoController::doCapture` lays out and draws everything pending, grabs the
target with `QWidget::grab` (the OpenGL viewport renders into the image), and
writes a PNG. The status bar is cleared first. With `annotations` (numbered
callouts at fractions of the image, with labels) it also writes
`<name>-annotated.png`; the original stays unannotated.

## Privacy

Captures show example projects only. Paths under the home directory are shown
as `~` in the Plugins and Workflows dialogs; plugin messages name the plugin
folder, not its path; status messages (which can contain paths) are cleared;
the log panel is not captured. Reviewing a new screenshot for private data is
part of adding it.

## Validation

The help build checks that every screenshot a page shows is in the manifest,
that every manifest entry names an existing page, that the image exists
(strict build; `--draft` lets a not-yet-captured screenshot pass with a
warning), and that no page marked planned or not implemented shows one.
