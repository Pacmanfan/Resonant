---
id: ui.shortcuts
title: Keyboard shortcuts
category: User Interface Reference
order: 90
status: available
summary: Every keyboard shortcut and mouse control of the main window, the viewport and the scene tree.
keywords: [shortcuts, keyboard, keys, hotkeys, mouse, accelerators]
related: [ui.main-window, ui.viewport, ui.scene-hierarchy, gs.navigation, ui.help-window]
---

These are the shortcuts of the main window's menus and of the viewport. Each
key does exactly what its menu entry does, and does nothing while that entry
is greyed out (for example **Delete** with nothing selected).

## Files and editing

| Key | Command |
|---|---|
| Ctrl+N | **File > New Project** |
| Ctrl+O | **File > Open Project...** |
| Ctrl+S | **File > Save** |
| Ctrl+Shift+S | **File > Save As...** |
| Ctrl+Q | **File > Exit** |
| Ctrl+Z | **Edit > Undo** |
| Ctrl+Shift+Z | **Edit > Redo** |
| Ctrl+D | **Edit > Duplicate** |
| Del | **Edit > Delete** |
| F2 | **Edit > Rename** (exactly one object selected) |
| Ctrl+A | **Edit > Select All** |
| Esc | **Edit > Deselect** |

## Scene, view and materials

| Key | Command |
|---|---|
| Ctrl+G | **Scene > New Group** |
| H | **Scene > Hide / Show Selected** |
| G | **View > Show Grid** (on and off) |
| Home | **View > Frame All** |
| F | **View > Frame Selection** |
| Ctrl+1 | **View > Top (looking down Z)** |
| Ctrl+2 | **View > Front (looking along +Y)** |
| Ctrl+3 | **View > Right (looking along -X)** |
| Ctrl+M | **Materials > Material Library...** |

## Simulation and safety

| Key | Command |
|---|---|
| F5 | **Acoustics > Run Field Simulation** |
| F6 | **Simulation > Run Full-Wave Simulation** |
| Ctrl+Shift+Space | **Experiments > EMERGENCY STOP**: stops every physical output at once; the stop stays latched until it is reset in the Experimental Hardware panel ([Safety](../experiments/safety.md)) |

## Help

| Key | Command |
|---|---|
| F1 | **Help > Context Help**: the help page for the panel, tab, editor or dialog with the keyboard focus, else the one under the mouse pointer ([The help window](help-window.md)) |
| Ctrl+F1 | **Help > Help Contents** |
| Shift+F1 | **Help > Search Help...** |

## Python, automation and presenting

| Key | Command |
|---|---|
| Ctrl+Shift+P | **Tools > Python Console** |
| F11 | **Tools > Presenter Mode** (on and off) |
| Space, Page Down | **Next** in presenter mode |
| Esc | leave presenter mode |
| F5, F9, Shift+F5 | in the script editor: Run, Run Selection, Stop ([The script editor](../python/editor.md)) |

## Viewport mouse controls

| Mouse | Action |
|---|---|
| left drag | orbit around the centre point |
| right or middle drag | pan |
| wheel | zoom towards the centre point (12 % per notch) |
| left click | select the object under the pointer; on empty space, clear the selection |
| Ctrl + left click | add the object to the selection, or remove it |

See [Viewport controls](viewport.md).

## Scene tree

| Key or mouse | Action |
|---|---|
| click, Ctrl+click, Shift+click | select one, add or remove one, select a range |
| arrow keys | move through the tree |
| F2 | rename the selected object; Enter confirms, Esc cancels |
| right click | the context menu (add, rename, duplicate, delete, hide, group, material, frame) |
| click the check box | hide or show the object |

See [The scene hierarchy](scene-hierarchy.md).

## Menu access keys

Every menu and entry has an underlined letter: press **Alt** with the letter
of a menu (Alt+F for File, Alt+V for View, Alt+X for Experiments, ...),
then the underlined letter of the entry. Where two menus share a letter (E
for Edit and Electromagnetics, H for Mechanics and Help), press it again to
move to the next one. Number fields accept the arrow keys and Page
Up / Page Down to step their value.

## When a key does not work

- *A letter key types into a field*: single-key shortcuts (G, H, F) and keys such as Home and Del go to the text or number field that has the keyboard focus. Click in the viewport or the scene tree first.
- *Redo is not Ctrl+Y*: Resonant uses Ctrl+Shift+Z.
- *Nothing happens*: the command is greyed out in its menu, usually because nothing (or more than one object, for F2) is selected.

Shortcuts cannot be changed. The Python console and the script editor have
keys of their own ([The Python console](../python/console.md),
[The script editor](../python/editor.md)).

## Related topics

[The main window](main-window.md) · [Viewport controls](viewport.md) ·
[The scene hierarchy](scene-hierarchy.md) · [Navigation and selection](../getting-started/navigation.md)
