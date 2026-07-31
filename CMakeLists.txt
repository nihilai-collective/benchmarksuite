# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Nihilai Collective Corp
# https://github.com/nihilai-collective/benchmarksuite
# CMakeLists.txt

cmake_minimum_required(VERSION 3.28)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BNCH_SWT_DETECT_CPU_PROPERTIES "Override cpu-cache-size selection" OFF)
option(BNCH_SWT_DETECT_GPU_PROPERTIES "Override cuda-cache-size selection" OFF)
option(BNCH_SWT_CUDA "Enable CUDA support" OFF)

include(cmake/get_version.cmake)

if(BNCH_SWT_CUDA)
    set(BNCH_SWT_LANGUAGE CUDA)
    project(benchmarksuite
        VERSION ${CLEAN_VERSION}
        DESCRIPTION "A header-only C++ benchmarking library with cross-platform hardware performance counter integration, providing precise measurements of cycles, instructions, branches, cache behavior, and throughput with minimal overhead."
        LANGUAGES CUDA 
    )
    find_package(CUDAToolkit REQUIRED COMPONENTS cudart)
    include(cmake/detection/benchmarksuite_detect_gpu_properties.cmake)
    set(CMAKE_CUDA_ARCHITECTURES ${BNCH_SWT_MAJOR_COMPUTE_CAPABILITY}${BNCH_SWT_MINOR_COMPUTE_CAPABILITY} CACHE STRING "Cuda architectures." FORCE)
    set(BNCH_SWT_MAIN_FILE_EXTENSION .cu)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
else()
    set(BNCH_SWT_LANGUAGE CXX)
    project(benchmarksuite
        VERSION ${CLEAN_VERSION}
        DESCRIPTION "A header-only C++ benchmarking library with cross-platform hardware performance counter integration, providing precise measurements of cycles, instructions, branches, cache behavior, and throughput with minimal overhead."
        LANGUAGES CXX
    )
    set(BNCH_SWT_MAIN_FILE_EXTENSION .cpp)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

include(cmake/detection/benchmarksuite_detect_cpu_properties.cmake)

include(cmake/library_setup.cmake)

include(cmake/installation_setup.cmake)

message(STATUS "benchmarksuite Configuration Summary:")
message(STATUS "=============================")
message(STATUS "Library Version: ${CLEAN_VERSION}")
message(STATUS "Max Thread Count: ${BNCH_SWT_THREAD_COUNT}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${BNCH_SWT_COMPILER_ID}")
message(STATUS "CPU Variants: ${BNCH_SWT_INSTRUCTION_SET_NAME}")

if(BNCH_SWT_CUDA)
    message(STATUS "CUDA Architectures: ${CMAKE_CUDA_ARCHITECTURES}")
endif()

if(BNCH_SWT_BENCHMARKS)
    add_subdirectory(src)
endif()
if(BNCH_SWT_UNIT_TESTS)
    add_subdirectory(unit-tests)
endif()