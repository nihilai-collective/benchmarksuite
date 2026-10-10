/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/defines.hpp
 */

#pragma once

#define BNCH_SWT_STRINGIFY_IMPL(x) #x
#define BNCH_SWT_STRINGIFY(x) BNCH_SWT_STRINGIFY_IMPL(x)

#ifndef BNCH_SWT_COMPILER_CUDA
	#if defined(__CUDACC__)
		#define BNCH_SWT_COMPILER_CUDA 1
	#else
		#define BNCH_SWT_COMPILER_CUDA 0
	#endif
#endif

#ifndef BNCH_SWT_ARCH_X64
	#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
		#define BNCH_SWT_ARCH_X64 1
	#else
		#define BNCH_SWT_ARCH_X64 0
	#endif
#endif

#ifndef BNCH_SWT_ARCH_ARM64
	#if defined(__aarch64__) || defined(_M_ARM64)
		#define BNCH_SWT_ARCH_ARM64 1
	#else
		#define BNCH_SWT_ARCH_ARM64 0
	#endif
#endif

#ifndef BNCH_SWT_PLATFORM_ANDROID
	#if defined(__ANDROID__)
		#define BNCH_SWT_PLATFORM_ANDROID 1
	#else
		#define BNCH_SWT_PLATFORM_ANDROID 0
	#endif
#endif

#ifndef BNCH_SWT_PLATFORM_WINDOWS
	#if defined(_WIN32)
		#define BNCH_SWT_PLATFORM_WINDOWS 1
	#else
		#define BNCH_SWT_PLATFORM_WINDOWS 0
	#endif
#endif

#ifndef BNCH_SWT_PLATFORM_LINUX
	#if defined(__linux__) && !defined(__ANDROID__)
		#define BNCH_SWT_PLATFORM_LINUX 1
	#else
		#define BNCH_SWT_PLATFORM_LINUX 0
	#endif
#endif

#ifndef BNCH_SWT_PLATFORM_MAC
	#if defined(__APPLE__)
		#define BNCH_SWT_PLATFORM_MAC 1
	#else
		#define BNCH_SWT_PLATFORM_MAC 0
	#endif
#endif

#ifndef BNCH_SWT_COMPILER_CLANG
	#if defined(__clang__)
		#define BNCH_SWT_COMPILER_CLANG 1
	#else
		#define BNCH_SWT_COMPILER_CLANG 0
	#endif
#endif

#ifndef BNCH_SWT_COMPILER_MSVC
	#if defined(_MSC_VER) && !defined(__clang__)
		#define BNCH_SWT_COMPILER_MSVC 1
	#else
		#define BNCH_SWT_COMPILER_MSVC 0
	#endif
#endif

#ifndef BNCH_SWT_COMPILER_GCC
	#if defined(__GNUC__) && !defined(__clang__)
		#define BNCH_SWT_COMPILER_GCC 1
	#else
		#define BNCH_SWT_COMPILER_GCC 0
	#endif
#endif

#if BNCH_SWT_PLATFORM_WINDOWS && BNCH_SWT_COMPILER_MSVC
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
#endif

#if BNCH_SWT_COMPILER_CUDA
	#if defined(NDEBUG)
		#define BNCH_SWT_DEFAULT_INLINE_IMPL __forceinline__
	#else
		#define BNCH_SWT_DEFAULT_INLINE_IMPL inline __noinline__
	#endif
#elif BNCH_SWT_COMPILER_MSVC
	#if defined(NDEBUG)
		#define BNCH_SWT_DEFAULT_INLINE_IMPL [[msvc::forceinline]] inline
	#else
		#define BNCH_SWT_DEFAULT_INLINE_IMPL [[msvc::noinline]] inline
	#endif
#else
	#if defined(NDEBUG)
		#define BNCH_SWT_DEFAULT_INLINE_IMPL inline __attribute__((always_inline))
	#else
		#define BNCH_SWT_DEFAULT_INLINE_IMPL inline
	#endif
