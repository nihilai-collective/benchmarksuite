// MIT License @ /License.md
// Copyright (c) 2026 Nihilai Collective Corp
// https://github.com/nihilai-collective/benchmarksuite
// include/benchmarksuite-incl/benchmarksuite_gpu_properties.hpp
#pragma once

#include <benchmarksuite-incl/config.hpp>
#include <benchmarksuite-incl/aligned_const.hpp>

namespace benchmarksuite {

	struct BNCH_SWT_ALIGN(512) uint512_aligner {
		BNCH_SWT_ALIGN(512) uint64_t value{};

		BNCH_SWT_HOST consteval operator const uint64_t&() const BNCH_SWT_LIFETIME_BOUND {
			return value;
		}
	};

	struct gpu_properties {
	  protected:
		static constexpr uint512_aligner arg_alignment_raw{ 16ULL };
		static constexpr uint512_aligner flops_raw{ 178984960000000ULL };
		static constexpr uint512_aligner memory_bw_raw{ 896064000000ULL };
		static constexpr uint512_aligner alignment_raw{ 512ULL };
		static constexpr uint512_aligner sm_count_raw{ 70ULL };
		static constexpr uint512_aligner max_threads_per_sm_raw{ 1536ULL };
		static constexpr uint512_aligner max_threads_per_block_raw{ 1024ULL };
		static constexpr uint512_aligner warp_size_raw{ 32ULL };
		static constexpr uint512_aligner l2_cache_size_raw{ 50331648ULL };
		static constexpr uint512_aligner max_persisting_l2_bytes_raw{ 31457280ULL };
		static constexpr uint512_aligner shared_mem_per_sm_raw{ 49152ULL };
		static constexpr uint512_aligner max_grid_size_x_raw{ 2147483647ULL };
		static constexpr uint512_aligner max_grid_size_y_raw{ 65535ULL };
		static constexpr uint512_aligner max_grid_size_z_raw{ 65535ULL };
		static constexpr uint512_aligner gpu_arch_index_raw{ 4ULL };
		static constexpr uint512_aligner total_threads_raw{ 107520ULL };

	  public:
		static constexpr const uint64_t& arg_alignment{ arg_alignment_raw };
		static constexpr const uint64_t& flops{ flops_raw };
		static constexpr const uint64_t& memory_bw{ memory_bw_raw };
		static constexpr const uint64_t& alignment{ alignment_raw };
		static constexpr const uint64_t& sm_count{ sm_count_raw };
		static constexpr const uint64_t& max_threads_per_sm{ max_threads_per_sm_raw };
		static constexpr const uint64_t& max_threads_per_block{ max_threads_per_block_raw };
		static constexpr const uint64_t& warp_size{ warp_size_raw };
		static constexpr const uint64_t& l2_cache_size{ l2_cache_size_raw };
		static constexpr const uint64_t& max_persisting_l2_bytes{ max_persisting_l2_bytes_raw };
		static constexpr const uint64_t& shared_mem_per_sm{ shared_mem_per_sm_raw };
		static constexpr const uint64_t& max_grid_size_x{ max_grid_size_x_raw };
		static constexpr const uint64_t& max_grid_size_y{ max_grid_size_y_raw };
		static constexpr const uint64_t& max_grid_size_z{ max_grid_size_z_raw };
		static constexpr const uint64_t& total_threads{ total_threads_raw };
		static constexpr const uint64_t& gpu_arch_index{ gpu_arch_index_raw };
	};
}
