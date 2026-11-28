# License Audit: the Linux ZIP

Every component in the Linux ZIP, the license it is distributed under, and
what that requires (RES-DIST-011, ADR-000218). The table is the content of
the package's `licenses/third_party/components.json` for release 0.1.0; its
source is `tools/release/third_party.json`, and `tools/release/package.py`
fails when a bundled file belongs to no component or a component's license
is not accepted. `tools/release/validate_licenses.py` checks the package.

## Resonant itself

No license has been chosen for Resonant's own code yet. The package's
`LICENSE` (and `licenses/application/LICENSE`) says so and grants nothing
beyond what the law allows (the options and what each requires are in
`LICENSING_DECISION.md`). **Choosing a license is a decision for the
project's owner** before the ZIP is published; the third-party obligations
below do not depend on that choice (no component requires Resonant to be
under a particular license: there is no GPL-only component).

## How components are included

- **Shared libraries** in `lib/` and Qt plugins in `lib/qt6/plugins/`:
  unmodified files from Ubuntu 24.04 packages, dynamically linked. Their
  Debian copyright files are in `licenses/third_party/<component>/`, the GNU
  and other common license texts in `licenses/common/`.
- **Python** (interpreter and standard library) and **Python packages**
  (PyPI wheels) in `python/`: unmodified; their license files are copied
  from the distribution's metadata.
- **Compiled in**: header-only libraries (Eigen, nlohmann/json, pybind11)
  and the static CUDA runtime.

## Obligations met

- **LGPL (Qt, GLib, GnuTLS, Nettle, GMP, libidn2, libunistring, libgcrypt,
  libgpg-error, Graphite2, libsystemd, libtasn1, librtmp, libssh, keyutils,
  VOLK)**: shipped as separate, unmodified shared libraries that the user
  can replace (search paths are relative; nothing is statically linked);
  license texts included; `THIRD_PARTY_NOTICES.txt` names the exact Ubuntu
  source package and version for each, whose sources Ubuntu publishes
  (`apt-get source <package>=<version>`, launchpad.net). **Open point for a
  public release:** LGPL-2.1 (section 6) and LGPL-3.0 (through GPL-3.0
  section 6) ask the distributor to provide the corresponding source with the
  binaries, or a written offer, or (LGPL-3.0) clear directions to a server
  that keeps it available. Publishing the source packages of the LGPL
  components next to the ZIP (`apt-get source` for each name and version in
  the notice) satisfies all of these; the pointer to Ubuntu's archive alone
  may not.
- **GPL with the GCC Runtime Library Exception (libgomp, NumPy's
  libgfortran)**: the exception permits distribution with the programs.
- **MPL-2.0 (Eigen)**: unmodified, the source location is named in the notice.
- **Apache-2.0 (OpenSSL, grpcio)**, **BSD/MIT/ISC-style** and the others:
  notices and license texts included.
- **Cyrus SASL**: its attribution sentence is in the notice.
- **CUDA runtime**: `libcudart_static` is redistributable under Attachment A
  of the CUDA Toolkit EULA (included); the NVIDIA driver is not bundled.
- **SoapySDR (BSL-1.0)**: the license text is included
  (`tools/release/licenses/SoapySDR-BSL-1.0.txt`); it is a source build on
  the build machine.

## Left out because of their licenses

Python's `readline` module (links GNU Readline, GPL-3.0) and the `gdbm` /
`dbm.gnu` modules (link GNU dbm, GPL-3.0). The console does not use them.

## Components

