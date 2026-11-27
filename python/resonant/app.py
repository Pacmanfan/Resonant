"""The bridge between a script in the Resonant console and the application.

In the console, ``project`` is a copy of the open project (taken when the
console started or when ``app.pull()`` asked for it). Changing it changes the
copy only; ``app.apply(project, "what changed")`` hands it back, and the
application applies it as ONE undoable step. Nothing else in the application
is reachable from a script: no device, no output, no window.
"""
import json
import sys

_channel = None  # set by the worker: a function sending one message to the application


def _send(message: dict) -> None:
    if _channel is None:
        raise RuntimeError("not running inside the Resonant console")
    _channel(message)


def apply(project, description: str = "Script changes") -> None:
    """Hand the (changed) project back to the application as one undoable change."""
    _send({"type": "apply", "description": description, "project": json.loads(project.to_json())})


def pull() -> None:
    """Ask the application for a fresh copy of the open project (arrives as `project`)."""
    _send({"type": "pull"})


def message(text: str) -> None:
    """Show a line in the application's status bar."""
    _send({"type": "message", "text": str(text)})


def running_in_console() -> bool:
    return _channel is not None


def stdout_is_console() -> bool:
    return hasattr(sys.stdout, "resonant_console")
