#include <benchmarksuite>

#include "fused_key_kernels.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__) && FUSED_KEY_TIER != 0
	#include <intrin.h>
#endif

#if FUSED_KEY_TIER == 0
using stage_type = benchmarksuite::benchmark_stage<"fused-key-compare-neon", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_iteration_count = 20000 }>;
inline constexpr std::string_view tierName{ "NEON" };
#elif FUSED_KEY_TIER == 1
using stage_type = benchmarksuite::benchmark_stage<"fused-key-compare-avx2", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_iteration_count = 20000 }>;
inline constexpr std::string_view tierName{ "AVX2" };
#else
using stage_type = benchmarksuite::benchmark_stage<"fused-key-compare-avx512", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_iteration_count = 20000 }>;
inline constexpr std::string_view tierName{ "AVX512" };
#endif

namespace {

	struct corpus_storage {
		std::string buffer{};
		std::vector<uint32_t> offsets{};
		fused_key::key_corpus view{};
		uint64_t keyBytes{};
	};

	void appendValue(std::string& buffer, std::mt19937_64& rng) {
		std::uniform_int_distribution<uint64_t> digitCount{ 1, 12 };
		std::uniform_int_distribution<uint32_t> digit{ 0, 9 };
		const uint64_t digits{ digitCount(rng) };
		for (uint64_t d = 0; d < digits; ++d) {
			buffer.push_back(static_cast<char>('0' + digit(rng)));
		}
		buffer.push_back(',');
	}