#endif

#ifndef BNCH_SWT_HOST_DEVICE
	#if BNCH_SWT_COMPILER_CUDA
		#define BNCH_SWT_HOST_DEVICE BNCH_SWT_DEFAULT_INLINE_IMPL __host__ __device__
	#else
		#define BNCH_SWT_HOST_DEVICE BNCH_SWT_DEFAULT_INLINE_IMPL
	#endif
#endif

#ifndef BNCH_SWT_HOST
	#if BNCH_SWT_COMPILER_CUDA
		#define BNCH_SWT_HOST BNCH_SWT_DEFAULT_INLINE_IMPL __host__
	#else
		#define BNCH_SWT_HOST BNCH_SWT_DEFAULT_INLINE_IMPL
	#endif
#endif

#ifndef BNCH_SWT_DEVICE
	#if BNCH_SWT_COMPILER_CUDA
		#define BNCH_SWT_DEVICE BNCH_SWT_DEFAULT_INLINE_IMPL __device__
	#else
		#define BNCH_SWT_DEVICE BNCH_SWT_DEFAULT_INLINE_IMPL
	#endif
#endif

#ifndef BNCH_SWT_NOINLINE
	#if BNCH_SWT_COMPILER_CUDA
		#define BNCH_SWT_NOINLINE inline __noinline__
	#elif BNCH_SWT_COMPILER_MSVC
		#define BNCH_SWT_NOINLINE [[msvc::noinline]] inline
	#else
		#define BNCH_SWT_NOINLINE __attribute__((noinline)) inline
	#endif
#endif

#ifndef BNCH_SWT_LIFETIME_BOUND
	#if BNCH_SWT_COMPILER_CLANG
		#define BNCH_SWT_LIFETIME_BOUND [[clang::lifetimebound]]
	#elif BNCH_SWT_COMPILER_MSVC
		#define BNCH_SWT_LIFETIME_BOUND [[msvc::lifetimebound]]
	#else
		#define BNCH_SWT_LIFETIME_BOUND
	#endif
#endif

#ifndef BNCH_SWT_GLOBAL
	#define BNCH_SWT_GLOBAL __global__
#endif

#ifndef BNCH_SWT_COMPILER_VERSION
	#if BNCH_SWT_COMPILER_CUDA
		#define BNCH_SWT_COMPILER_VERSION BNCH_SWT_STRINGIFY(__CUDACC_VER_MAJOR__.__CUDACC_VER_MINOR__.__CUDACC_VER_BUILD__)
	#elif BNCH_SWT_COMPILER_CLANG
		#define BNCH_SWT_COMPILER_VERSION BNCH_SWT_STRINGIFY(__clang_major__.__clang_minor__.__clang_patchlevel__)
	#elif BNCH_SWT_COMPILER_MSVC
		#define BNCH_SWT_COMPILER_VERSION BNCH_SWT_STRINGIFY(_MSC_FULL_VER)
	#elif BNCH_SWT_COMPILER_GCC
		#define BNCH_SWT_COMPILER_VERSION BNCH_SWT_STRINGIFY(__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__)
	#else
		#define BNCH_SWT_COMPILER_VERSION "unknown"
	#endif
#endif

#ifndef BNCH_SWT_OPERATING_SYSTEM_VERSION
	#define BNCH_SWT_OPERATING_SYSTEM_VERSION "unknown"
#endif

#ifndef BNCH_SWT_INSTRUCTION_SET_NAME
	#define BNCH_SWT_INSTRUCTION_SET_NAME "unknown"
#endif

#ifndef BNCH_SWT_CPU_SUPPORT
	#define BNCH_SWT_CPU_SUPPORT 1
#endif

#ifndef BNCH_SWT_GPU_SUPPORT
	#define BNCH_SWT_GPU_SUPPORT BNCH_SWT_COMPILER_CUDA
#endif
