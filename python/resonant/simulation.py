"""Long-running simulations and background execution.

Native calls release the GIL, so a calculation can run in a Python thread
while the caller keeps working; ``Background`` wraps that with progress,
cancellation and the result or the error."""
import inspect
import threading

from ._native import native


def _id(x):
    return x.id if hasattr(x, "id") else x


def fdtd(project, region, progress=None, cancel=None) -> dict:
    """Run the full-wave FDTD solver on a region (the project's FDTD settings).
    ``progress(fraction, message)`` is called during the run; ``cancel()`` returning True stops it."""
    return native.fdtd_run(project, _id(region), progress, cancel)


class Background:
    """Runs ``function(*args, **kwargs)`` in a thread (with ``progress=`` and
    ``cancel=`` callables when the function accepts them).

    ``progress`` (0..1) and ``message`` are updated, ``cancel()`` asks the
    calculation to stop, ``wait()`` returns the result or raises its error."""

    def __init__(self, function, *args, **kwargs):
        self.progress = 0.0
        self.message = ""
        self._cancelled = False
        self._result = None
        self._error = None
        # Progress and cancellation reach functions that accept them.
        try:
            accepted = inspect.signature(function).parameters
        except (TypeError, ValueError):
            accepted = {}
        if "progress" in accepted:
            kwargs.setdefault("progress", self._on_progress)
        if "cancel" in accepted:
            kwargs.setdefault("cancel", lambda: self._cancelled)
        self._thread = threading.Thread(target=self._run, args=(function, args, kwargs), daemon=True)
        self._thread.start()

    def _on_progress(self, fraction, message=""):
        self.progress, self.message = fraction, message

    def _run(self, function, args, kwargs):
        try:
            self._result = function(*args, **kwargs)
        except BaseException as error:  # reported by wait()
            self._error = error

    def cancel(self):
        self._cancelled = True

    @property
    def done(self) -> bool:
        return not self._thread.is_alive()

    def wait(self, timeout=None):
        self._thread.join(timeout)
        if self._thread.is_alive():
            raise TimeoutError("the calculation is still running")
        if self._error is not None:
            raise self._error
        return self._result
