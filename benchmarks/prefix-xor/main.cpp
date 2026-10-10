#include <benchmarksuite>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#if defined(__aarch64__) || defined(_M_ARM64)
	#include <arm_neon.h>
	#define PREFIX_XOR_NEON 1
#else
	#include <immintrin.h>
	#define PREFIX_XOR_NEON 0
#endif

#if defined(_MSC_VER) && !defined(__clang__)
	#define PREFIX_XOR_INLINE __forceinline
#else
	#define PREFIX_XOR_INLINE inline __attribute__((always_inline))
#endif

struct prefix_xor_clmul_op {
	PREFIX_XOR_INLINE static uint64_t impl(uint64_t bitmask) noexcept {
#if PREFIX_XOR_NEON
		return vgetq_lane_u64(vreinterpretq_u64_p128(vmull_p64(static_cast<poly64_t>(bitmask), static_cast<poly64_t>(~0ull))), 0);
#else
		const __m128i all_ones = _mm_set1_epi8(static_cast<char>(-1));
		const __m128i result   = _mm_clmulepi64_si128(_mm_set_epi64x(0, static_cast<int64_t>(bitmask)), all_ones, 0);
		return static_cast<uint64_t>(_mm_cvtsi128_si64(result));
#endif
	}
};

struct prefix_xor_shift_op {
	PREFIX_XOR_INLINE static uint64_t impl(uint64_t bitmask) noexcept {
		bitmask ^= bitmask << 1;
		bitmask ^= bitmask << 2;
		bitmask ^= bitmask << 4;
		bitmask ^= bitmask << 8;
		bitmask ^= bitmask << 16;
		bitmask ^= bitmask << 32;
		return bitmask;
	}
};

template<typename op_type> struct throughput_run {
	static uint64_t impl(const std::vector<uint64_t>& input, std::vector<uint64_t>& output) noexcept {
		const uint64_t* in = input.data();
		uint64_t* out	   = output.data();
		const uint64_t count = input.size();
		for (uint64_t x = 0; x < count; ++x) {
			out[x] = op_type::impl(in[x]);
		}
		benchmarksuite::do_not_optimize_away(out);
		return count * sizeof(uint64_t);
	}
};

template<typename op_type> struct chained_run {
	static uint64_t impl(const std::vector<uint64_t>& input, std::vector<uint64_t>& output) noexcept {
		const uint64_t* in = input.data();
		uint64_t* out	   = output.data();
		const uint64_t count = input.size();
		uint64_t prev_in_string{};
		for (uint64_t x = 0; x < count; ++x) {
			const uint64_t in_string = op_type::impl(in[x]) ^ prev_in_string;
			prev_in_string			 = static_cast<uint64_t>(static_cast<int64_t>(in_string) >> 63);
			out[x]					 = in_string;
		}
		benchmarksuite::do_not_optimize_away(out);
		return count * sizeof(uint64_t);
	}
};

enum class bit_patterns {
	uniform,
	sparse,
	quote_like,
	dense,
	all_zero,
	all_ones,
	alternating,
	single_bit,
};

template<bit_patterns pattern> struct pattern_traits;

template<> struct pattern_traits<bit_patterns::uniform> {
	static constexpr benchmarksuite::string_literal name{ "uniform" };
	static uint64_t impl(std::mt19937_64& rng) noexcept {
		return rng();
	}
};

template<> struct pattern_traits<bit_patterns::sparse> {
	static constexpr benchmarksuite::string_literal name{ "sparse-1in64" };
	static uint64_t impl(std::mt19937_64& rng) noexcept {
		return rng() & rng() & rng() & rng() & rng() & rng();
	}
};

template<> struct pattern_traits<bit_patterns::quote_like> {
	static constexpr benchmarksuite::string_literal name{ "quote-like-1in8" };
	static uint64_t impl(std::mt19937_64& rng) noexcept {
		return rng() & rng() & rng();
	}
};

template<> struct pattern_traits<bit_patterns::dense> {
	static constexpr benchmarksuite::string_literal name{ "dense-63in64" };
	static uint64_t impl(std::mt19937_64& rng) noexcept {
		return rng() | rng() | rng() | rng() | rng() | rng();
	}
};

template<> struct pattern_traits<bit_patterns::all_zero> {
	static constexpr benchmarksuite::string_literal name{ "all-zero" };
	static uint64_t impl(std::mt19937_64&) noexcept {
		return 0;
	}
};

