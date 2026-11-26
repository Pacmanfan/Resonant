# GoogleTest 1.17.0 (vendored, BSD-3-Clause) as a static library.
TEMPLATE = lib
TARGET = resonant_gtest
CONFIG += staticlib c++20 warn_off
CONFIG -= qt debug_and_release debug_and_release_target

INCLUDEPATH += $$PWD/googletest/include $$PWD/googletest
SOURCES += $$PWD/googletest/src/gtest-all.cc
