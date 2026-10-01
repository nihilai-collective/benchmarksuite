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

template<uint64_t byte_count> BNCH_SWT_HOST void pow2MemcpyWrapper(std::byte* __restrict dst, const std::byte* __restrict src) {
	std::memcpy(dst, src, byte_count);
}

BNCH_SWT_HOST void memcpyWrapper(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count) {
	std::memcpy(dst, src, byte_count);
}

template<uint64_t chunk_bytes> BNCH_SWT_HOST void copyOverlappingChunks(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count) {
	pow2MemcpyWrapper<chunk_bytes>(dst, src);
	pow2MemcpyWrapper<chunk_bytes>(dst + byte_count - chunk_bytes, src + byte_count - chunk_bytes);
}

BNCH_SWT_HOST static void copyDecomposed(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count) {
	uint64_t offset{};
	if ((byte_count >> 7) & 1ull) {
		pow2MemcpyWrapper<1ull << 7>(dst + offset, src + offset);
		offset += 1ull << 7;
	}
	if ((byte_count >> 6) & 1ull) {
		pow2MemcpyWrapper<1ull << 6>(dst + offset, src + offset);
		offset += 1ull << 6;
	}
	if ((byte_count >> 5) & 1ull) {
		pow2MemcpyWrapper<1ull << 5>(dst + offset, src + offset);
		offset += 1ull << 5;
	}
	if ((byte_count >> 4) & 1ull) {
		pow2MemcpyWrapper<1ull << 4>(dst + offset, src + offset);
		offset += 1ull << 4;
	}
	if ((byte_count >> 3) & 1ull) {
		pow2MemcpyWrapper<1ull << 3>(dst + offset, src + offset);
		offset += 1ull << 3;
	}
	if ((byte_count >> 2) & 1ull) {
		pow2MemcpyWrapper<1ull << 2>(dst + offset, src + offset);
		offset += 1ull << 2;
	}
	if ((byte_count >> 1) & 1ull) {
		pow2MemcpyWrapper<1ull << 1>(dst + offset, src + offset);
		offset += 1ull << 1;
	}
	if ((byte_count >> 0) & 1ull) {
		pow2MemcpyWrapper<1ull << 0>(dst + offset, src + offset);
		offset += 1ull << 0;
	}
}

