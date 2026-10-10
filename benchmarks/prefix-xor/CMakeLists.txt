cmake_minimum_required(VERSION 3.28)
project(prefix-xor-bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. benchmarksuite)

add_executable(prefix-xor-bench main.cpp)
target_link_libraries(prefix-xor-bench PRIVATE benchmarksuite::benchmarksuite)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
	target_compile_options(prefix-xor-bench PRIVATE -O3 -march=armv8.4-a+crypto)
elseif(MSVC)
	target_compile_options(prefix-xor-bench PRIVATE /O2 /arch:AVX2)
else()
	target_compile_options(prefix-xor-bench PRIVATE -O3 -march=native)
endif()
