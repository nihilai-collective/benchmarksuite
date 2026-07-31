// MIT License @ /License.md
// Copyright (c) 2026 Nihilai Collective Corp
// https://github.com/nihilai-collective/benchmarksuite
// include/benchmarksuite-incl/counters/linux_perf_events.hpp

#pragma once

#include <benchmarksuite-incl/config.hpp>

#if BNCH_SWT_PLATFORM_LINUX

namespace benchmarksuite::internal {

	BNCH_SWT_HOST static uint64_t rdtsc() {
		uint32_t a, d;
		__asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
		return static_cast<unsigned long>(a) | (static_cast<unsigned long>(d) << 32);
	}

	template<benchmark_types benchmark_types, typename function_type> struct iteration_metric_collector {
		template<typename metric_type, typename... arg_types> BNCH_SWT_NOINLINE static void impl(metric_type& iteration_data, arg_types&&... args) {
			const auto start_clock				= clock_type::now();
			const volatile uint64_t cycle_start = rdtsc();
			iteration_data.bytes_processed		= static_cast<uint64_t>(function_type::impl(std::forward<arg_types>(args)...));
			const volatile uint64_t cycle_end	= rdtsc();
			const auto end_clock				= clock_type::now();
			iteration_data.time_in_ns			= (end_clock - start_clock).count();
			iteration_data.cycles.emplace(cycle_end - cycle_start);
		}
	};

}

#endif
