---
id: ui.preferences
title: Preferences
category: User Interface Reference
order: 100
status: available
summary: What Resonant remembers between sessions (window, panel layout, grid and axes, display unit, last folder), where it is stored and how to reset it.
keywords: [preferences, settings, layout, display unit, reset, workspace]
related: [ui.panels, gs.units, gs.saving, ui.main-window, py.installation]
---

Resonant has no Preferences dialog. The few settings that belong to you
rather than to a project are changed where you use them (a menu or a panel)
and remembered automatically for the next session. Everything that describes
the engineering problem is stored in the project file instead.

## What is remembered

| Setting | Where you change it | Default | Saved |
|---|---|---|---|
| Window size and position | by resizing and moving the window | 1440 x 900 pixels | when Resonant closes |
| Panel and toolbar arrangement | by dragging, stacking, floating and closing panels and toolbars | see [Panels](panels.md) | when Resonant closes |
| Ground grid shown | **View > Show Grid** (G) | on | when Resonant closes |
| Coordinate axes shown | **View > Show Axes** | on | when Resonant closes |
| Display length unit | **Tools > Display Length Unit** | metres | when Resonant closes |
| Folder for Open and Save dialogs | by opening or saving a project | your documents folder | at each successful open or save |

The Open and Save As dialogs start in the folder of the current project if
it has been saved, otherwise in the last folder used.

## The display length unit

**Tools > Display Length Unit** offers metres (m), centimetres (cm),
millimetres (mm) and inches (in). It changes how lengths are shown and typed
in the Properties panel, the editors and the analysis tables; it never changes
a stored value, which is always in SI units, and a project saved in one unit
opens identically in another. Angles are always shown in degrees. See
[Units and coordinate systems](../getting-started/units.md).

## Not preferences: settings stored in the project

These travel with the project file and are restored when it is opened, so
they are the same on every computer:

- the acoustic medium, frequency and field model (Acoustics panel, Medium tab);
- the full-wave solver settings, mechanical analysis settings and electromagnetic settings of the examples and of your projects;
- materials, experiments and optimization problems.

Session-only state is not stored anywhere: the camera position (the view is
framed when a project opens), the sampled acoustic fields (run the
simulation again after opening), and the Console's level filter, which starts at *Information and
above*. See [Saving and loading projects](../getting-started/saving.md).

## Where the settings are kept

They are kept per user, in the platform's standard settings store, under the
organization and application name *Resonant*. On Linux this is the text file
`~/.config/Resonant/Resonant.conf` (under `$XDG_CONFIG_HOME` when that is
set), with the keys `workspace/geometry`,
`workspace/state`, `view/grid`, `view/axes`, `units/length` and
`files/lastDirectory`.

## Resetting

- **View > Reset Layout** restores the default arrangement of panels and toolbars at once. It keeps the window size, the grid and axes switches and the display unit.
- To return everything to the defaults, close Resonant and delete the settings file; it is recreated on the next close.

## Common mistakes

- *A change was not remembered*: the layout, grid, axes and unit are written only when Resonant closes normally. If it was ended by the system or crashed, or if you chose **Cancel** at the unsaved-changes question, the previous settings stay.
- *A colleague's project looks different*: the panel layout and display unit are yours, not the project's. The values in the project are the same.

## Other configuration

The Python interpreter, the Python package and plugin locations are not set
in the application's menus; see [Python installation and configuration](../python/installation.md)
and [Plugins](../python/plugins.md). Keyboard shortcuts cannot be changed
([Keyboard shortcuts](shortcuts.md)).

## Related topics

[Panels](panels.md) · [Units and coordinate systems](../getting-started/units.md) ·
[Saving and loading projects](../getting-started/saving.md) · [The main window](main-window.md)
