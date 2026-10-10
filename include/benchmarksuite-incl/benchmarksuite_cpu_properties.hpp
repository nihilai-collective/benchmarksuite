/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/benchmarksuite_cpu_properties.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

namespace benchmarksuite {

	struct cpu_properties {
		static constexpr uint64_t thread_count{ 32ULL };
		static constexpr uint64_t l1_cache_size{ 49152ULL };
		static constexpr uint64_t l2_cache_size{ 2097152ULL };
		static constexpr uint64_t l3_cache_size{ 37748736ULL };
		static constexpr uint64_t cpu_arch_index{ 1ULL };
		static constexpr uint64_t cpu_alignment{ 32ULL };
	};

}
