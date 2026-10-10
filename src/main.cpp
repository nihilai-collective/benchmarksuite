/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/main.cpp
 */

#include <benchmarksuite>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <vector>

#if BNCH_SWT_COMPILER_GCC
	#pragma GCC diagnostic ignored "-Warray-bounds"
	#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

static constexpr uint64_t copies_per_iteration{ 4096 };
static constexpr uint64_t buffer_bytes{ 1 << 16 };
static constexpr uint64_t buffer_mask{ buffer_bytes - 1 };
static constexpr uint64_t offset_stride{ 192 };
static constexpr uint64_t offset_alignment_mask{ ~uint64_t{ 255 } };

template<typename value_type> BNCH_SWT_HOST uint64_t countlZero(value_type value) noexcept {
#if BNCH_SWT_ARCH_X64 && (BNCH_SWT_COMPILER_GCC || BNCH_SWT_COMPILER_CLANG)
	return static_cast<uint64_t>(_lzcnt_u64(value));
#elif BNCH_SWT_ARCH_X64 && BNCH_SWT_COMPILER_MSVC
	return static_cast<uint64_t>(__lzcnt64(value));
#elif BNCH_SWT_ARCH_ARM64 && (BNCH_SWT_COMPILER_GCC || BNCH_SWT_COMPILER_CLANG)
	return value == 0 ? 64ULL : static_cast<uint64_t>(__builtin_clzll(value));
#elif BNCH_SWT_ARCH_ARM64 && BNCH_SWT_COMPILER_MSVC
	return static_cast<uint64_t>(_CountLeadingZeros64(value));
#else
	return static_cast<uint64_t>(std::countl_zero(value));
#endif
}

static volatile uint64_t runtime_size_source{};

static constexpr auto alignment{ benchmarksuite::cpu_properties::cpu_alignment };

template<uint64_t byte_count, typename value_type> BNCH_SWT_HOST void pow2MemcpyWrapper(value_type* __restrict dst, const value_type* __restrict src) {
	static_assert(std::has_single_bit(byte_count), "Sorry, but you can only memcpy a power-of-2 size.");
	std::memcpy(dst, src, byte_count);
}

BNCH_SWT_HOST void memcpyWrapper(void* __restrict dst, const void* __restrict src, uint64_t byte_count) {
	std::memcpy(dst, src, byte_count);
}

template<uint64_t alignment, uint64_t byte_count> struct memcpy_runner;

template<uint64_t alignment, uint64_t byte_count>
	requires(alignment >= byte_count)
struct memcpy_runner<alignment, byte_count> {
	BNCH_SWT_HOST static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count_rt) noexcept {
		pow2MemcpyWrapper<byte_count>(dst, src);
		pow2MemcpyWrapper<byte_count>(dst + byte_count_rt - byte_count, src + byte_count_rt - byte_count);
	}
};

template<uint64_t alignment, uint64_t byte_count>
	requires(alignment == (byte_count / 2))
struct memcpy_runner<alignment, byte_count> {
	static constexpr uint64_t byte_count_local{ byte_count / 2 };
	static constexpr uint64_t local_x1{ byte_count_local };
	static constexpr uint64_t local_x2{ local_x1 + byte_count_local };

	BNCH_SWT_HOST static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count_rt) noexcept {
		pow2MemcpyWrapper<byte_count_local>(dst, src);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x1, src + local_x1);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x2, src + byte_count_rt - local_x2);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x1, src + byte_count_rt - local_x1);
	}
};

template<uint64_t alignment, uint64_t byte_count>
	requires(alignment == (byte_count / 4))
struct memcpy_runner<alignment, byte_count> {
	static constexpr uint64_t byte_count_local{ byte_count / 4 };
	static constexpr uint64_t local_x1{ byte_count_local };
	static constexpr uint64_t local_x2{ local_x1 + byte_count_local };
	static constexpr uint64_t local_x3{ local_x2 + byte_count_local };
	static constexpr uint64_t local_x4{ local_x3 + byte_count_local };