| Component | Version | License (as distributed) | Source | Linking | In the package |
|---|---|---|---|---|---|
| Brotli | 1.1.0-2ubuntu0.1 | `MIT` | https://github.com/google/brotli | shared library | lib/libbrotlicommon.so.1, lib/libbrotlidec.so.1 |
| bzip2 | 1.0.8-5.1ubuntu0.1 | `bzip2-1.0.6` | https://sourceware.org/bzip2/ | shared library | lib/libbz2.so.1.0 |
| com_err (e2fsprogs) | 1.47.0-2.4~exp1ubuntu4.1 | `MIT` | https://e2fsprogs.sourceforge.net/ | shared library | lib/libcom_err.so.2 |
| Cyrus SASL | 2.1.28+dfsg1-5ubuntu3.1 | `BSD-Attribution-HPND-disclaimer` | https://www.cyrusimap.org/sasl/ | shared library | lib/libsasl2.so.2 |
| D-Bus library | 1.14.10-4ubuntu4.1 | `AFL-2.1` | https://www.freedesktop.org/wiki/Software/dbus/ | shared library | lib/libdbus-1.so.3 |
| double-conversion | 3.3.0-1build1 | `BSD-3-Clause` | https://github.com/google/double-conversion | shared library | lib/libdouble-conversion.so.3 |
| Eigen | 3.4.0 | `MPL-2.0` | https://eigen.tuxfamily.org/ | header-only | compiled into bin/resonant, bin/resonant-automation, python/resonant/_resonant*.so |
| GLib | 2.80.0-6ubuntu3.10 | `LGPL-2.1-or-later` | https://gitlab.gnome.org/GNOME/glib | shared library | lib/libglib-2.0.so.0 |
| GMP | 2:6.3.0+dfsg-2ubuntu6.1 | `LGPL-3.0-or-later` | https://gmplib.org/ | shared library | lib/libgmp.so.10 |
| GNU OpenMP runtime (libgomp) | 14.2.0-4ubuntu2~24.04.1 | `GPL-3.0-or-later WITH GCC-exception-3.1` | https://gcc.gnu.org/ | shared library | lib/libgomp.so.1 |
| GnuTLS | 3.8.3-1.1ubuntu3.6 | `LGPL-2.1-or-later` | https://www.gnutls.org/ | shared library | lib/libgnutls.so.30 |
| Graphite2 | 1.3.14-2ubuntu0.24.04.1 | `LGPL-2.1-or-later` | https://github.com/silnrsi/graphite | shared library | lib/libgraphite2.so.3 |
| gRPC for Python (grpcio) | 1.78.0 | `Apache-2.0` | https://grpc.io/ | Python package | python/site-packages (grpcio) |
| HarfBuzz | 8.3.0-2build2 | `MIT` | https://harfbuzz.github.io/ | shared library | lib/libharfbuzz.so.0 |
| HDF5 | 1.10.10+repack-3.1ubuntu4 | `BSD-3-Clause` | https://www.hdfgroup.org/solutions/hdf5/ | shared library | lib/libhdf5_serial.so.103 |
| ICU | 74.2-1ubuntu3.1 | `Unicode-3.0` | https://icu.unicode.org/ | shared library | lib/libicudata.so.74, lib/libicui18n.so.74, lib/libicuuc.so.74 |
| JSON for Modern C++ (nlohmann/json) | 3.11.3 | `MIT` | https://github.com/nlohmann/json | header-only | compiled into bin/resonant, bin/resonant-automation, python/resonant/_resonant*.so |
| keyutils library | 1.6.3-3build1 | `LGPL-2.1-or-later` | https://people.redhat.com/~dhowells/keyutils/ | shared library | lib/libkeyutils.so.1 |
| libaec (szip compatible) | 1.1.2-1build1 | `BSD-2-Clause` | https://gitlab.dkrz.de/k202009/libaec | shared library | lib/libaec.so.0, lib/libsz.so.2 |
| libb2 (BLAKE2) | 0.98.1-1.1build1 | `CC0-1.0` | https://github.com/BLAKE2/libb2 | shared library | lib/libb2.so.1 |
| libcap | 1:2.66-5ubuntu2.4 | `BSD-3-Clause` | https://sites.google.com/site/fullycapable/ | shared library | lib/libcap.so.2 |
| libcurl | 8.5.0-2ubuntu10.16 | `curl` | https://curl.se/ | shared library | lib/libcurl.so.4 |
| libffi | 3.4.6-1build1 | `MIT` | https://sourceware.org/libffi/ | shared library | lib/libffi.so.8 |
| Libgcrypt | 1.10.3-2ubuntu0.2 | `LGPL-2.1-or-later` | https://gnupg.org/software/libgcrypt/ | shared library | lib/libgcrypt.so.20 |
| libgpg-error | 1.47-3build2.1 | `LGPL-2.1-or-later` | https://gnupg.org/software/libgpg-error/ | shared library | lib/libgpg-error.so.0 |
| Libidn2 | 2.3.7-2build1.1 | `LGPL-3.0-or-later` | https://www.gnu.org/software/libidn/ | shared library | lib/libidn2.so.0 |
| libjpeg-turbo | 2.1.5-2ubuntu2 | `IJG AND BSD-3-Clause AND Zlib` | https://libjpeg-turbo.org/ | shared library | lib/libjpeg.so.8 |
| libpng | 1.6.43-5ubuntu0.7 | `Libpng` | http://www.libpng.org/pub/png/libpng.html | shared library | lib/libpng16.so.16 |
| libpsl | 0.21.2-1.1build1 | `MIT` | https://rockdaboot.github.io/libpsl/ | shared library | lib/libpsl.so.5 |
| librtmp | 2.4+20151223.gitfa8646d.1-2build7 | `LGPL-2.1-or-later` | https://rtmpdump.mplayerhq.hu/ | shared library | lib/librtmp.so.1 |
| libssh | 0.10.6-2ubuntu0.6 | `LGPL-2.1-only` | https://www.libssh.org/ | shared library | lib/libssh.so.4 |
| libsystemd | 255.4-1ubuntu8.19 | `LGPL-2.1-or-later` | https://systemd.io/ | shared library | lib/libsystemd.so.0 |
| Libtasn1 | 4.19.0-3ubuntu0.24.04.2 | `LGPL-2.1-or-later` | https://www.gnu.org/software/libtasn1/ | shared library | lib/libtasn1.so.6 |
| libunistring | 1.1-2build1.1 | `LGPL-3.0-or-later` | https://www.gnu.org/software/libunistring/ | shared library | lib/libunistring.so.5 |
| libxkbcommon | 1.6.0-1build1 | `MIT` | https://xkbcommon.org/ | shared library | lib/libxkbcommon-x11.so.0, lib/libxkbcommon.so.0 |
| LZ4 | 1.9.4-1build1.1 | `BSD-2-Clause` | https://lz4.org/ | shared library | lib/liblz4.so.1 |
| MD4C | 0.4.8-1build1 | `MIT` | https://github.com/mity/md4c | shared library | lib/libmd4c.so.0 |
| MIT Kerberos | 1.20.1-6ubuntu2.12 | `MIT` | https://web.mit.edu/kerberos/ | shared library | lib/libgssapi_krb5.so.2, lib/libk5crypto.so.3, lib/libkrb5.so.3 (+1 more) |
| ncurses | 6.4+20240113-1ubuntu2.2 | `X11` | https://invisible-island.net/ncurses/ | shared library | lib/libncursesw.so.6, lib/libpanelw.so.6, lib/libtinfo.so.6 |
| Nettle | 3.9.1-2.2build1.1 | `LGPL-3.0-or-later` | https://www.lysator.liu.se/~nisse/nettle/ | shared library | lib/libhogweed.so.6, lib/libnettle.so.8 |
| nghttp2 | 1.59.0-1ubuntu0.4 | `MIT` | https://nghttp2.org/ | shared library | lib/libnghttp2.so.14 |
| NumPy (with OpenBLAS, libgfortran, libquadmath in numpy.libs) | 2.2.6 | `BSD-3-Clause AND GPL-3.0-or-later WITH GCC-exception-3.1 AND LGPL-2.1-or-later` | https://numpy.org/ | Python package | python/site-packages (numpy) |
| NVIDIA CUDA Runtime (static) | 13.5 | `LicenseRef-NVIDIA-CUDA-EULA` | https://developer.nvidia.com/cuda-toolkit | statically linked (libcudart_static) into the programs and the native module | compiled into bin/resonant, bin/resonant-automation, python/resonant/_resonant*.so |
| OpenLDAP libraries | 2.6.10+dfsg-0ubuntu0.24.04.1 | `OLDAP-2.8` | https://www.openldap.org/ | shared library | lib/liblber.so.2, lib/libldap.so.2 |
| OpenSSL | 3.0.13-0ubuntu3.16 | `Apache-2.0` | https://www.openssl.org/ | shared library | lib/libcrypto.so.3, lib/libssl.so.3 |
| Orc | 1:0.4.38-1ubuntu0.1 | `BSD-2-Clause` | https://gitlab.freedesktop.org/gstreamer/orc | shared library | lib/liborc-0.4.so.0 |
| p11-kit | 0.25.3-4ubuntu2.2 | `BSD-3-Clause` | https://p11-glue.github.io/p11-glue/p11-kit.html | shared library | lib/libp11-kit.so.0 |
| PCRE2 | 10.42-4ubuntu2.2 | `BSD-3-Clause` | https://github.com/PCRE2Project/pcre2 | shared library | lib/libpcre2-16.so.0, lib/libpcre2-8.so.0 |
| protobuf (Python) | 6.33.5 | `BSD-3-Clause` | https://protobuf.dev/ | Python package | python/site-packages (protobuf) |
| Protocol Buffers (C++ library) | 3.21.12-8.2ubuntu0.4 | `BSD-3-Clause` | https://protobuf.dev/ | shared library | lib/libprotobuf.so.32 |
| pybind11 | 2.11.1 | `BSD-3-Clause` | https://github.com/pybind/pybind11 | header-only | compiled into python/resonant/_resonant*.so |
| Python 3.12 (interpreter and standard library) | 3.12.3-1ubuntu0.17 | `PSF-2.0` | https://www.python.org/ | the interpreter in python/runtime | python/runtime/bin/python3, python/runtime/lib/python3.12 |
| Qt 6 (qtbase, qtsvg, qtwayland) | 6.4.2+dfsg-21.1build5, 6.4.2-4ubuntu3, 6.4.2-5build3 | `LGPL-3.0-only` | https://www.qt.io/ | shared library | lib/libQt6Core.so.6, lib/libQt6DBus.so.6, lib/libQt6Gui.so.6 (+23 more) |
| SoapySDR | 0.8.1 | `BSL-1.0` | https://github.com/pothosware/SoapySDR | shared library | lib/libSoapySDR.so.0.8-3 |
| SQLite | 3.45.1-1ubuntu2.9 | `blessing` | https://www.sqlite.org/ | shared library | lib/libsqlite3.so.0 |
| typing_extensions | 4.15.0 | `PSF-2.0` | https://github.com/python/typing_extensions | Python package | python/site-packages (typing_extensions) |
| VOLK | 3.1.2-1.1build1 | `LGPL-3.0-or-later` | https://www.libvolk.org/ | shared library | lib/libvolk.so.3.1 |
| xcb-util libraries (util, icccm, image, keysyms, render-util) | 0.3.9-1build4, 0.4.0-1build3, 0.4.0-1build4, 0.4.0-2build1, 0.4.1-1.1build3 | `MIT` | https://xcb.freedesktop.org/ | shared library | lib/libxcb-icccm.so.4, lib/libxcb-image.so.0, lib/libxcb-keysyms.so.1 (+2 more) |
| XZ Utils (liblzma) | 5.6.1+really5.4.5-1ubuntu0.4 | `0BSD` | https://tukaani.org/xz/ | shared library | lib/liblzma.so.5 |
| Zstandard | 1.5.5+dfsg2-2build1.1 | `BSD-3-Clause` | https://facebook.github.io/zstd/ | shared library | lib/libzstd.so.1 |
