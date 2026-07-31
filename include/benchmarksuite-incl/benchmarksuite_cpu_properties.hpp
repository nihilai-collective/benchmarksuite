/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/benchmarksuite_cpu_properties.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

namespace benchmarksuite {

	struct BNCH_SWT_ALIGN(64) uint64_holder {
		BNCH_SWT_ALIGN(64) uint64_t value {};

		BNCH_SWT_HOST constexpr operator const uint64_t&() const BNCH_SWT_LIFETIME_BOUND {
			return value;
		}
	};

	struct cpu_properties {
	  protected:
		static constexpr uint64_holder thread_count_raw{ 32ULL };
		static constexpr uint64_holder l1_cache_size_raw{ 49152ULL };
		static constexpr uint64_holder l2_cache_size_raw{ 2097152ULL };
		static constexpr uint64_holder l3_cache_size_raw{ 37748736ULL };
		static constexpr uint64_holder cpu_arch_index_raw{ 1ULL };
		static constexpr uint64_holder cpu_alignment_raw{ 32ULL };

	  public:
		static constexpr const uint64_t& thread_count{ thread_count_raw };
		static constexpr const uint64_t& l1_cache_size{ l1_cache_size_raw };
		static constexpr const uint64_t& l2_cache_size{ l2_cache_size_raw };
		static constexpr const uint64_t& l3_cache_size{ l3_cache_size_raw };
		static constexpr const uint64_t& cpu_arch_index{ cpu_arch_index_raw };
		static constexpr const uint64_t& cpu_alignment{ cpu_alignment_raw };
	};

}