	BNCH_SWT_HOST static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count_rt) noexcept {
		pow2MemcpyWrapper<byte_count_local>(dst, src);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x1, src + local_x1);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x2, src + local_x2);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x3, src + local_x3);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x4, src + byte_count_rt - local_x4);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x3, src + byte_count_rt - local_x3);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x2, src + byte_count_rt - local_x2);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x1, src + byte_count_rt - local_x1);
	}
};

template<uint64_t alignment, uint64_t byte_count>
	requires(alignment == (byte_count / 8))
struct memcpy_runner<alignment, byte_count> {
	static constexpr uint64_t byte_count_local{ byte_count / 8 };
	static constexpr uint64_t local_x1{ byte_count_local };
	static constexpr uint64_t local_x2{ local_x1 + byte_count_local };
	static constexpr uint64_t local_x3{ local_x2 + byte_count_local };
	static constexpr uint64_t local_x4{ local_x3 + byte_count_local };
	static constexpr uint64_t local_x5{ local_x4 + byte_count_local };
	static constexpr uint64_t local_x6{ local_x5 + byte_count_local };
	static constexpr uint64_t local_x7{ local_x6 + byte_count_local };
	static constexpr uint64_t local_x8{ local_x7 + byte_count_local };

	BNCH_SWT_HOST static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count_rt) noexcept {
		pow2MemcpyWrapper<byte_count_local>(dst, src);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x1, src + local_x1);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x2, src + local_x2);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x3, src + local_x3);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x4, src + local_x4);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x5, src + local_x5);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x6, src + local_x6);
		pow2MemcpyWrapper<byte_count_local>(dst + local_x7, src + local_x7);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x8, src + byte_count_rt - local_x8);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x7, src + byte_count_rt - local_x7);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x6, src + byte_count_rt - local_x6);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x5, src + byte_count_rt - local_x5);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x4, src + byte_count_rt - local_x4);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x3, src + byte_count_rt - local_x3);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x2, src + byte_count_rt - local_x2);
		pow2MemcpyWrapper<byte_count_local>(dst + byte_count_rt - local_x1, src + byte_count_rt - local_x1);
	}
};