	corpus_storage buildCorpus(std::string_view key, uint64_t keyCount, uint64_t seed) {
		corpus_storage corpus{};
		std::mt19937_64 rng{ seed };
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
			appendValue(corpus.buffer, rng);
		}
		corpus.buffer.append(64, ' ');
		corpus.view	   = fused_key::key_corpus{ corpus.buffer.data(), corpus.offsets.data(), corpus.offsets.size() };
		corpus.keyBytes = keyCount * key.size();
		return corpus;
	}

	struct mixed_corpus_storage {
		std::string buffer{};
		std::vector<uint32_t> offsets{};
		std::vector<uint8_t> lengths{};
		std::vector<uint64_t> expectedLo{};
		std::vector<uint64_t> expectedHi{};
		std::vector<fused_key::expected_block> expectedBlocks{};
		fused_key::mixed_key_corpus view{};
		uint64_t keyBytes{};
	};

	mixed_corpus_storage buildMixedCorpus(uint64_t keyCount, uint64_t seed) {
		mixed_corpus_storage corpus{};
		std::mt19937_64 rng{ seed };
		std::uniform_int_distribution<uint64_t> keyChoice{ 0, fused_key::mixedKeys.size() - 1 };
		std::uniform_int_distribution<uint64_t> mismatch{ 0, 15 };
		corpus.buffer.reserve(keyCount * 32 + 64);
		corpus.offsets.reserve(keyCount);
		corpus.lengths.reserve(keyCount);
		corpus.expectedLo.reserve(keyCount);
		corpus.expectedHi.reserve(keyCount);
		corpus.expectedBlocks.reserve(keyCount);
		for (uint64_t i = 0; i < keyCount; ++i) {
			const std::string_view key{ fused_key::mixedKeys[keyChoice(rng)] };
			uint64_t lo{};
			uint64_t hi{};
			fused_key::expected_block block{};
			std::memcpy(&lo, key.data(), 8);
			std::memcpy(&hi, key.data() + key.size() - 8, 8);
			std::memcpy(block.bytes, key.data(), key.size());
			corpus.offsets.emplace_back(static_cast<uint32_t>(corpus.buffer.size()));
			corpus.lengths.emplace_back(static_cast<uint8_t>(key.size()));
			corpus.expectedLo.emplace_back(lo);
			corpus.expectedHi.emplace_back(hi);
			corpus.expectedBlocks.emplace_back(block);
			std::string keyCopy{ key };
			if (mismatch(rng) == 0) {
				const uint64_t flipIndex{ 1 + (i % (keyCopy.size() - 3)) };
				keyCopy[flipIndex] = static_cast<char>(keyCopy[flipIndex] ^ 0x20);
			}
			corpus.buffer += keyCopy;
			corpus.keyBytes += key.size();
			appendValue(corpus.buffer, rng);
		}
		corpus.buffer.append(64, ' ');
		corpus.view = fused_key::mixed_key_corpus{ corpus.buffer.data(), corpus.offsets.data(), corpus.lengths.data(), corpus.expectedLo.data(), corpus.expectedHi.data(),
			corpus.expectedBlocks.data(), corpus.offsets.size() };
		return corpus;
	}

	template<auto scan_function> struct scan_functor {
		BNCH_SWT_HOST static uint64_t impl(const corpus_storage& corpus) {
			uint64_t matched{ scan_function(corpus.view) };
			benchmarksuite::do_not_optimize_away(matched);
			return corpus.keyBytes;
		}
	};

	template<auto scan_function> struct mixed_scan_functor {
		BNCH_SWT_HOST static uint64_t impl(const mixed_corpus_storage& corpus) {
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

	template<benchmarksuite::string_literal test_name, auto u64_scan, auto xmm_scan, auto xmm_jsonifier_memcpy_scan, auto xmm_pow2_scan> void runSet(const corpus_storage& corpus) {
		stage_type::run_benchmark<test_name, "u64-overlap", scan_functor<u64_scan>>(corpus);
		stage_type::run_benchmark<test_name, "xmm-memcpy-upto", scan_functor<xmm_scan>>(corpus);
		stage_type::run_benchmark<test_name, "xmm-jsonifier-memcpy", scan_functor<xmm_jsonifier_memcpy_scan>>(corpus);
		stage_type::run_benchmark<test_name, "xmm-pow2-decomp", scan_functor<xmm_pow2_scan>>(corpus);
	}

	template<benchmarksuite::string_literal test_name, auto u64_scan, auto xmm_scan, auto xmm_jsonifier_memcpy_scan> void runMixedSet(const mixed_corpus_storage& corpus) {
		stage_type::run_benchmark<test_name, "u64-overlap", mixed_scan_functor<u64_scan>>(corpus);
		stage_type::run_benchmark<test_name, "xmm-memcpy-upto", mixed_scan_functor<xmm_scan>>(corpus);
		stage_type::run_benchmark<test_name, "xmm-jsonifier-memcpy", mixed_scan_functor<xmm_jsonifier_memcpy_scan>>(corpus);
	}

}

int32_t main() {
	if (!tierSupported()) {
		std::cout << "Skipping " << tierName << " fused-key comparison: this CPU does not support the tier." << std::endl;
		return 0;
	}
	benchmarksuite::pin_for_benchmark();
	runSet<"9-byte-key", fused_key::scanU64Key9, fused_key::scanXmmKey9, fused_key::scanXmmJsonifierMemcpyKey9, fused_key::scanXmmPow2Key9>(buildCorpus(fused_key::key9, keyCount, 0x9E3779B97F4A7C15ull));
	runSet<"11-byte-key", fused_key::scanU64Key11, fused_key::scanXmmKey11, fused_key::scanXmmJsonifierMemcpyKey11, fused_key::scanXmmPow2Key11>(buildCorpus(fused_key::key11, keyCount, 0xBF58476D1CE4E5B9ull));
	runSet<"14-byte-key", fused_key::scanU64Key14, fused_key::scanXmmKey14, fused_key::scanXmmJsonifierMemcpyKey14, fused_key::scanXmmPow2Key14>(buildCorpus(fused_key::key14, keyCount, 0x94D049BB133111EBull));
	runMixedSet<"mixed-9-to-15-byte-keys", fused_key::scanU64Mixed, fused_key::scanXmmMixed, fused_key::scanXmmJsonifierMemcpyMixed>(buildMixedCorpus(keyCount, 0xD6E8FEB86659FD93ull));
	std::cout << "Fused-key comparison tier: " << tierName << std::endl;
	for (const char* testName: { "9-byte-key", "11-byte-key", "14-byte-key", "mixed-9-to-15-byte-keys" }) {
		std::cout << stage_type::get_test_results(testName).to_markdown(true, true) << std::endl;
	}
	return 0;
}
