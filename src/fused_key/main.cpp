#include <benchmarksuite>

#include "fused_key_kernels.hpp"

#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__) && FUSED_KEY_TIER != 0
	#include <intrin.h>
#endif

#if FUSED_KEY_TIER == 0
using stage_type = benchmarksuite::benchmark_stage<"fused-key-compare-neon", benchmarksuite::stage_config_data{ .max_iteration_count = 20000 }>;
inline constexpr std::string_view tierName{ "NEON" };
#elif FUSED_KEY_TIER == 1
using stage_type = benchmarksuite::benchmark_stage<"fused-key-compare-avx2", benchmarksuite::stage_config_data{ .max_iteration_count = 20000 }>;
inline constexpr std::string_view tierName{ "AVX2" };
#else
using stage_type = benchmarksuite::benchmark_stage<"fused-key-compare-avx512", benchmarksuite::stage_config_data{ .max_iteration_count = 20000 }>;
inline constexpr std::string_view tierName{ "AVX512" };
#endif

namespace {

	struct corpus_storage {
		std::string buffer{};
		std::vector<uint32_t> offsets{};
		fused_key::key_corpus view{};
		uint64_t keyBytes{};
	};

	corpus_storage buildCorpus(std::string_view key, uint64_t keyCount, uint64_t seed) {
		corpus_storage corpus{};
		std::mt19937_64 rng{ seed };
		std::uniform_int_distribution<uint64_t> digitCount{ 1, 12 };
		std::uniform_int_distribution<uint32_t> digit{ 0, 9 };
		std::uniform_int_distribution<uint64_t> mismatch{ 0, 15 };
		corpus.buffer.reserve(keyCount * (key.size() + 16) + 64);
		corpus.offsets.reserve(keyCount);
		for (uint64_t i = 0; i < keyCount; ++i) {
			corpus.offsets.emplace_back(static_cast<uint32_t>(corpus.buffer.size()));
			std::string keyCopy{ key };
			if (mismatch(rng) == 0) {
				const uint64_t flipIndex{ 1 + (i % (keyCopy.size() - 3)) };
				keyCopy[flipIndex] = static_cast<char>(keyCopy[flipIndex] ^ 0x20);
			}
			corpus.buffer += keyCopy;
			const uint64_t digits{ digitCount(rng) };
			for (uint64_t d = 0; d < digits; ++d) {
				corpus.buffer.push_back(static_cast<char>('0' + digit(rng)));
			}
			corpus.buffer.push_back(',');
		}
		corpus.buffer.append(64, ' ');
		corpus.view	   = fused_key::key_corpus{ corpus.buffer.data(), corpus.offsets.data(), corpus.offsets.size() };
		corpus.keyBytes = keyCount * key.size();
		return corpus;
	}

	template<auto scan_function> struct scan_functor {
		BNCH_SWT_HOST static uint64_t impl(const corpus_storage& corpus) {
			uint64_t matched{ scan_function(corpus.view) };
			benchmarksuite::do_not_optimize_away(matched);
			return corpus.keyBytes;
		}
	};

	bool tierSupported() {
#if FUSED_KEY_TIER == 0
		return true;
#elif defined(_MSC_VER) && !defined(__clang__)
		int32_t registers[4]{};
		__cpuidex(registers, 7, 0);
		const uint32_t ebx{ static_cast<uint32_t>(registers[1]) };
		const uint32_t ecx{ static_cast<uint32_t>(registers[2]) };
		const bool avx2Tier{ (ebx & (1u << 5)) != 0 && (ebx & (1u << 8)) != 0 };
	#if FUSED_KEY_TIER == 1
		static_cast<void>(ecx);
		return avx2Tier;
	#else
		return avx2Tier && (ebx & (1u << 16)) != 0 && (ebx & (1u << 30)) != 0 && (ecx & (1u << 6)) != 0;
	#endif
#else
		__builtin_cpu_init();
		const bool avx2Tier{ __builtin_cpu_supports("avx2") != 0 && __builtin_cpu_supports("bmi2") != 0 };
	#if FUSED_KEY_TIER == 1
		return avx2Tier;
	#else
		return avx2Tier && __builtin_cpu_supports("avx512f") != 0 && __builtin_cpu_supports("avx512bw") != 0 && __builtin_cpu_supports("avx512vbmi2") != 0;
	#endif
#endif
	}

	inline constexpr uint64_t keyCount{ 16384 };

	template<benchmarksuite::string_literal test_name, auto u64_scan, auto xmm_scan> void runPair(std::string_view key, uint64_t seed) {
		const corpus_storage corpus{ buildCorpus(key, keyCount, seed) };
		stage_type::run_benchmark<test_name, "u64-overlap", scan_functor<u64_scan>>(corpus);
		stage_type::run_benchmark<test_name, "xmm-memcpy-upto", scan_functor<xmm_scan>>(corpus);
	}

}

int32_t main() {
	if (!tierSupported()) {
		std::cout << "Skipping " << tierName << " fused-key comparison: this CPU does not support the tier." << std::endl;
		return 0;
	}
	benchmarksuite::pin_for_benchmark();
	runPair<"9-byte-key", fused_key::scanU64Key9, fused_key::scanXmmKey9>(fused_key::key9, 0x9E3779B97F4A7C15ull);
	runPair<"11-byte-key", fused_key::scanU64Key11, fused_key::scanXmmKey11>(fused_key::key11, 0xBF58476D1CE4E5B9ull);
	runPair<"14-byte-key", fused_key::scanU64Key14, fused_key::scanXmmKey14>(fused_key::key14, 0x94D049BB133111EBull);
	std::cout << "Fused-key comparison tier: " << tierName << std::endl;
	for (const char* testName: { "9-byte-key", "11-byte-key", "14-byte-key" }) {
		std::cout << stage_type::get_test_results(testName).to_markdown(true, true) << std::endl;
	}
	return 0;
}