template<uint64_t max_bytes> BNCH_SWT_HOST void jsonifierMemcpyUpTo(void* __restrict dst, const void* __restrict src, uint64_t byte_count) noexcept {
	switch (std::bit_width(byte_count)) {
		case 0: {
			return;
		}
		case 1: {
			if constexpr (max_bytes >= 1) {
				*static_cast<std::byte* __restrict>(dst) = *static_cast<const std::byte* __restrict>(src);
			}
			return;
		}
		case 2: {
			if constexpr (max_bytes >= 2) {
				memcpy_runner<alignment, 2ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 3: {
			if constexpr (max_bytes >= 4) {
				memcpy_runner<alignment, 4ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 4: {
			if constexpr (max_bytes >= 8) {
				memcpy_runner<alignment, 8ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 5: {
			if constexpr (max_bytes >= 16) {
				memcpy_runner<alignment, 16ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 6: {
			if constexpr (max_bytes >= 32) {
				memcpy_runner<alignment, 32ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 7: {
			if constexpr (max_bytes >= 64) {
				memcpy_runner<alignment, 64ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 8: {
			if constexpr (max_bytes >= 128) {
				memcpy_runner<alignment, 128ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		default: {
			if constexpr (max_bytes >= 256) {
				memcpyWrapper(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
	}
}

BNCH_SWT_HOST void jsonifierMemcpy(void* __restrict dst, const void* __restrict src, uint64_t byte_count) noexcept {
	switch (std::bit_width(byte_count)) {
		case 0: {
			return;
		}
		case 1: {
			*static_cast<std::byte* __restrict>(dst) = *static_cast<const std::byte* __restrict>(src);
			return;
		}
		case 2: {
			memcpy_runner<alignment, 2ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		case 3: {
			memcpy_runner<alignment, 4ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		case 4: {
			memcpy_runner<alignment, 8ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		case 5: {
			memcpy_runner<alignment, 16ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		case 6: {
			memcpy_runner<alignment, 32ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		case 7: {
			memcpy_runner<alignment, 64ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		case 8: {
			memcpy_runner<alignment, 128ULL>::impl(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
		default: {
			memcpyWrapper(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			return;
		}
	}
}

struct std_copier {
	BNCH_SWT_HOST static void copy(char* dst, const char* src, uint64_t byte_count) {
		std::memcpy(dst, src, byte_count);
	}
};

template<uint64_t max_bytes> struct jsonifier_copier {
	BNCH_SWT_HOST static void copy(char* dst, const char* src, uint64_t byte_count) {
		jsonifierMemcpyUpTo<max_bytes>(dst, src, byte_count);
	}
};

struct jsonifier_runtime_copier {
	BNCH_SWT_HOST static void copy(char* dst, const char* src, uint64_t byte_count) {
		jsonifierMemcpy(dst, src, byte_count);
	}
};

static constexpr uint64_t length_table_size{ 1 << 16 };
static constexpr uint64_t length_table_mask{ length_table_size - 1 };
static uint64_t sequence_base{};

template<uint64_t min_length, uint64_t max_length> static constexpr std::array<uint16_t, length_table_size> make_length_table() {
	std::array<uint16_t, length_table_size> table{};
	uint64_t state{ 0x9E3779B97F4A7C15ull };
	for (uint64_t i{}; i < length_table_size; ++i) {
		state	 = state * 6364136223846793005ull + 1442695040888963407ull;
		table[i] = static_cast<uint16_t>(min_length + (state >> 33) % (max_length - min_length + 1));
	}
	return table;
}

template<uint64_t min_length, uint64_t max_length> static constexpr std::array<uint16_t, length_table_size> length_table{ make_length_table<min_length, max_length>() };

BNCH_SWT_HOST uint64_t copy_offset(uint64_t i) {
	return (i * offset_stride) & buffer_mask & offset_alignment_mask;
}

BNCH_SWT_HOST uint64_t source_jitter(uint64_t i) {
	return (i * 5) & 63;
}

BNCH_SWT_HOST uint64_t destination_jitter(uint64_t i) {
	return (i * 11 + 3) & 63;
}

template<typename copier_type, uint64_t min_length, uint64_t max_length> struct random_length_copy {
	static uint64_t impl(char* dst, const char* src) {
		uint64_t total{};
		sequence_base += 12347;
		const uint64_t base = sequence_base;
		for (uint64_t i{}; i < copies_per_iteration; ++i) {
			const uint64_t offset = copy_offset(i);
			const uint64_t size	  = length_table<min_length, max_length>[(base + i) & length_table_mask] + runtime_size_source;
			copier_type::copy(dst + offset + destination_jitter(i), src + offset + source_jitter(i), size);
			total += size;
		}
		benchmarksuite::do_not_optimize_away(dst);
		return total;
	}
};

template<typename copier_type, uint64_t max_verified_bytes> static bool verify_copier() {
	static constexpr uint64_t guard_bytes{ 64 };
	std::vector<char> src(max_verified_bytes + 2 * guard_bytes);
	for (uint64_t i{}; i < src.size(); ++i) {
		src[i] = static_cast<char>(i * 31 + 7);
	}
	for (uint64_t byte_count{}; byte_count <= max_verified_bytes; ++byte_count) {
		std::vector<char> dst(src.size(), static_cast<char>(0x5A));
		copier_type::copy(dst.data() + guard_bytes, src.data() + guard_bytes, byte_count);
		for (uint64_t i{}; i < dst.size(); ++i) {
			const bool inside_copy = i >= guard_bytes && i < guard_bytes + byte_count;
			const char expected	   = inside_copy ? src[i] : static_cast<char>(0x5A);
			if (dst[i] != expected) {
				std::cout << "copier mismatch for max " << max_verified_bytes << " at length " << byte_count << " byte " << i << std::endl;
				return false;
			}
		}
	}
	return true;
}

using stage_type_runtime = benchmarksuite::benchmark_stage<"jsonifier_memcpy_buckets",
	benchmarksuite::
		stage_config_data{ .include_non_converged_results = true, .max_iteration_count = 10000000, .convergence_threshold = 5.0, .max_time_in_s = 1, .rse_threshold = 2.5 }>;

using stage_type_up_to = benchmarksuite::benchmark_stage<"jsonifier_memcpy_up_to_buckets",
	benchmarksuite::
		stage_config_data{ .include_non_converged_results = true, .max_iteration_count = 10000000, .convergence_threshold = 5.0, .max_time_in_s = 1, .rse_threshold = 2.5 }>;

static void run_runtime_lengths(char* dst, const char* src) {
	stage_type_runtime::run_benchmark<"[lengths-2-3]", "std-memcpy", random_length_copy<std_copier, 2, 3>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-2-3]", "jsonifier-memcpy", random_length_copy<jsonifier_runtime_copier, 2, 3>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-4-7]", "std-memcpy", random_length_copy<std_copier, 4, 7>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-4-7]", "jsonifier-memcpy", random_length_copy<jsonifier_runtime_copier, 4, 7>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-8-15]", "std-memcpy", random_length_copy<std_copier, 8, 15>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-8-15]", "jsonifier-memcpy", random_length_copy<jsonifier_runtime_copier, 8, 15>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-16-31]", "std-memcpy", random_length_copy<std_copier, 16, 31>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-16-31]", "jsonifier-memcpy", random_length_copy<jsonifier_runtime_copier, 16, 31>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-32-63]", "std-memcpy", random_length_copy<std_copier, 32, 63>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-32-63]", "jsonifier-memcpy", random_length_copy<jsonifier_runtime_copier, 32, 63>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-1-63]", "std-memcpy", random_length_copy<std_copier, 1, 63>>(dst, src);
	stage_type_runtime::run_benchmark<"[lengths-1-63]", "jsonifier-memcpy", random_length_copy<jsonifier_runtime_copier, 1, 63>>(dst, src);
}

static void run_up_to_lengths(char* dst, const char* src) {
	stage_type_up_to::run_benchmark<"[lengths-4-7]", "std-memcpy", random_length_copy<std_copier, 4, 7>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-4-7]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<7>, 4, 7>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-8-15]", "std-memcpy", random_length_copy<std_copier, 8, 15>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-8-15]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<15>, 8, 15>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-16-31]", "std-memcpy", random_length_copy<std_copier, 16, 31>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-16-31]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<31>, 16, 31>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-31]", "std-memcpy", random_length_copy<std_copier, 1, 31>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-31]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<31>, 1, 31>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-32]", "std-memcpy", random_length_copy<std_copier, 1, 32>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-32]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<32>, 1, 32>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-63]", "std-memcpy", random_length_copy<std_copier, 1, 63>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-63]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<63>, 1, 63>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-64]", "std-memcpy", random_length_copy<std_copier, 1, 64>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-64]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<64>, 1, 64>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-127]", "std-memcpy", random_length_copy<std_copier, 1, 127>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-127]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<127>, 1, 127>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-128]", "std-memcpy", random_length_copy<std_copier, 1, 128>>(dst, src);
	stage_type_up_to::run_benchmark<"[lengths-1-128]", "jsonifier-memcpy-up-to", random_length_copy<jsonifier_copier<128>, 1, 128>>(dst, src);
}

template<uint64_t... max_lengths> static bool verify_all_copiers() {
	return (verify_copier<jsonifier_copier<max_lengths>, max_lengths>() && ...);
}

int32_t main() {
	if (!verify_all_copiers<1, 3, 4, 7, 8, 15, 16, 31, 32, 64, 128>() || !verify_copier<jsonifier_runtime_copier, 600>()) {
		return 1;
	}
	benchmarksuite::pin_for_benchmark();
	std::vector<char> source_buffer(buffer_bytes + 2048, 'a');
	std::vector<char> destination_buffer(buffer_bytes + 2048, 'b');
	char* dst		= destination_buffer.data();
	const char* src = source_buffer.data();
	run_runtime_lengths(dst, src);
	auto finished_tests = stage_type_runtime::get_finished_tests();
	for (auto& value: finished_tests) {
		std::cout << value.to_markdown() << std::endl;
	}
	std::cout << stage_type_runtime::get_all_results().to_csv() << std::endl;
	run_up_to_lengths(dst, src);
	finished_tests = stage_type_up_to::get_finished_tests();
	for (auto& value: finished_tests) {
		std::cout << value.to_markdown() << std::endl;
	}
	std::cout << stage_type_up_to::get_all_results().to_csv() << std::endl;
	return 0;
}
