/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/config.hpp
 */

#pragma once

#include <source_location>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <optional>
#include <fstream>
#include <variant>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <vector>
#include <mutex>
#include <cmath>
#include <array>
#include <span>

#if BNCH_SWT_COMPILER_CUDA
	#define BNCH_SWT_ALIGN(x) __align__(x)
	#include <cuda_fp16.h>
	#include <cuda_bf16.h>
#else
	#define BNCH_SWT_ALIGN(x) alignas(x)
#endif

#if BNCH_SWT_PLATFORM_WINDOWS
	#include <Windows.h>
	#include <intrin.h>
#elif BNCH_SWT_PLATFORM_MAC
	#include <libkern/OSCacheControl.h>
	#include <sys/sysctl.h>
	#include <unistd.h>
	#include <dlfcn.h>
	#include <pthread.h>
	#include <sys/qos.h>
	#include <mach/mach.h>
	#include <mach/thread_policy.h>
	#include <mach/thread_act.h>
#elif BNCH_SWT_PLATFORM_LINUX
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	#endif
	#include <asm/unistd.h>
	#include <linux/perf_event.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
	#include <pthread.h>
	#include <sched.h>
	#include <sys/resource.h>
	#include <errno.h>
	#if defined(__i386__) || defined(__x86_64__)
		#include <immintrin.h>
		#include <cpuid.h>
	#endif
#elif BNCH_SWT_PLATFORM_ANDROID
	#if defined(__x86_64__) || defined(__i386__)
		#include <cpuid.h>
	#endif
	#include <sys/system_properties.h>
	#include <pthread.h>
	#include <sched.h>
	#include <sys/resource.h>
	#include <errno.h>
#endif

namespace benchmarksuite {

	using nanoseconds  = std::chrono::duration<double, std::nano>;
	using milliseconds = std::chrono::duration<double, std::milli>;

	struct steady_clock {
		using clock_type			  = std::conditional_t<std::chrono::high_resolution_clock::is_steady, std::chrono::high_resolution_clock, std::chrono::steady_clock>;
		using seconds				  = std::chrono::duration<double>;
		using time_point_type_nano	  = std::chrono::time_point<clock_type, nanoseconds>;
		using time_point_type_seconds = std::chrono::time_point<clock_type, seconds>;
		using rep					  = double;
		using period				  = std::nano;
		using duration				  = std::chrono::duration<rep, period>;
		using time_point			  = std::chrono::time_point<steady_clock>;

		static constexpr bool is_steady = true;

		static time_point now() noexcept {
			auto system_now			  = std::chrono::steady_clock::now();
			auto duration_since_epoch = system_now.time_since_epoch();

			auto casted_duration = std::chrono::duration_cast<duration>(duration_since_epoch);

			return time_point(casted_duration);
		}
	};

	using clock_type = steady_clock;

	enum class benchmark_types {
		cpu,
		cuda,
	};

	namespace internal {

		template<typename event_count, benchmark_types, uint64_t count> struct event_collector_type;

	}

	template<typename value_type> using base_t = std::remove_cvref_t<value_type>;

	struct iteration_metrics {
		std::optional<uint64_t> cache_references_val{};
		std::optional<uint64_t> bytes_processed_val{};
		std::optional<nanoseconds> elapsed_ns_val{};
		std::optional<uint64_t> branch_misses_val{};
		std::optional<uint64_t> instructions_val{};
		std::optional<uint64_t> cache_misses_val{};
		std::optional<uint64_t> branches_val{};
		std::optional<uint64_t> cycles;
		uint64_t bytes_processed;
		double time_in_ns;
	};

}
