function(bnch_swt_add_fused_key_tier tier_name tier_index)
    set(kernel_target fused-key-kernels-${tier_name})
    set(driver_target fused-key-compare-${tier_name})

    add_library(${kernel_target} OBJECT fused_key_kernels.cpp)
    target_compile_features(${kernel_target} PRIVATE cxx_std_20)
    target_compile_options(${kernel_target} PRIVATE ${BNCH_SWT_COMPILER_OPTIONS} ${ARGN})

    add_executable(${driver_target} main.cpp)
    target_compile_definitions(${driver_target} PRIVATE FUSED_KEY_TIER=${tier_index})
    target_link_libraries(${driver_target} PRIVATE benchmarksuite::benchmarksuite ${kernel_target})
    target_compile_options(${driver_target} PRIVATE ${BNCH_SWT_COMPILER_OPTIONS})

    install(
        FILES
            $<TARGET_FILE:${driver_target}>
        DESTINATION bin
        OPTIONAL
    )
endfunction()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    bnch_swt_add_fused_key_tier(neon 0)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    bnch_swt_add_fused_key_tier(avx2 1 /arch:AVX2)
    bnch_swt_add_fused_key_tier(avx512 2 /arch:AVX512)
else()
    set(BNCH_SWT_FUSED_KEY_AVX2_FLAGS -msse4.2 -mavx -mavx2 -mbmi -mbmi2 -mlzcnt -mpopcnt -mfma)
    bnch_swt_add_fused_key_tier(avx2 1 ${BNCH_SWT_FUSED_KEY_AVX2_FLAGS})
    bnch_swt_add_fused_key_tier(avx512 2 ${BNCH_SWT_FUSED_KEY_AVX2_FLAGS} -mavx512f -mavx512bw -mavx512vl -mavx512vbmi2)
endif()
