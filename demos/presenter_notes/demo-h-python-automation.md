# H. Python automation: presenter notes

Type nothing during the demonstration: the script is part of the sequence. Afterwards, show Edit > Undo in the Edit menu.

## Steps

1. **Open the linear array example** (`open-project`)
2. **The Python console** (`workspace`)
3. **`project` in the console is a copy of the open project.** (`pause`)
4. **Run a script: add a target, focus, calculate, export, apply** (`python`)
5. **Recalculate in the application** (`workspace`)
6. **The application's field of the scripted design** (`run`)
7. **Frame the field** (`camera`)
8. **Edit > Undo would remove the script's change in one step.** (`pause`)

## The story

- **problem**: Repeat a study, sweep a parameter or connect Resonant to other tools without clicking.
- **configure**: Nothing beyond the open project: the script adds a target and focuses the array.
- **calculate**: In Python: the focusing phases and the field on the plane (the same C++ models as the application).
- **meaning**: The script's design change appears in the application as one undoable step; its field matches the application's.
- **inspect**: The console output (peak pressure, exported file), the new target in the scene tree, the field recalculated in the application.
- **value**: Everything the panels do is scriptable and repeatable, and scripts cannot silently change or damage the project.
- **assumptions**: Python 3 with NumPy and the resonant package; scripts are not sandboxed (they run with your permissions).
