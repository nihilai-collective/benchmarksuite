/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/main.cpp
 */

#include <benchmarksuite>
#include <bit>
#include <random>

#if BNCH_SWT_ARCH_X64 && !BNCH_SWT_COMPILER_MSVC
	#include <immintrin.h>
#endif

inline constexpr uint64_t value_count{ 4096 };

enum class dataset_kind : uint64_t {
	dense,
	sparse,
	sparse_with_zeros,
};

template<typename value_type, dataset_kind kind> inline std::array<value_type, value_count> test_values{};

template<typename value_type, dataset_kind kind> struct dataset_filler {
	BNCH_SWT_HOST static void impl() {
		std::mt19937_64 engine{ 0x5eed0000ULL + sizeof(value_type) + static_cast<uint64_t>(kind) };
		for (value_type& value: test_values<value_type, kind>) {
			const uint64_t random_bits = engine();
			if (kind == dataset_kind::dense) {
				const value_type truncated = static_cast<value_type>(random_bits);
				value					   = truncated == 0 ? value_type{ 1 } : truncated;
			} else if (kind == dataset_kind::sparse_with_zeros && (random_bits & 3ULL) == 0) {
				value = value_type{ 0 };
			} else {
				const uint64_t first_bit  = (random_bits >> 8) % (sizeof(value_type) * 8);
				const uint64_t second_bit = (random_bits >> 24) % (sizeof(value_type) * 8);
				value					  = static_cast<value_type>((1ULL << first_bit) | (1ULL << second_bit));
			}
		}
	}
};

