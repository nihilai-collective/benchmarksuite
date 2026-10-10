# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Nihilai Collective Corp
# https://github.com/nihilai-collective/benchmarksuite
# src/CMakeLists.txt

set(MAIN_SOURCE main${BNCH_SWT_MAIN_FILE_EXTENSION})

add_executable(benchmarksuite-main ${MAIN_SOURCE})

if (NOT BNCH_SWT_CUDA)

    if (PRINT_ASSEMBLY)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            message(STATUS "Assembly printing enabled. Using Post-Build step to generate assembly file.")
        
            if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
                add_custom_command(
                    TARGET benchmarksuite-main POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:benchmarksuite-main>"
                    COMMAND ${CMAKE_COMMAND} -E echo "Assembly file location: $<TARGET_FILE_DIR:benchmarksuite-main>/benchmarksuite-main.asm"
                    COMMAND sh -c "otool -tV '$<TARGET_FILE:benchmarksuite-main>' > '$<TARGET_FILE_DIR:benchmarksuite-main>/benchmarksuite-main.asm'"
                    COMMENT "Generating final assembly for benchmarksuite-main"
                    VERBATIM
                )
            else()
                add_custom_command(
                    TARGET benchmarksuite-main POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:benchmarksuite-main>"
                    COMMAND ${CMAKE_COMMAND} -E echo "Assembly file location: $<TARGET_FILE_DIR:benchmarksuite-main>/benchmarksuite-main.asm"
                    COMMAND sh -c "objdump -d '$<TARGET_FILE:benchmarksuite-main>' > '$<TARGET_FILE_DIR:benchmarksuite-main>/benchmarksuite-main.asm'"
                    COMMENT "Generating final assembly for benchmarksuite-main"
                    VERBATIM
                )
            endif()
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            message(STATUS "Assembly printing enabled for MSVC. Using Post-Build dumpbin to generate assembly file.")

            add_custom_command(
                TARGET benchmarksuite-main POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Assembly file location: $<TARGET_FILE_DIR:benchmarksuite-main>/benchmarksuite-main.asm"
                COMMAND dumpbin /NOLOGO /DISASM "/OUT:$<TARGET_FILE_DIR:benchmarksuite-main>/benchmarksuite-main.asm" "$<TARGET_FILE:benchmarksuite-main>"
                COMMENT "Generating final assembly for benchmarksuite-main"
                VERBATIM
            )
        endif()
    endif()

endif()

include(FetchContent)

FetchContent_Declare(
    jsonifier
    GIT_REPOSITORY https://github.com/nihilai-collective/Jsonifier.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    SYSTEM
)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    SYSTEM
)

FetchContent_MakeAvailable(jsonifier simdjson)

set_target_properties(benchmarksuite-main PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_SCAN_FOR_MODULES OFF)

target_link_libraries(benchmarksuite-main PRIVATE benchmarksuite::benchmarksuite Jsonifier::Jsonifier simdjson::simdjson)

target_compile_definitions(
    benchmarksuite-main PRIVATE
    BNCH_SWT_STRING_CORPUS_DIR="${CMAKE_SOURCE_DIR}/data/string_corpus"
)

set(BNCH_SWT_MSVC_COMPILER_OPTIONS
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
    $<$<AND:$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<NOT:$<VERSION_LESS:$<CXX_COMPILER_VERSION>,23>>>:-Wno-lifetime-safety-lifetimebound-violation;-Wno-lifetime-safety-intra-tu-suggestions>
    $<$<AND:$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<NOT:$<VERSION_LESS:$<CXX_COMPILER_VERSION>,23>>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,24>>:-Wno-lifetime-safety-invalidation>
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
    benchmarksuite-main PUBLIC
    ${BNCH_SWT_COMPILER_OPTIONS}
)

install(
    FILES
        $<TARGET_FILE:benchmarksuite-main>
    DESTINATION bin
    OPTIONAL
)
