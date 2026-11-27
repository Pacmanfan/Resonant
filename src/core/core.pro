# resonant_core: the engineering data model and physics. No Qt, no OpenGL.
include(../../warnings.pri)

TEMPLATE = lib
TARGET = resonant_core
CONFIG += staticlib
CONFIG -= qt

INCLUDEPATH += $$PWD
SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

# CUDA kernels (*.cu) when the toolkit was found (acceleration.pri). The
# objects go into the static library like any other.
equals(RESONANT_CUDA, 1) {
    CUDA_SOURCES = $$files($$PWD/*.cu, true)
    for(arch, RESONANT_CUDA_ARCHS): NVCC_GENCODE += -gencode arch=compute_$${arch},code=sm_$${arch}
    NEWEST_ARCH = $$last(RESONANT_CUDA_ARCHS)
    NVCC_GENCODE += -gencode arch=compute_$${NEWEST_ARCH},code=compute_$${NEWEST_ARCH}
    ARCH_LIST = $$join(RESONANT_CUDA_ARCHS, ",")
    cuda.input = CUDA_SOURCES
    cuda.output = ${QMAKE_FILE_BASE}_cuda.o
    # --fmad=false: no fused multiply-add, so the kernels compute the same
    # numbers as the CPU reference (the host build has no FMA contraction either).
    cuda.commands = $$CUDA_DIR/bin/nvcc -std=c++20 -O2 --fmad=false -Xcompiler -fPIC $$NVCC_GENCODE -DRESONANT_HAVE_CUDA \
                    -DRESONANT_CUDA_ARCHITECTURES=\\\"$$ARCH_LIST\\\" -I$$PWD -c ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
    cuda.dependency_type = TYPE_C
    cuda.variable_out = OBJECTS
    cuda.name = nvcc ${QMAKE_FILE_IN}
    QMAKE_EXTRA_COMPILERS += cuda
}
