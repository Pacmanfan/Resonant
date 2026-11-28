# Resonant for Linux: the ZIP

How the ZIP is built, verified and published is `DEPLOYMENT.md` (ADR-000230).

Download one ZIP, extract it anywhere, run `./resonant`. Nothing is
installed, and nothing needs to be (RES-DIST-001..015). This page is for
users; how the ZIP is made is in [the packaging decisions](PACKAGING_DECISIONS.md).

## What the ZIP contains

`RESONANT-Linux-x86_64-<version>.zip` holds one folder,
`RESONANT-Linux-x86_64-<version>/`, with the complete application: the
program and the headless automation host, Qt 6 and every library they need,
Python 3.12 with NumPy (for the console, scripts, plugins and the gRPC
bridge), the offline help, 43 example projects, the Demo Studio
demonstrations, example plugins and Python scripts, and the license notices.
Only libraries that every Linux desktop has (the C library, OpenGL, X11,
fonts, ALSA) come from your system.

## Requirements

- x86_64 Linux with **glibc 2.38 or newer**: Ubuntu 24.04 or newer, Fedora
  40 or newer, Debian 13, Arch, openSUSE Tumbleweed (tested: Ubuntu 24.04,
  Fedora 41, Fedora 42).
- A desktop session (X11, or Wayland with XWayland) and an **OpenGL 4.5**
  driver (Mesa 21+ or NVIDIA).
- About 300 MB of disk.

Details and the full list of system libraries: [Linux compatibility](LINUX_COMPATIBILITY.md).

## Download and verify

Download the ZIP and its `.sha256` file into the same folder, then:

```bash
sha256sum -c RESONANT-Linux-x86_64-<version>.sha256
```

It prints `OK` when the download is intact.

## Extract

```bash
unzip RESONANT-Linux-x86_64-<version>.zip
```

Any folder works: your home folder, a second drive, a USB stick, a path
with spaces. Use a tool that keeps file permissions (`unzip`, GNOME's
Archive Manager, KDE's Ark); if `bin/resonant` lost its execute permission,
the launcher says so.

## Start

```bash
cd RESONANT-Linux-x86_64-<version>
./resonant                                      # the application
./resonant examples/acoustics/c-focused-planar-array.resonant
```

From a file manager: open the folder and run `resonant` ("Run as a
Program" or "Run" in most file managers). To start it from anywhere, link
the launcher into a folder on your `PATH`:

```bash
ln -s "$PWD/resonant" ~/.local/bin/resonant
```

`resonant.sh` is the same launcher under another name. Arguments are passed
to the application unchanged, and its exit status is returned.

## Check the system and report problems

```bash
./resonant --check-runtime                 # every check, without opening the main window
./resonant --diagnostics --report report.txt
./resonant --software-gl                   # Mesa's software OpenGL renderer (slower)
```

`--check-runtime` checks the package's files, the C library, Qt, OpenGL 4.5,
Python, the help, the examples, compute backends and plugins, and prints
`PASS`, `WARN` or `FAIL` for each. `--diagnostics` adds every loaded library.
A report shows your home folder as `~` and environment variables by name
only; it contains no project or setting. Nothing is ever sent: share a
report only if you want to.

If a system library is missing, `./resonant` stops before the program loads
and names the library and the package that provides it on Debian/Ubuntu,
Fedora and Arch.

## Help

- In the application: Help > Contents, or F1 on any panel.
- In a browser: open `help/index.html` (works offline, with search).

## Where your files go

| What | Where |
|---|---|
| Settings | `~/.config/Resonant/` |
| Data: scripts, demonstration runs, plugins | `~/.local/share/Resonant/` |
| Cache | `~/.cache/Resonant/` |
| Projects and exports | wherever you save them (Documents by default) |

The extracted folder is never written to, so it can be read-only or shared.

## Update

Extract the new ZIP next to the old one and start the new `./resonant`;
your settings and data stay where they are. Delete the old folder when you
no longer need it.

## Remove

Delete the extracted folder. To remove your settings and data as well,
delete the three folders in the table above.

## Known limitations

- glibc older than 2.38 (Ubuntu 22.04, Debian 12, RHEL 9) is not supported.
- Native Wayland has not been tested; on Wayland sessions Resonant uses XWayland.
- No desktop-menu entry is installed.
- HTTPS from Python scripts may need `SSL_CERT_FILE` on non-Debian systems.
- SDR device modules are not included (SoapySDR discovery uses the system's).

## Troubleshooting

| Symptom | What to do |
|---|---|
| "this system has glibc X; this package needs glibc 2.38 or newer" | use a newer distribution |
| "the system lacks libraries ..." | install the packages it names, then start again |
| "no graphical session" | start Resonant from a desktop session; `./resonant --check-runtime` works without one |
| `FAIL  OpenGL` in `--check-runtime` | update the GPU driver; try `./resonant --software-gl` |
| "bin/resonant is not executable" | extract again with `unzip` or a tool that keeps permissions |
| "the package is incomplete or damaged" | extract the ZIP again (and check its checksum) |
| The Python console reports an error | `./resonant --check-runtime` shows the bundled Python's state; `RESONANT_PYTHON` selects another interpreter on purpose |
| The NVIDIA GPU is not used on a hybrid laptop | start with `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./resonant` |
