# MIT License @ /License.md
# Copyright (c) 2026 Nihilai Collective Corp
# https://github.com/nihilai-collective/benchmarksuite
# unit-tests/CMakeLists.txt

add_executable(benchmarksuite-unit-tests main.cpp)

include(FetchContent)

FetchContent_Declare(
  rt-ut
  GIT_REPOSITORY https://github.com/nihilai-collective/rt-ut.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rt-ut)

target_link_libraries(benchmarksuite-unit-tests
    PUBLIC
        benchmarksuite::benchmarksuite rt-ut::rt-ut
)

option(BNCH_SWT_ASAN  "Enable AddressSanitizer"            OFF)
option(BNCH_SWT_UBSAN "Enable UndefinedBehaviorSanitizer"  OFF)

set(BNCH_SWT_ASAN_EFFECTIVE  ${BNCH_SWT_ASAN})
set(BNCH_SWT_UBSAN_EFFECTIVE ${BNCH_SWT_UBSAN})

if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(BNCH_SWT_ASAN_EFFECTIVE OFF)
    set(BNCH_SWT_UBSAN_EFFECTIVE OFF)
    message(STATUS "Disabling sanitizers: not supported with GCC on macOS.")
endif()

if(BNCH_SWT_UBSAN_EFFECTIVE AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(WARNING
        "BNCH_SWT_UBSAN has no effect with MSVC (no equivalent runtime). "
        "Use Clang-cl or a Clang/GCC build to get UBSan coverage.")
    set(BNCH_SWT_UBSAN_EFFECTIVE OFF)
endif()

set(BNCH_SWT_HOMEBREW_GCC_LIBDIR "")
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    find_program(BNCH_SWT_BREW_EXE brew)
    if(BNCH_SWT_BREW_EXE)
        execute_process(
            COMMAND ${BNCH_SWT_BREW_EXE} --prefix gcc
            OUTPUT_VARIABLE BNCH_SWT_BREW_GCC_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE BNCH_SWT_BREW_RC
        )
        if(BNCH_SWT_BREW_RC EQUAL 0 AND BNCH_SWT_BREW_GCC_PREFIX)
            file(GLOB BNCH_SWT_GCC_LIB_CANDIDATES
                "${BNCH_SWT_BREW_GCC_PREFIX}/lib/gcc/*")
            if(BNCH_SWT_GCC_LIB_CANDIDATES)
                list(SORT BNCH_SWT_GCC_LIB_CANDIDATES)
                list(REVERSE BNCH_SWT_GCC_LIB_CANDIDATES)
                list(GET BNCH_SWT_GCC_LIB_CANDIDATES 0 BNCH_SWT_HOMEBREW_GCC_LIBDIR)
            endif()
        endif()
    endif()
    if(NOT BNCH_SWT_HOMEBREW_GCC_LIBDIR)
        message(STATUS
            "Could not auto-detect Homebrew GCC runtime path; link-time "
            "rpath for libstdc++ will not be added. Set "
            "BNCH_SWT_HOMEBREW_GCC_LIBDIR manually if needed.")
    endif()
endif()

set(BNCH_SWT_MSVC_COMPILER_OPTIONS
    $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>>>:/Od;/Zi;/RTC1>
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>>:/Od;/Zi>
    $<$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>:/fsanitize=address>
    /wd4820
    /wd5039
    /wd4710
    /wd4711
    /wd5045
    /Wall
    /W4
    /WX
)

set(BNCH_SWT_CLANG_COMPILER_OPTIONS
    $<$<AND:$<CONFIG:Release>,$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>>:-O1;-fno-rtti>
    $<$<CONFIG:Debug>:-O0;-g;-fno-omit-frame-pointer>
    $<$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>:-fsanitize=address;-fno-omit-frame-pointer;-fno-optimize-sibling-calls;-fsanitize-address-use-after-scope;-U_FORTIFY_SOURCE;-D_FORTIFY_SOURCE=0>
    $<$<BOOL:${BNCH_SWT_UBSAN_EFFECTIVE}>:-fsanitize=undefined;-fno-sanitize-recover=all>
    $<$<AND:$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<NOT:$<VERSION_LESS:$<CXX_COMPILER_VERSION>,23>>>:-Wno-lifetime-safety-lifetimebound-violation;-Wno-lifetime-safety-intra-tu-suggestions;-Wno-lifetime-safety-invalidation>
    -Weverything
    -Wpedantic
    -Werror
    -Wno-unsafe-buffer-usage
    -Wno-c++20-extensions
    -Wextra
    -Wall
    -Wno-c++98-compat-pedantic
    -Wno-c++98-compat
    -Wno-c++20-compat
    -Wno-padded
)

set(BNCH_SWT_GNU_COMPILER_OPTIONS
    $<$<CONFIG:Debug>:-O0;-g>
    $<$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>:-fsanitize=address;-fsanitize-address-use-after-scope;-U_FORTIFY_SOURCE;-D_FORTIFY_SOURCE=0>
    $<$<BOOL:${BNCH_SWT_UBSAN_EFFECTIVE}>:-fsanitize=undefined;-fno-sanitize-recover=all>
    -Wpedantic
    -Werror
    -Wextra
    -Wall
)

set(BNCH_SWT_COMPILER_OPTIONS
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${BNCH_SWT_CLANG_COMPILER_OPTIONS}>
    $<$<CXX_COMPILER_ID:GNU>:${BNCH_SWT_GNU_COMPILER_OPTIONS}>
    $<$<CXX_COMPILER_ID:MSVC>:${BNCH_SWT_MSVC_COMPILER_OPTIONS}>
)

target_compile_options(
    benchmarksuite-unit-tests PUBLIC
    ${BNCH_SWT_COMPILER_OPTIONS}
)

set(BNCH_SWT_MSVC_LINK_OPTIONS
    $<$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>:/DEBUG>
    $<$<BOOL:${BNCH_SWT_UBSAN_EFFECTIVE}>:/DEBUG>
)

set(BNCH_SWT_CLANG_LINK_OPTIONS
    $<$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>:-fsanitize=address>
    $<$<BOOL:${BNCH_SWT_UBSAN_EFFECTIVE}>:-fsanitize=undefined>
)

set(BNCH_SWT_GNU_LINK_OPTIONS
    $<$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>:-fsanitize=address>
    $<$<BOOL:${BNCH_SWT_UBSAN_EFFECTIVE}>:-fsanitize=undefined>
    $<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${BNCH_SWT_ASAN_EFFECTIVE}>>:-static-libasan>
)

set(BNCH_SWT_LINK_OPTIONS
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${BNCH_SWT_CLANG_LINK_OPTIONS}>
    $<$<CXX_COMPILER_ID:GNU>:${BNCH_SWT_GNU_LINK_OPTIONS}>
    $<$<CXX_COMPILER_ID:MSVC>:${BNCH_SWT_MSVC_LINK_OPTIONS}>
)

target_link_options(benchmarksuite-unit-tests
    PUBLIC ${BNCH_SWT_LINK_OPTIONS}
)

install(
    FILES
        $<TARGET_FILE:benchmarksuite-unit-tests>
    DESTINATION bin
    OPTIONAL
)
