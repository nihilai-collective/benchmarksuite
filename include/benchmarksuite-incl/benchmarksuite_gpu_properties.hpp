/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/benchmarksuite_gpu_properties.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

namespace benchmarksuite {

	struct gpu_properties {
		static constexpr uint64_t arg_alignment{ 16ULL };
		static constexpr uint64_t flops{ 178984960000000ULL };
		static constexpr uint64_t memory_bw{ 896064000000ULL };
		static constexpr uint64_t alignment{ 512ULL };
		static constexpr uint64_t sm_count{ 70ULL };
		static constexpr uint64_t max_threads_per_sm{ 1536ULL };
		static constexpr uint64_t max_threads_per_block{ 1024ULL };
		static constexpr uint64_t warp_size{ 32ULL };
		static constexpr uint64_t l2_cache_size{ 50331648ULL };
		static constexpr uint64_t max_persisting_l2_bytes{ 31457280ULL };
		static constexpr uint64_t shared_mem_per_sm{ 49152ULL };
		static constexpr uint64_t max_grid_size_x{ 2147483647ULL };
		static constexpr uint64_t max_grid_size_y{ 65535ULL };
		static constexpr uint64_t max_grid_size_z{ 65535ULL };
		static constexpr uint64_t gpu_arch_index{ 4ULL };
		static constexpr uint64_t total_threads{ 107520ULL };
	};
}
