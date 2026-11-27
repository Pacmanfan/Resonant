Resonant {VERSION} for Linux (x86_64)
==================================

Resonant is a wave field engineering studio: model, simulate, visualize and
optimize acoustic, mechanical and electromagnetic waves. This folder is the
complete application. Nothing needs to be installed and nothing is written
into this folder.

Start
-----
    ./resonant                         the application
    ./resonant examples/acoustics/<project>.resonant
    ./resonant --check-runtime         checks this system without opening a window
    ./resonant --diagnostics           a report to read or share (--report report.txt saves it)
    ./resonant --software-gl           Mesa's software OpenGL renderer (slower; for systems without a working GPU driver)

From a file manager, open this folder and run "resonant" (many file
managers offer "Run as a Program" or "Run" for it). The launcher works from
any folder, also through a symbolic link, e.g.  ln -s "$PWD/resonant" ~/.local/bin/resonant

What your system needs
----------------------
- x86_64 Linux with glibc {GLIBC} or newer (Ubuntu 24.04, Debian 13, Fedora
  40, Arch and newer releases).
- A desktop session (X11, or Wayland with XWayland) and an OpenGL {OPENGL}
  driver: Mesa 21 or newer (Intel, AMD, software), or the NVIDIA driver.
- The system libraries every desktop has: OpenGL/EGL (libglvnd), X11/xcb,
  fontconfig, FreeType, ALSA, zlib, expat and the C++ runtime. If one is
  missing, ./resonant names it and the package that provides it.

Qt 6, Python {PYTHON} with NumPy, the help and every other library are inside
this folder.

Help, examples and demonstrations
---------------------------------
- help/index.html opens the help in any browser (offline); in the
  application it is Help > Contents (F1 on any panel).
- examples/ holds example projects (File > Open), demos/ the Demo Studio
  demonstrations (Tools > Demo Studio).
- python/examples/ holds Python scripts for the console and script editor.

Your files
----------
Settings:   ~/.config/Resonant/
Data:       ~/.local/share/Resonant/ (scripts, demonstration runs, plugins)
Cache:      ~/.cache/Resonant/
Projects:   wherever you save them (Documents by default).

Update or remove
----------------
Extract a newer ZIP next to this one and start it; your settings and data
stay where they are. To remove Resonant, delete this folder (and, if you
want, the three folders above).

Check the download
------------------
    sha256sum -c RESONANT-Linux-x86_64-{VERSION}.sha256     (next to the ZIP)
    sha256sum -c diagnostics/FILES.sha256                 (inside this folder)

More: docs/release/LINUX_ZIP_DISTRIBUTION.md in the source repository, or
the help topic "Installing the Linux ZIP". License: LICENSE; third-party
components: THIRD_PARTY_NOTICES.txt and licenses/.
