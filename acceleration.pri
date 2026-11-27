# Optional acceleration for every qmake project of Resonant (included by
# warnings.pri; ADR-000212). Each part is used when found and can be turned
# off: qmake CONFIG+=no_volk, CONFIG+=no_cuda. The CMake build has the same
# switches (RESONANT_WITH_VOLK, RESONANT_WITH_CUDA).

# Libraries go into QMAKE_LIBS, which qmake places after LIBS on the link
# line: Resonant's static libraries (added to LIBS by each project) must come
# before the libraries they use, or the linker drops them.

# VOLK vector kernels (LGPL-3.0, dynamically linked) through pkg-config.
!no_volk:packagesExist(volk) {
    QMAKE_CXXFLAGS += $$system(pkg-config --cflags volk)
    QMAKE_LIBS += $$system(pkg-config --libs volk)
    DEFINES += RESONANT_HAVE_VOLK
}

# CUDA: the toolkit named by CUDA_DIR (qmake CUDA_DIR=...), or the one whose
# nvcc is on the PATH, or the standard /usr/local/cuda.
!no_cuda {
    isEmpty(CUDA_DIR) {
        NVCC_ON_PATH = $$system(command -v nvcc 2>/dev/null)
        !isEmpty(NVCC_ON_PATH): CUDA_DIR = $$clean_path($$dirname(NVCC_ON_PATH)/..)
        else:exists(/usr/local/cuda/bin/nvcc): CUDA_DIR = /usr/local/cuda
    }
    !isEmpty(CUDA_DIR):exists($$CUDA_DIR/bin/nvcc) {
        RESONANT_CUDA = 1
        # GPU architectures compiled into the library (real code), plus PTX of
        # the newest for later GPUs: Turing, Ampere, Ada, Blackwell.
        isEmpty(RESONANT_CUDA_ARCHS): RESONANT_CUDA_ARCHS = 75 86 89 120
        DEFINES += RESONANT_HAVE_CUDA
        # The runtime is linked statically: no CUDA library has to be shipped.
        QMAKE_LIBS += -L$$CUDA_DIR/lib64 -lcudart_static -ldl -lrt -lpthread
    }
}
