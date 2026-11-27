# Python scripting in the application

**Requirements:** RES-PY-007..011. **Decision:** ADR-000215.
**User help:** `py.console`, `py.editor`, `py.examples`, `trouble.python`.

## Console (src/app/PythonConsole, forms/PythonConsole.ui)

A dock beside the Analysis panel. Names: `project` (a copy of the open
project), `resonant`, `app`.

| Feature | Implementation |
|---|---|
| execution | requests to the worker; the GUI never waits |
| multi-line input | one line: Python's `single` mode (values echoed; an unfinished block continues with indentation); several lines: an indented last line continues the block, otherwise the lines run in `block` mode (`code.compile_command(..., "exec")`) |
| highlighting | `PythonHighlighter` (keywords, builtins, strings incl. triple-quoted across lines, comments, numbers, decorators) |
| completion | Tab / Ctrl+Space ask the worker (`rlcompleter` over the live namespace) |
| history | Up / Down |
| errors | tracebacks in red; the console stays usable; a dead worker is reported and **Restart** starts a new one |
| inspection, help | `name?` (type, repr, docstring); `help()` |
| script files | **Run File...** |
| results | output and the `apply` summary |
| Stop | SIGINT to the worker → `KeyboardInterrupt` |

## Script editor (src/app/ScriptEditor, forms/ScriptEditor.ui)

`CodeEditor` (line numbers, current / error line, auto-indent, Tab / Shift+Tab,
completion) with Run (F5), Run Selection (F9; de-indented as a block, error
lines mapped back), Stop (Shift+F5), a Problems list that navigates to the
error line, templates (`python/templates/`), examples (`python/examples/`,
opened as unsaved copies), scripts stored in the project (`scripts` extension,
an undoable `SetProjectExtensionCommand`). User scripts are saved under the
user's data directory (`QStandardPaths::AppDataLocation/scripts`), never among
application files.

## Changing the open project

`app.apply(project, description)` sends the changed copy to the application;
`MainWindow::applyProjectCopy` turns the difference into one composite of the
ordinary commands (`projectChangeCommand`, `src/core/commands/ProjectChange`):
removals, insertions (with subtrees), moves, state changes, the material
library, project settings. Undo restores the project exactly (tested). What
there is no command for (removing a settings key, renaming the project) is
reported, not applied.

## Example scripts

Seven runnable examples (`python/examples/`), each printing real results;
`python/tests/test_examples.py` runs every one.
