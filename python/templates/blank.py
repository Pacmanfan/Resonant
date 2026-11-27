"""A new Resonant script.

`project` is a copy of the open project (in the console and the script
editor); app.apply(project, "what changed") applies your changes to the
open project as one undoable step.
"""
import resonant
from resonant import app

print(resonant.project.summary(project))  # noqa: F821
