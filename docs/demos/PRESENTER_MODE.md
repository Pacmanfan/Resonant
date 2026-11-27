# Presenter mode

**Requirement:** RES-DEMO-007. **User help:** `ui.demo-studio`.

**Tools > Presenter Mode** (F11), **Run in presenter mode** in Demo Studio,
the `presenter` step, or `--presenter` on the command line.

| On | Off |
|---|---|
| the window's dock and toolbar state is saved; every dock and toolbar, the menu bar and the status bar are hidden; the presenter bar is shown at the bottom, only as tall as it needs; the viewport takes the rest | the saved state is restored; the menu and status bars return; the presenter bar hides. The project is never touched |

The presenter bar (`PresenterOverlay`, forms/PresenterOverlay.ui) shows the
demonstration's name, "Step n of m" with progress, the step's description as
the instruction (larger type), the presenter notes behind **Notes**, and
**Pause** / **Resume**, **Next** (Space, Page Down), **Restart** and **Exit
Presenter Mode** (Esc). At a narration pause **Next** is the default button.
F11 and F1 keep working without the menu bar.

Steps can still show panels in presenter mode (`workspace` with `docks`
keeps the presenter bar), so a demonstration can show, for example, the mode
table beside the viewport.
