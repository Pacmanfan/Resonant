"""Locates and imports the native module ``_resonant``.

Search order: inside this package (an installed build), the directory named
by ``RESONANT_PYTHON_MODULE_DIR``, then ``sys.path`` (a development build
adds ``build/<tree>/src/python``).
"""
import importlib
import os
import sys

try:
    from . import _resonant as native  # installed next to the package
except ImportError:
    _dir = os.environ.get("RESONANT_PYTHON_MODULE_DIR")
    if _dir and _dir not in sys.path:
        sys.path.insert(0, _dir)
    try:
        native = importlib.import_module("_resonant")
    except ImportError as error:
        raise ImportError(
            "the native Resonant module (_resonant) was not found: build Resonant with Python support "
            "(pybind11 and the Python headers) and set RESONANT_PYTHON_MODULE_DIR to the directory that holds it"
        ) from error