template<uint64_t max_bytes> BNCH_SWT_HOST void jsonifierMemcpyUpTo(void* __restrict dst, const void* __restrict src, uint64_t byte_count) {
	switch (std::bit_width(byte_count)) {
		case 0: {
			return;
		}
		case 1: {
			*static_cast<std::byte* __restrict>(dst) = *static_cast<const std::byte* __restrict>(src);
			return;
		}
		case 2: {
			if constexpr (max_bytes >= 2) {
				copyOverlappingChunks<2>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 3: {
			if constexpr (max_bytes >= 4) {
				copyOverlappingChunks<4>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 4: {
			if constexpr (max_bytes >= 8) {
				copyOverlappingChunks<8>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 5: {
			if constexpr (max_bytes >= 16) {
				copyOverlappingChunks<16>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 6: {
			if constexpr (max_bytes >= 32) {
				copyOverlappingChunks<32>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 7: {
			if constexpr (max_bytes >= 64) {
				copyOverlappingChunks<64>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 8: {
			if constexpr (max_bytes >= 128) {
				copyOverlappingChunks<128>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
			}
			return;
		}
		case 9: {
			if constexpr (max_bytes >= 256) {
				if (byte_count == 256) {
					copyOverlappingChunks<128>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
				} else {
					memcpyWrapper(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byte_count);
				}
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

BNCH_SWT_HOST void jsonifierMemcpy(void* __restrict destination, const void* __restrict source, uint64_t byte_count) {
	jsonifierMemcpyUpTo<std::numeric_limits<uint64_t>::max()>(destination, source, byte_count);
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

template<uint64_t max_length> static constexpr std::array<uint8_t, length_table_size> make_length_table() {
	std::array<uint8_t, length_table_size> table{};
	uint64_t state{ 0x9E3779B97F4A7C15ull };
	for (uint64_t i{}; i < length_table_size; ++i) {
		state	 = state * 6364136223846793005ull + 1442695040888963407ull;
		table[i] = static_cast<uint8_t>((state >> 33) % (max_length + 1));
	}
	return table;
}

template<uint64_t max_length> static constexpr std::array<uint8_t, length_table_size> length_table{ make_length_table<max_length>() };

BNCH_SWT_HOST uint64_t copy_offset(uint64_t i) {
	return (i * offset_stride) & buffer_mask & offset_alignment_mask;
}

BNCH_SWT_HOST uint64_t source_jitter(uint64_t i) {
	return (i * 5) & 63;
}

BNCH_SWT_HOST uint64_t destination_jitter(uint64_t i) {
	return (i * 11 + 3) & 63;
}

template<typename copier_type, uint64_t max_length> struct random_length_copy {
	static uint64_t impl(char* dst, const char* src) {
		uint64_t total{};
		sequence_base += 12347;
		const uint64_t base = sequence_base;
		for (uint64_t i{}; i < copies_per_iteration; ++i) {
			const uint64_t offset = copy_offset(i);
			const uint64_t size	  = length_table<max_length>[(base + i) & length_table_mask] + runtime_size_source;
			copier_type::copy(dst + offset + destination_jitter(i), src + offset + source_jitter(i), size);
			total += size;
		}
		benchmarksuite::do_not_optimize_away(dst);
		return total;
	}
};

template<typename copier_type, uint64_t max_verified_bytes> static bool verify_copier() {
	static constexpr uint64_t guard_bytes{ 64 };
	std::vector<char> source(max_verified_bytes + 2 * guard_bytes);
	for (uint64_t i{}; i < source.size(); ++i) {
		source[i] = static_cast<char>(i * 31 + 7);
	}
	for (uint64_t byte_count{}; byte_count <= max_verified_bytes; ++byte_count) {
		std::vector<char> destination(source.size(), static_cast<char>(0x5A));
		copier_type::copy(destination.data() + guard_bytes, source.data() + guard_bytes, byte_count);
		for (uint64_t i{}; i < destination.size(); ++i) {
			const bool inside_copy = i >= guard_bytes && i < guard_bytes + byte_count;
			const char expected	   = inside_copy ? source[i] : static_cast<char>(0x5A);
			if (destination[i] != expected) {
				std::cout << "copier mismatch for max " << max_verified_bytes << " at length " << byte_count << " byte " << i << std::endl;
				return false;
			}
		}
	}
	return true;
}

using stage_type =
	benchmarksuite::benchmark_stage<"jsonifier_memcpy", benchmarksuite::stage_config_data{ .convergence_threshold = 10.0, .max_time_in_s = 10, .rse_threshold = 15.0 }>;

template<uint64_t max_length, benchmarksuite::string_literal test_name> void run_random_lengths(char* dst, const char* src) {
	stage_type::run_benchmark<test_name, "std-memcpy-" + test_name, random_length_copy<std_copier, max_length>>(dst, src);
	stage_type::run_benchmark<test_name, "jsonifier-memcpy-up-to-" + test_name, random_length_copy<jsonifier_copier<max_length>, max_length>>(dst, src);
}

template<uint64_t max_length, benchmarksuite::string_literal test_name> void run_runtime_random_lengths(char* dst, const char* src) {
	stage_type::run_benchmark<test_name, "std-memcpy-" + test_name, random_length_copy<std_copier, max_length>>(dst, src);
	stage_type::run_benchmark<test_name, "jsonifier-memcpy-" + test_name, random_length_copy<jsonifier_runtime_copier, max_length>>(dst, src);
}

template<uint64_t... max_lengths> static bool verify_all_copiers() {
	return (verify_copier<jsonifier_copier<max_lengths>, max_lengths>() && ...);
}

int32_t main() {
	if (!verify_all_copiers<4, 8, 16, 32, 64, 128>() || !verify_copier<jsonifier_runtime_copier, 600>()) {
		return 1;
	}
	benchmarksuite::pin_for_benchmark();
	std::vector<char> source_buffer(buffer_bytes + 2048, 'a');
	std::vector<char> destination_buffer(buffer_bytes + 2048, 'b');
	char* dst		= destination_buffer.data();
	const char* src = source_buffer.data();
	run_random_lengths<4, "[lengths-0-4]">(dst, src);
	run_random_lengths<8, "[lengths-0-8]">(dst, src);
	run_random_lengths<16, "[lengths-0-16]">(dst, src);
	run_random_lengths<32, "[lengths-0-32]">(dst, src);
	run_random_lengths<64, "[lengths-0-64]">(dst, src);
	run_random_lengths<128, "[lengths-0-128]">(dst, src);
	run_runtime_random_lengths<3, "[random-runtime-lengths-0-3]">(dst, src);
	run_runtime_random_lengths<5, "[random-runtime-lengths-0-5]">(dst, src);
	run_runtime_random_lengths<7, "[random-runtime-lengths-0-7]">(dst, src);
	run_runtime_random_lengths<9, "[random-runtime-lengths-0-9]">(dst, src);
	run_runtime_random_lengths<11, "[random-runtime-lengths-0-11]">(dst, src);
	run_runtime_random_lengths<13, "[random-runtime-lengths-0-13]">(dst, src);
	run_runtime_random_lengths<15, "[random-runtime-lengths-0-15]">(dst, src);
	run_runtime_random_lengths<17, "[random-runtime-lengths-0-17]">(dst, src);
	run_runtime_random_lengths<19, "[random-runtime-lengths-0-19]">(dst, src);
	run_runtime_random_lengths<21, "[random-runtime-lengths-0-21]">(dst, src);
	run_runtime_random_lengths<23, "[random-runtime-lengths-0-23]">(dst, src);
	run_runtime_random_lengths<25, "[random-runtime-lengths-0-25]">(dst, src);
	run_runtime_random_lengths<27, "[random-runtime-lengths-0-27]">(dst, src);
	run_runtime_random_lengths<29, "[random-runtime-lengths-0-29]">(dst, src);
	run_runtime_random_lengths<31, "[random-runtime-lengths-0-31]">(dst, src);
	run_runtime_random_lengths<33, "[random-runtime-lengths-0-33]">(dst, src);
	run_runtime_random_lengths<35, "[random-runtime-lengths-0-35]">(dst, src);
	run_runtime_random_lengths<37, "[random-runtime-lengths-0-37]">(dst, src);
	run_runtime_random_lengths<39, "[random-runtime-lengths-0-39]">(dst, src);
	run_runtime_random_lengths<41, "[random-runtime-lengths-0-41]">(dst, src);
	run_runtime_random_lengths<43, "[random-runtime-lengths-0-43]">(dst, src);
	run_runtime_random_lengths<45, "[random-runtime-lengths-0-45]">(dst, src);
	run_runtime_random_lengths<47, "[random-runtime-lengths-0-47]">(dst, src);
	run_runtime_random_lengths<49, "[random-runtime-lengths-0-49]">(dst, src);
	run_runtime_random_lengths<51, "[random-runtime-lengths-0-51]">(dst, src);
	run_runtime_random_lengths<53, "[random-runtime-lengths-0-53]">(dst, src);
	run_runtime_random_lengths<55, "[random-runtime-lengths-0-55]">(dst, src);
	run_runtime_random_lengths<65, "[random-runtime-lengths-0-65]">(dst, src);
	run_runtime_random_lengths<97, "[random-runtime-lengths-0-97]">(dst, src);
	run_runtime_random_lengths<129, "[random-runtime-lengths-0-129]">(dst, src);
	auto finished_tests = stage_type::get_finished_tests();
	for (auto& value: finished_tests) {
		std::cout << value.to_markdown() << std::endl;
	}
	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return 0;
}
