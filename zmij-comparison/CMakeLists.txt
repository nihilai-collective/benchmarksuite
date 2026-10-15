include(FetchContent)

set(BNCH_SWT_JSONIFIER_DIR "${CMAKE_SOURCE_DIR}/../jsonifier" CACHE PATH "Path to the jsonifier repository")
set(BNCH_SWT_ZMIJ_UPSTREAM_TAG 57c99b0118e39ea4a1fcdf7aad634d5bce42730f CACHE STRING "Upstream zmij commit")

FetchContent_Declare(
    zmij_upstream
    GIT_REPOSITORY https://github.com/vitaut/zmij.git
    GIT_TAG ${BNCH_SWT_ZMIJ_UPSTREAM_TAG}
    SOURCE_SUBDIR do-not-add-subdirectory
)
FetchContent_MakeAvailable(zmij_upstream)

add_library(zmij-upstream STATIC
    ${zmij_upstream_SOURCE_DIR}/zmij.cc
    ${zmij_upstream_SOURCE_DIR}/zmij-int.cc
)
target_include_directories(zmij-upstream PUBLIC ${zmij_upstream_SOURCE_DIR})
set_target_properties(zmij-upstream PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(zmij-upstream PRIVATE voided_hw::bnch_swt_cpu)

if(EXISTS "${BNCH_SWT_JSONIFIER_DIR}/CMakeLists.txt")
    add_subdirectory(${BNCH_SWT_JSONIFIER_DIR} ${CMAKE_BINARY_DIR}/jsonifier EXCLUDE_FROM_ALL)
else()
    set(BNCH_SWT_JSONIFIER_TAG main CACHE STRING "Jsonifier git tag to fetch when no local checkout exists")
    FetchContent_Declare(
        jsonifier_remote
        GIT_REPOSITORY https://github.com/nihilai-collective/Jsonifier.git
        GIT_TAG ${BNCH_SWT_JSONIFIER_TAG}
        SOURCE_SUBDIR do-not-add-subdirectory
    )
    FetchContent_MakeAvailable(jsonifier_remote)
    add_subdirectory(${jsonifier_remote_SOURCE_DIR} ${CMAKE_BINARY_DIR}/jsonifier EXCLUDE_FROM_ALL)
endif()

add_executable(zmij-comparison main.cpp)
target_compile_features(zmij-comparison PRIVATE cxx_std_23)
target_link_libraries(zmij-comparison PRIVATE
    benchmarksuite::benchmarksuite
    Jsonifier::Jsonifier
    zmij-upstream
)

set_target_properties(zmij-upstream zmij-comparison PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
