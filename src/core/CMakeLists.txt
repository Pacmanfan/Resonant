# resonant_core: the engineering data model and physics. No Qt, no OpenGL.
file(GLOB_RECURSE RESONANT_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE RESONANT_CORE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(resonant_core STATIC ${RESONANT_CORE_SOURCES} ${RESONANT_CORE_HEADERS})
target_include_directories(resonant_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(resonant_core PUBLIC Eigen3::Eigen resonant_json Threads::Threads PRIVATE resonant_warnings)

if(RESONANT_HAVE_VOLK)
    target_compile_definitions(resonant_core PUBLIC RESONANT_HAVE_VOLK)
    target_link_libraries(resonant_core PUBLIC PkgConfig::VOLK)
endif()
if(RESONANT_HAVE_CUDA)
    file(GLOB_RECURSE RESONANT_CORE_CUDA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cu)
    target_sources(resonant_core PRIVATE ${RESONANT_CORE_CUDA_SOURCES})
    # No fused multiply-add: the kernels then compute the same numbers as the
    # CPU reference (the host build has no FMA contraction either).
    target_compile_options(resonant_core PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--fmad=false>)
    string(REPLACE ";" "," resonant_cuda_arch_list "${CMAKE_CUDA_ARCHITECTURES}")
    target_compile_definitions(resonant_core PUBLIC RESONANT_HAVE_CUDA
                               PRIVATE RESONANT_CUDA_ARCHITECTURES="${resonant_cuda_arch_list}")
    # The runtime is linked statically: no CUDA library has to be shipped.
    target_link_libraries(resonant_core PUBLIC CUDA::cudart_static)
endif()
