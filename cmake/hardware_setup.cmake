# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Nihilai Collective Corp
# https://github.com/nihilai-collective/benchmarksuite
# cmake/hardware_setup.cmake

include(FetchContent)

FetchContent_Declare(
    voided_hw_detection
    GIT_REPOSITORY https://github.com/nihilai-collective/voided-hw-detection.git
    GIT_TAG main
)
set(BNCH_SWT_SAVED_FETCHCONTENT_FULLY_DISCONNECTED ${FETCHCONTENT_FULLY_DISCONNECTED})
set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
FetchContent_MakeAvailable(voided_hw_detection)
set(FETCHCONTENT_FULLY_DISCONNECTED ${BNCH_SWT_SAVED_FETCHCONTENT_FULLY_DISCONNECTED})

if(BNCH_SWT_CUDA)
    voided_hw_detect_gpu(
        PREFIX BNCH_SWT
        TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmarksuite_gpu_properties.hpp.in
        HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/benchmarksuite-incl/benchmarksuite_gpu_properties.hpp
    )
endif()

voided_hw_detect_cpu(
    PREFIX BNCH_SWT
    TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmarksuite_cpu_properties.hpp.in
    HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/benchmarksuite-incl/benchmarksuite_cpu_properties.hpp
)
