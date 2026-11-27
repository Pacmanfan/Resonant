# HDF5 for measurement data (ADR-000213), included by warnings.pri. Used when
# pkg-config finds it; turn it off with qmake CONFIG+=no_hdf5 (measurements
# are then stored in Resonant's binary format). CMake: RESONANT_WITH_HDF5.
!no_hdf5:packagesExist(hdf5) {
    HDF5_INCLUDES = $$system(pkg-config --cflags-only-I hdf5)
    QMAKE_CXXFLAGS += $$replace(HDF5_INCLUDES, -I, -isystem )
    # After LIBS on the link line, like the acceleration libraries.
    QMAKE_LIBS += $$system(pkg-config --libs hdf5)
    DEFINES += RESONANT_HAVE_HDF5
}
