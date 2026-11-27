# Optional instrument libraries for src/hardware (ADR-000213), included by
# warnings.pri. Each is used when pkg-config finds it and can be turned off:
# qmake CONFIG+=no_alsa, CONFIG+=no_soapysdr. CMake: RESONANT_WITH_ALSA,
# RESONANT_WITH_SOAPYSDR. Without them the hardware library has the
# simulated instruments only.

# ALSA (LGPL-2.1, dynamically linked): Linux audio interfaces.
!no_alsa:packagesExist(alsa) {
    QMAKE_LIBS += $$system(pkg-config --libs alsa)
    DEFINES += RESONANT_HAVE_ALSA
}

# SoapySDR (BSL-1.0): software-defined radios (discovery and capabilities).
!no_soapysdr:packagesExist(SoapySDR) {
    SOAPY_INCLUDES = $$system(pkg-config --cflags-only-I SoapySDR)
    QMAKE_CXXFLAGS += $$replace(SOAPY_INCLUDES, -I, -isystem )
    QMAKE_LIBS += $$system(pkg-config --libs SoapySDR)
    DEFINES += RESONANT_HAVE_SOAPYSDR
}
