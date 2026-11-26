# Settings shared by every qmake project of Resonant (included by each .pro).
CONFIG += c++20 warn_on
CONFIG -= debug_and_release debug_and_release_target
QMAKE_CXXFLAGS += -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor

RESONANT_ROOT = $$PWD

# Third-party headers are system headers: their warnings are not ours.
QMAKE_CXXFLAGS += -isystem $$RESONANT_ROOT/third_party/nlohmann
CONFIG += link_pkgconfig
PKGCONFIG += eigen3

# Qt 6.4's moc cannot parse the C++20 syntax in libstdc++ 13 headers
# (<compare>, <concepts>, <numbers>, ...). moc only needs our own class
# declarations, so it is not given the compiler's own include directories:
# standard headers it cannot find are skipped. (CMake gets the same effect
# from CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF.)
load(moc)
MOC_INCLUDEPATH -= $$QMAKE_DEFAULT_INCDIRS