template<> struct pattern_traits<bit_patterns::all_ones> {
	static constexpr benchmarksuite::string_literal name{ "all-ones" };
	static uint64_t impl(std::mt19937_64&) noexcept {
		return ~0ull;
	}
};

template<> struct pattern_traits<bit_patterns::alternating> {
	static constexpr benchmarksuite::string_literal name{ "alternating" };
	static uint64_t impl(std::mt19937_64& rng) noexcept {
		return (rng() & 1) ? 0xAAAAAAAAAAAAAAAAull : 0x5555555555555555ull;
	}
};

template<> struct pattern_traits<bit_patterns::single_bit> {
	static constexpr benchmarksuite::string_literal name{ "single-bit" };
	static uint64_t impl(std::mt19937_64& rng) noexcept {
		return 1ull << (rng() & 63);
	}
};

static constexpr benchmarksuite::stage_config_data config{
	.measured_iteration_count			= 100,
	.max_iteration_count				= 10000,
	.convergence_threshold				= 1.0,
	.max_time_in_s						= 5,
	.rse_threshold						= 2.5,
	.max_k								= 100000,
	.min_k								= 30,
	.clear_cpu_caches_before_iterations = false,
	.include_non_converged_results		= true,
	.benchmark_type						= benchmarksuite::benchmark_types::cpu,
};

using prefix_xor_stage = benchmarksuite::benchmark_stage<"prefix-xor", config>;

template<bit_patterns pattern, uint64_t mask_count> struct run_case {
	static void impl() {
		using traits = pattern_traits<pattern>;
		std::mt19937_64 rng{ 0x5EEDull + static_cast<uint64_t>(pattern) * 1000003ull + mask_count };
		std::vector<uint64_t> input(mask_count);
		std::vector<uint64_t> output(mask_count);
		for (auto& value: input) {
			value = traits::impl(rng);
		}
		std::vector<uint64_t> check_clmul(mask_count);
		std::vector<uint64_t> check_shift(mask_count);
		chained_run<prefix_xor_clmul_op>::impl(input, check_clmul);
		chained_run<prefix_xor_shift_op>::impl(input, check_shift);
		if (check_clmul != check_shift) {
			std::cerr << "MISMATCH: " << traits::name << " x" << mask_count << std::endl;
			std::exit(1);
		}
		static constexpr auto size_name = benchmarksuite::internal::to_string_literal<static_cast<int64_t>(mask_count)>();
		static constexpr auto base_name = traits::name + benchmarksuite::string_literal{ "-x" } + size_name;
		static constexpr auto tp_name	= base_name + benchmarksuite::string_literal{ "-throughput" };
		static constexpr auto ch_name	= base_name + benchmarksuite::string_literal{ "-chained" };
		prefix_xor_stage::run_benchmark<tp_name, "clmul", throughput_run<prefix_xor_clmul_op>>(input, output);
		prefix_xor_stage::run_benchmark<tp_name, "shift-xor", throughput_run<prefix_xor_shift_op>>(input, output);
		prefix_xor_stage::run_benchmark<ch_name, "clmul", chained_run<prefix_xor_clmul_op>>(input, output);
		prefix_xor_stage::run_benchmark<ch_name, "shift-xor", chained_run<prefix_xor_shift_op>>(input, output);
	}
};

template<bit_patterns pattern> struct run_pattern {
	static void impl() {
		run_case<pattern, 16>::impl();
		run_case<pattern, 256>::impl();
		run_case<pattern, 4096>::impl();
		run_case<pattern, 65536>::impl();
		run_case<pattern, 1048576>::impl();
	}
};

int32_t main() {
	benchmarksuite::pin_for_benchmark();
	run_pattern<bit_patterns::uniform>::impl();
	run_pattern<bit_patterns::sparse>::impl();
	run_pattern<bit_patterns::quote_like>::impl();
	run_pattern<bit_patterns::dense>::impl();
	run_pattern<bit_patterns::all_zero>::impl();
	run_pattern<bit_patterns::all_ones>::impl();
	run_pattern<bit_patterns::alternating>::impl();
	run_pattern<bit_patterns::single_bit>::impl();
	for (const auto& test: prefix_xor_stage::get_finished_tests()) {
		std::cout << test.to_markdown();
	}
	return 0;
}