struct countl_zero_op {
	BNCH_SWT_HOST static uint64_t existing(uint16_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return value == 0 ? 16ULL : static_cast<uint64_t>(__lzcnt16(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountLeadingZeros16(value));
#elif BNCH_SWT_ARCH_ARM64 || defined(__LZCNT__)
		return value == 0 ? 16ULL : static_cast<uint64_t>(__builtin_clz(static_cast<uint32_t>(value)) - 16);
#else
		return static_cast<uint64_t>(std::countl_zero(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint32_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(__lzcnt(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountLeadingZeros(value));
#elif BNCH_SWT_ARCH_ARM64
		return value == 0 ? 32ULL : static_cast<uint64_t>(__builtin_clz(value));
#elif defined(__LZCNT__)
		return static_cast<uint64_t>(_lzcnt_u32(value));
#else
		return static_cast<uint64_t>(std::countl_zero(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint64_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(__lzcnt64(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountLeadingZeros64(value));
#elif BNCH_SWT_ARCH_ARM64
		return value == 0 ? 64ULL : static_cast<uint64_t>(__builtin_clzll(value));
#elif defined(__LZCNT__)
		return static_cast<uint64_t>(_lzcnt_u64(value));
#else
		return static_cast<uint64_t>(std::countl_zero(value));
#endif
	}

	template<typename value_type> BNCH_SWT_HOST static uint64_t standard(value_type value) {
		return static_cast<uint64_t>(std::countl_zero(value));
	}
};

struct countr_zero_op {
	BNCH_SWT_HOST static uint64_t existing(uint16_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return value == 0 ? 16ULL : static_cast<uint64_t>(_tzcnt_u32(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountTrailingZeros16(value));
#elif BNCH_SWT_ARCH_ARM64 || defined(__BMI__)
		return value == 0 ? 16ULL : static_cast<uint64_t>(__builtin_ctz(static_cast<uint32_t>(value)));
#else
		return static_cast<uint64_t>(std::countr_zero(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint32_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(_tzcnt_u32(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountTrailingZeros(value));
#elif BNCH_SWT_ARCH_ARM64
		return value == 0 ? 32ULL : static_cast<uint64_t>(__builtin_ctz(value));
#elif defined(__BMI__)
		return static_cast<uint64_t>(_tzcnt_u32(value));
#else
		return static_cast<uint64_t>(std::countr_zero(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint64_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(_tzcnt_u64(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountTrailingZeros64(value));
#elif BNCH_SWT_ARCH_ARM64
		return value == 0 ? 64ULL : static_cast<uint64_t>(__builtin_ctzll(value));
#elif defined(__BMI__)
		return static_cast<uint64_t>(_tzcnt_u64(value));
#else
		return static_cast<uint64_t>(std::countr_zero(value));
#endif
	}

	template<typename value_type> BNCH_SWT_HOST static uint64_t standard(value_type value) {
		return static_cast<uint64_t>(std::countr_zero(value));
	}
};

struct countr_zero_unsafe_op {
	BNCH_SWT_HOST static uint64_t existing(uint16_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(_tzcnt_u32(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountTrailingZeros(value));
#elif BNCH_SWT_ARCH_ARM64 || defined(__BMI__)
		return static_cast<uint64_t>(__builtin_ctz(static_cast<uint32_t>(value)));
#else
		return static_cast<uint64_t>(std::countr_zero(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint32_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(_tzcnt_u32(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountTrailingZeros(value));
#elif BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(__builtin_ctz(value));
#elif defined(__BMI__)
		return static_cast<uint64_t>(_tzcnt_u32(value));
#else
		return static_cast<uint64_t>(std::countr_zero(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint64_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(_tzcnt_u64(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountTrailingZeros64(value));
#elif BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(__builtin_ctzll(value));
#elif defined(__BMI__)
		return static_cast<uint64_t>(_tzcnt_u64(value));
#else
		return static_cast<uint64_t>(std::countr_zero(value));
#endif
	}

	template<typename value_type> BNCH_SWT_HOST static uint64_t standard(value_type value) {
		return static_cast<uint64_t>(std::countr_zero(value));
	}
};

struct pop_count_op {
	BNCH_SWT_HOST static uint64_t existing(uint16_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(__popcnt16(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountOneBits(value));
#elif BNCH_SWT_ARCH_ARM64 || defined(__POPCNT__)
		return static_cast<uint64_t>(__builtin_popcount(static_cast<uint32_t>(value)));
#else
		return static_cast<uint64_t>(std::popcount(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint32_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(__popcnt(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountOneBits(value));
#elif BNCH_SWT_ARCH_ARM64 || defined(__POPCNT__)
		return static_cast<uint64_t>(__builtin_popcount(value));
#else
		return static_cast<uint64_t>(std::popcount(value));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint64_t value) {
#if BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64
		return static_cast<uint64_t>(__popcnt64(value));
#elif BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_ARM64
		return static_cast<uint64_t>(_CountOneBits64(value));
#elif BNCH_SWT_ARCH_ARM64 || defined(__POPCNT__)
		return static_cast<uint64_t>(__builtin_popcountll(value));
#else
		return static_cast<uint64_t>(std::popcount(value));
#endif
	}

	template<typename value_type> BNCH_SWT_HOST static uint64_t standard(value_type value) {
		return static_cast<uint64_t>(std::popcount(value));
	}
};

struct blsr_op {
	BNCH_SWT_HOST static uint64_t existing(uint32_t value) {
#if (BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64) || defined(__BMI__)
		return static_cast<uint64_t>(_blsr_u32(value));
#else
		return static_cast<uint64_t>(value & (value - 1));
#endif
	}

	BNCH_SWT_HOST static uint64_t existing(uint64_t value) {
#if (BNCH_SWT_COMPILER_MSVC && BNCH_SWT_ARCH_X64) || defined(__BMI__)
		return static_cast<uint64_t>(_blsr_u64(value));
#else
		return static_cast<uint64_t>(value & (value - 1));
#endif
	}

	template<typename value_type> BNCH_SWT_HOST static uint64_t standard(value_type value) {
		return static_cast<uint64_t>(value & (value - 1));
	}
};

struct mask_drain_op {
	BNCH_SWT_HOST static uint64_t existing(uint64_t value) {
		uint64_t sum{};
		while (value != 0) {
			sum += countr_zero_unsafe_op::existing(value);
			value &= value - 1;
		}
		return sum;
	}

	BNCH_SWT_HOST static uint64_t standard(uint64_t value) {
		uint64_t sum{};
		while (value != 0) {
			sum += countr_zero_unsafe_op::standard(value);
			value &= value - 1;
		}
		return sum;
	}
};

template<typename op_type, typename value_type, dataset_kind kind> struct existing_benchmark {
	BNCH_SWT_HOST static uint64_t impl() {
		uint64_t sum{};
		for (const value_type value: test_values<value_type, kind>) {
			sum += op_type::existing(value);
		}
		benchmarksuite::do_not_optimize_away(sum);
		return value_count * sizeof(value_type);
	}
};

template<typename op_type, typename value_type, dataset_kind kind> struct standard_benchmark {
	BNCH_SWT_HOST static uint64_t impl() {
		uint64_t sum{};
		for (const value_type value: test_values<value_type, kind>) {
			sum += op_type::standard(value);
		}
		benchmarksuite::do_not_optimize_away(sum);
		return value_count * sizeof(value_type);
	}
};

using stage_type = benchmarksuite::benchmark_stage<"bit-ops", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_time_in_s = 5 }>;

inline uint64_t mismatch_count{};

template<benchmarksuite::string_literal test_name, typename op_type, typename value_type, dataset_kind kind> struct test_runner {
	BNCH_SWT_HOST static void impl() {
		dataset_filler<value_type, kind>::impl();
		for (const value_type value: test_values<value_type, kind>) {
			if (op_type::existing(value) != op_type::standard(value)) {
				++mismatch_count;
			}
		}
		stage_type::template run_benchmark<test_name, "existing", existing_benchmark<op_type, value_type, kind>>();
		stage_type::template run_benchmark<test_name, "std", standard_benchmark<op_type, value_type, kind>>();
		std::cout << stage_type::get_test_results(test_name.operator std::string_view()).to_markdown() << std::endl;
	}
};

int32_t main() {
	benchmarksuite::pin_for_benchmark();

	test_runner<"countl-zero-u16-dense", countl_zero_op, uint16_t, dataset_kind::dense>::impl();
	test_runner<"countl-zero-u16-sparse", countl_zero_op, uint16_t, dataset_kind::sparse>::impl();
	test_runner<"countl-zero-u16-zeros", countl_zero_op, uint16_t, dataset_kind::sparse_with_zeros>::impl();
	test_runner<"countl-zero-u32-dense", countl_zero_op, uint32_t, dataset_kind::dense>::impl();
	test_runner<"countl-zero-u32-sparse", countl_zero_op, uint32_t, dataset_kind::sparse>::impl();
	test_runner<"countl-zero-u32-zeros", countl_zero_op, uint32_t, dataset_kind::sparse_with_zeros>::impl();
	test_runner<"countl-zero-u64-dense", countl_zero_op, uint64_t, dataset_kind::dense>::impl();
	test_runner<"countl-zero-u64-sparse", countl_zero_op, uint64_t, dataset_kind::sparse>::impl();
	test_runner<"countl-zero-u64-zeros", countl_zero_op, uint64_t, dataset_kind::sparse_with_zeros>::impl();

	test_runner<"countr-zero-u16-dense", countr_zero_op, uint16_t, dataset_kind::dense>::impl();
	test_runner<"countr-zero-u16-sparse", countr_zero_op, uint16_t, dataset_kind::sparse>::impl();
	test_runner<"countr-zero-u16-zeros", countr_zero_op, uint16_t, dataset_kind::sparse_with_zeros>::impl();
	test_runner<"countr-zero-u32-dense", countr_zero_op, uint32_t, dataset_kind::dense>::impl();
	test_runner<"countr-zero-u32-sparse", countr_zero_op, uint32_t, dataset_kind::sparse>::impl();
	test_runner<"countr-zero-u32-zeros", countr_zero_op, uint32_t, dataset_kind::sparse_with_zeros>::impl();
	test_runner<"countr-zero-u64-dense", countr_zero_op, uint64_t, dataset_kind::dense>::impl();
	test_runner<"countr-zero-u64-sparse", countr_zero_op, uint64_t, dataset_kind::sparse>::impl();
	test_runner<"countr-zero-u64-zeros", countr_zero_op, uint64_t, dataset_kind::sparse_with_zeros>::impl();

	test_runner<"countr-zero-unsafe-u16-dense", countr_zero_unsafe_op, uint16_t, dataset_kind::dense>::impl();
	test_runner<"countr-zero-unsafe-u16-sparse", countr_zero_unsafe_op, uint16_t, dataset_kind::sparse>::impl();
	test_runner<"countr-zero-unsafe-u32-dense", countr_zero_unsafe_op, uint32_t, dataset_kind::dense>::impl();
	test_runner<"countr-zero-unsafe-u32-sparse", countr_zero_unsafe_op, uint32_t, dataset_kind::sparse>::impl();
	test_runner<"countr-zero-unsafe-u64-dense", countr_zero_unsafe_op, uint64_t, dataset_kind::dense>::impl();
	test_runner<"countr-zero-unsafe-u64-sparse", countr_zero_unsafe_op, uint64_t, dataset_kind::sparse>::impl();

	test_runner<"pop-count-u16-dense", pop_count_op, uint16_t, dataset_kind::dense>::impl();
	test_runner<"pop-count-u16-sparse", pop_count_op, uint16_t, dataset_kind::sparse>::impl();
	test_runner<"pop-count-u32-dense", pop_count_op, uint32_t, dataset_kind::dense>::impl();
	test_runner<"pop-count-u32-sparse", pop_count_op, uint32_t, dataset_kind::sparse>::impl();
	test_runner<"pop-count-u64-dense", pop_count_op, uint64_t, dataset_kind::dense>::impl();
	test_runner<"pop-count-u64-sparse", pop_count_op, uint64_t, dataset_kind::sparse>::impl();

	test_runner<"blsr-u32-dense", blsr_op, uint32_t, dataset_kind::dense>::impl();
	test_runner<"blsr-u32-sparse", blsr_op, uint32_t, dataset_kind::sparse>::impl();
	test_runner<"blsr-u32-zeros", blsr_op, uint32_t, dataset_kind::sparse_with_zeros>::impl();
	test_runner<"blsr-u64-dense", blsr_op, uint64_t, dataset_kind::dense>::impl();
	test_runner<"blsr-u64-sparse", blsr_op, uint64_t, dataset_kind::sparse>::impl();
	test_runner<"blsr-u64-zeros", blsr_op, uint64_t, dataset_kind::sparse_with_zeros>::impl();

	test_runner<"mask-drain-u64-dense", mask_drain_op, uint64_t, dataset_kind::dense>::impl();
	test_runner<"mask-drain-u64-sparse", mask_drain_op, uint64_t, dataset_kind::sparse>::impl();

	std::cout << "Result mismatches between existing and std: " << mismatch_count << std::endl;
	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return mismatch_count == 0 ? 0 : 1;
}
