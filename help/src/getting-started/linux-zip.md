---
id: gs.linux-zip
title: Installing the Linux ZIP
category: Getting Started
order: 15
status: available
summary: Download the self-contained Linux ZIP, check it, extract it anywhere and start Resonant with ./resonant; what your system needs, where your files go, diagnostics, updating and removing.
keywords: [zip, download, linux, extract, launcher, diagnostics, check-runtime, portable, relocatable, glibc, software-gl, uninstall, update]
related: [gs.installation, trouble.installation, trouble.dependencies, gs.introduction]
---

The Linux ZIP is the complete application in one folder: the program, Qt 6,
Python 3.12 with NumPy, this help, the example projects, the demonstrations
and every library they need. Nothing is installed; you extract the folder
where you like and start it.

## What your system needs

| Requirement | |
|---|---|
| Processor and system | x86_64 Linux with glibc 2.38 or newer (Ubuntu 24.04, Fedora 40, Debian 13 and newer releases) |
| Display | a desktop session: X11, or Wayland with XWayland |
| Graphics | an OpenGL 4.5 driver: Mesa 21 or newer (Intel, AMD, software), or the NVIDIA driver |
| Libraries | those every desktop has: OpenGL/EGL, X11, fontconfig, FreeType, ALSA, zlib, expat, the C++ runtime |

If something is missing, the launcher tells you what and which package
provides it, before anything else happens.

## Download, check and extract

Keep the ZIP and its `.sha256` file together and check the download:

```bash
sha256sum -c RESONANT-Linux-x86_64-0.1.0.sha256
unzip RESONANT-Linux-x86_64-0.1.0.zip
```

Any folder works, also on a second drive or a USB stick, and paths with
spaces. Use `unzip` or your desktop's archive manager, which keep the
programs executable.

## Start

```bash
cd RESONANT-Linux-x86_64-0.1.0
./resonant
./resonant examples/acoustics/c-focused-planar-array.resonant
```

In a file manager, run `resonant` from the folder. To start Resonant from
any terminal, link the launcher into a folder on your `PATH`, for example
`ln -s "$PWD/resonant" ~/.local/bin/resonant`. Every option of the
application is passed through, and Resonant's exit status is returned.

## Check the system

| Command | What it does |
|---|---|
| `./resonant --check-runtime` | checks the package's files, the C library, Qt, OpenGL 4.5, Python, the help, the examples, plugins and compute backends without opening the main window; `PASS`, `WARN` or `FAIL` for each |
| `./resonant --diagnostics --report report.txt` | the same plus every loaded library, saved to a file you can share |
| `./resonant --software-gl` | uses Mesa's software OpenGL renderer: slower, for systems whose GPU driver does not work |

A report shows your home folder as `~` and environment variables by name
only, with no project content or setting. Resonant never sends anything.

## Help outside the application

Open `help/index.html` in any browser: the same pages as Help > Contents,
with the contents and a search, offline.

## Where your files go

Settings are kept in `~/.config/Resonant/`, scripts, demonstration runs and
plugins in `~/.local/share/Resonant/`, caches in `~/.cache/Resonant/`, and
projects wherever you save them. Resonant never writes into its own folder,
so the folder can be read-only.

## Update and remove

To update, extract the newer ZIP next to the old one and start it: your
settings stay. To remove Resonant, delete its folder (and the three folders
above if you want your settings and data gone too).

## Common mistakes

- Extracting with a tool that drops permissions: the launcher reports that `bin/resonant` is not executable; extract again with `unzip`.
- Running on an older distribution: the launcher names your glibc version and the one needed.
- Starting it over a remote connection without OpenGL 4.5: `./resonant --check-runtime` reports the OpenGL version obtained.

## Limitations

- Only x86_64 with glibc 2.38 or newer; Ubuntu 22.04, Debian 12 and RHEL 9 are not supported by this package.
- Native Wayland has not been tested: on Wayland sessions Resonant uses XWayland.
- No desktop-menu entry is created.

See [Installation problems](../troubleshooting/installation.md) when the launcher refuses to start.
