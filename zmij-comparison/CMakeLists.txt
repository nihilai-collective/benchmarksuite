include(FetchContent)

set(BNCH_SWT_JSONIFIER_DIR "${CMAKE_SOURCE_DIR}/../jsonifier" CACHE PATH "Path to the jsonifier repository")
set(BNCH_SWT_SIMDJSON_TAG master CACHE STRING "simdjson git tag")
FetchContent_Declare(
    simdjson_remote
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG ${BNCH_SWT_SIMDJSON_TAG}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(simdjson_remote)

if(EXISTS "${BNCH_SWT_JSONIFIER_DIR}/CMakeLists.txt")
    add_subdirectory(${BNCH_SWT_JSONIFIER_DIR} ${CMAKE_BINARY_DIR}/jsonifier EXCLUDE_FROM_ALL)
else()
    set(BNCH_SWT_JSONIFIER_TAG main CACHE STRING "Jsonifier git tag to fetch when no local checkout exists")
    FetchContent_Declare(
        jsonifier_remote
        GIT_REPOSITORY https://github.com/nihilai-collective/Jsonifier.git
        GIT_TAG ${BNCH_SWT_JSONIFIER_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(jsonifier_remote)
endif()

set(BNCH_SWT_JSONIFIER_DATA_DIR "${CMAKE_SOURCE_DIR}/../Jsonifier-Data" CACHE PATH "Path to the Jsonifier-Data repository")
if(NOT EXISTS "${BNCH_SWT_JSONIFIER_DATA_DIR}/unit-tests/fast_float_corpus")
    FetchContent_Declare(
        jsonifier_data_remote
        GIT_REPOSITORY https://github.com/nihilai-collective/Jsonifier-Data.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR do-not-add-subdirectory
    )
    FetchContent_MakeAvailable(jsonifier_data_remote)
    set(BNCH_SWT_JSONIFIER_DATA_DIR "${jsonifier_data_remote_SOURCE_DIR}" CACHE PATH "Path to the Jsonifier-Data repository" FORCE)
endif()

add_executable(zmij-comparison main.cpp)
target_compile_definitions(zmij-comparison PRIVATE BNCH_SWT_FLOAT_CORPUS_DIR="${BNCH_SWT_JSONIFIER_DATA_DIR}/unit-tests/fast_float_corpus")
target_compile_features(zmij-comparison PRIVATE cxx_std_23)
target_link_libraries(zmij-comparison PRIVATE
    benchmarksuite::benchmarksuite
    Jsonifier::Jsonifier
    simdjson::simdjson
)

set_target_properties(simdjson zmij-comparison PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
