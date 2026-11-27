---
id: ui.toolbars
title: Toolbars
category: User Interface Reference
order: 20
status: available
summary: The File, Geometry, View and Acoustics toolbars: what each button does, and how to show, hide and move them.
keywords: [toolbar, buttons, file toolbar, geometry toolbar, view toolbar, acoustics toolbar]
related: [ui.main-window, ui.shortcuts, ui.panels, concepts.geometry, acoustics.arrays]
---

Resonant has four toolbars. Each button is a menu command: it does exactly
what the menu entry of the same name does, has the same keyboard shortcut,
and is greyed out at the same times. Hover over a button to see its tool tip;
the status bar shows the longer description.

## The four toolbars

| Toolbar | Buttons | Same as |
|---|---|---|
| **File** | New Project, Open Project..., Save, then Undo, Redo (icons) | the File and Edit menus |
| **Geometry** | Box, Sphere, Cylinder, Cone, Plane, Disc, Ellipsoid, then New Group | the Geometry menu and **Scene > New Group** |
| **View** | Frame All, Frame Selection, Show Grid | the View menu |
| **Acoustics** | Emitter, Planar Array, Target Point, Probe, Field Plane, then Run Field Simulation | the Acoustics menu |

The File toolbar shows the platform's standard icons; the other toolbars show
the command names as text.

## What the buttons do

- **New Project**, **Open Project...**, **Save** (Ctrl+N, Ctrl+O, Ctrl+S): see [Saving and loading projects](../getting-started/saving.md). Save on a project that has never been saved asks for a file name, as **Save As...** does.
- **Undo**, **Redo** (Ctrl+Z, Ctrl+Shift+Z): the tool tip names the step, for example *Undo Move Box*. They are greyed out when there is nothing to undo or redo.
- **Box** ... **Ellipsoid**: add a primitive with its default parameters at the origin of the scene, at the top level, and select it. Its parameters are then edited in the Properties panel ([Geometry](../core-concepts/geometry.md)).
- **New Group** (Ctrl+G): add an empty group at the top level.
- **Frame All** (Home), **Frame Selection** (F): fit the whole scene, or the selected objects, in the viewport. Frame Selection is greyed out while nothing is selected.
- **Show Grid** (G): a toggle button; pressed means the ground grid is shown.
- **Emitter**, **Planar Array**, **Target Point**, **Probe**, **Field Plane**: add an acoustic object, select it and frame the scene. New emitters and arrays use the frequency set in the Acoustics panel's Medium tab; a planar array has 8 x 8 elements at half-wavelength spacing.
- **Run Field Simulation** (F5): calculate the acoustic field over the region chosen in the Acoustics panel's Simulation tab.

## Showing, hiding and moving toolbars

- **View > Panels** lists the panels first and, below a separator, the four toolbars (File, Geometry, View, Acoustics). A tick means the toolbar is shown. Right-clicking a toolbar or a panel title bar opens the same list.
- Drag a toolbar by its handle to another edge of the window, or away from the edges to make it float.
- The arrangement is remembered between sessions; **View > Reset Layout** restores the default, with all four toolbars along the top ([Preferences](preferences.md)).

## Worked example: a focused array from the toolbars

1. **New Project** (Ctrl+N).
2. **Planar Array**: an 8 x 8 array at the origin, radiating along +Z. At the default 40 kHz in air at 20 degC the wavelength is about 8.6 mm, so the element spacing is about 4.3 mm.
3. **Target Point**: a target 100 mm above the origin, on the array axis.
4. **Field Plane**: an XZ plane 200 x 200 mm with 121 x 121 samples, centred 105 mm above the origin.
5. **Run Field Simulation** (F5): the unfocused field of the array appears on the plane.
6. **Acoustics > Focus Array on Target...**, then **Calculate** and **Apply to Array** in the Focus tab, and **Run Field Simulation** again: the field now concentrates at the target ([Focusing](../acoustics/focusing.md)).

## Limitations

- There are no toolbars for wave manipulators, mechanics, electromagnetics, optimization or experiments: use their menus and panels.
- Buttons cannot be added to or removed from a toolbar, and the File toolbar cannot be switched to text.
- **Planar Array** on the toolbar always adds a rectangular planar array; the other layouts are in **Acoustics > Add Array**.

## Related topics

[The main window](main-window.md) · [Keyboard shortcuts](shortcuts.md) ·
[Panels](panels.md) · [Acoustic arrays](../acoustics/arrays.md)
