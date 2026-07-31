# MIT License @ /License.md
# Copyright (c) 2026 Nihilai Collective Corp
# https://github.com/nihilai-collective/benchmarksuite
# cmake/detection/CMakeLists.txt

cmake_minimum_required(VERSION 3.28)

project(feature_detector
    VERSION 1.0.0
    LANGUAGES CXX
)

add_executable(feature_detector main.cpp)

if(BNCH_SWT_DETECT_CPU_PROPERTIES)
    target_compile_definitions(feature_detector
        PUBLIC
            BNCH_SWT_DETECT_CPU_PROPERTIES
            ${BNCH_SWT_COMPILE_DEFINITIONS}
    )
    
elseif(BNCH_SWT_DETECT_GPU_PROPERTIES)
    find_package(CUDAToolkit REQUIRED)
    set_property(GLOBAL PROPERTY CUDA_ARCHITECTURES native)

    target_link_libraries(feature_detector
        PUBLIC
            CUDA::cudart
    )
    
    target_compile_definitions(feature_detector
        PUBLIC
            BNCH_SWT_DETECT_GPU_PROPERTIES
            ${BNCH_SWT_COMPILE_DEFINITIONS}
    )
endif()