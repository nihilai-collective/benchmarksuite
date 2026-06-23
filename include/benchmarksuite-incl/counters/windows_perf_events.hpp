/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/counters/windows_perf_events.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

#if BNCH_SWT_PLATFORM_WINDOWS

namespace benchmarksuite::internal {

	template<benchmark_types benchmark_types, typename function_type> struct iteration_metric_collector {
		template<typename metric_type, typename... arg_types> BNCH_SWT_NOINLINE static void impl(metric_type& iteration_data, arg_types&&... args) {
			const auto start_clock				= clock_type::now();
			const volatile uint64_t cycle_start = __rdtsc();
			iteration_data.bytes_processed		= static_cast<uint64_t>(function_type::impl(std::forward<arg_types>(args)...));
			const volatile uint64_t cycle_end	= __rdtsc();
			const auto end_clock				= clock_type::now();
			iteration_data.time_in_ns			= (end_clock - start_clock).count();
			iteration_data.cycles.emplace(cycle_end - cycle_start);
		}
	};

}
#endif
