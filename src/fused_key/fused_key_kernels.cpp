#include "fused_key_kernels.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

#if defined(__aarch64__) || defined(_M_ARM64)
	#include <arm_neon.h>
	#define FUSED_KEY_NEON 1
#else
	#include <immintrin.h>
	#define FUSED_KEY_NEON 0
#endif

#if defined(_MSC_VER) && !defined(__clang__)
	#define FUSED_KEY_INLINE [[msvc::forceinline]] inline
#else
	#define FUSED_KEY_INLINE inline __attribute__((always_inline))
#endif

namespace fused_key {

	namespace {

#if FUSED_KEY_NEON
		inline constexpr uint64_t registerWidth{ 16 };
#elif defined(__AVX512F__)
		inline constexpr uint64_t registerWidth{ 64 };
#else
		inline constexpr uint64_t registerWidth{ 32 };
#endif

		consteval uint64_t packEight(std::string_view text, std::size_t offset) {
			uint64_t value{};
			for (std::size_t i = 0; i < 8; ++i) {
				value |= static_cast<uint64_t>(static_cast<uint8_t>(text[offset + i])) << (i * 8);
			}
			return value;
		}

		consteval std::array<uint8_t, 16> packSixteen(std::string_view text) {
			std::array<uint8_t, 16> values{};
			for (std::size_t i = 0; i < text.size(); ++i) {
				values[i] = static_cast<uint8_t>(text[i]);
			}
			return values;
		}

		template<uint64_t size> FUSED_KEY_INLINE void pow2MemcpyWrapper(void* __restrict dst, const void* __restrict src) noexcept {
			std::memcpy(dst, src, size);
		}

		template<uint64_t bucket_size, typename sequence_type> struct memcpy_runner_impl;

		template<uint64_t bucket_size, uint64_t... indices> struct memcpy_runner_impl<bucket_size, std::integer_sequence<uint64_t, indices...>> {
			static constexpr uint64_t chunk_size{ bucket_size < registerWidth ? bucket_size : registerWidth };
			FUSED_KEY_INLINE static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count) noexcept {
				std::byte* __restrict dst_tail		 = dst + byte_count - bucket_size;
				const std::byte* __restrict src_tail = src + byte_count - bucket_size;
				(pow2MemcpyWrapper<chunk_size>(dst + indices * chunk_size, src + indices * chunk_size), ...);
				(pow2MemcpyWrapper<chunk_size>(dst_tail + indices * chunk_size, src_tail + indices * chunk_size), ...);
			}
		};

		template<uint64_t bucket_size> struct memcpy_runner {
			static constexpr uint64_t chunk_size{ bucket_size < registerWidth ? bucket_size : registerWidth };
			static constexpr uint64_t chunk_count{ bucket_size / chunk_size };
			FUSED_KEY_INLINE static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count) noexcept {
				memcpy_runner_impl<bucket_size, std::make_integer_sequence<uint64_t, chunk_count>>::impl(dst, src, byte_count);
			}
		};

		inline constexpr uint64_t fallbackBucketIndex{ 9 };

		template<uint64_t max_bytes> inline constexpr uint64_t maxBytesBucketIndex{ static_cast<uint64_t>(std::bit_width(max_bytes)) };

		template<uint64_t max_bytes>
		inline constexpr uint64_t topBucketIndex{ maxBytesBucketIndex<max_bytes> < fallbackBucketIndex ? maxBytesBucketIndex<max_bytes> : fallbackBucketIndex };

		template<uint64_t bucket_index>
		FUSED_KEY_INLINE void copyBucket([[maybe_unused]] std::byte* __restrict dst, [[maybe_unused]] const std::byte* __restrict src, [[maybe_unused]] uint64_t byte_count) noexcept {
			if constexpr (bucket_index == 1) {
				*dst = *src;
			} else if constexpr (bucket_index == fallbackBucketIndex) {
				std::memcpy(dst, src, byte_count);
			} else if constexpr (bucket_index > 1) {
				memcpy_runner<1ull << (bucket_index - 1)>::impl(dst, src, byte_count);
			}
		}

		template<uint64_t low_index, uint64_t high_index>
		FUSED_KEY_INLINE void dispatchBuckets(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byte_count) noexcept {
			if constexpr (low_index == high_index) {
				copyBucket<low_index>(dst, src, byte_count);
			} else {
				constexpr uint64_t split_index{ (low_index + high_index + 1) / 2 };
				if (byte_count < (1ull << (split_index - 1))) {
					dispatchBuckets<low_index, split_index - 1>(dst, src, byte_count);
				} else {
					dispatchBuckets<split_index, high_index>(dst, src, byte_count);
				}
			}
		}

		template<uint64_t max_bytes> FUSED_KEY_INLINE void jsonifierMemcpyUpTo(void* __restrict dst, const void* __restrict src, uint64_t byte_count) noexcept {
			dispatchBuckets<0, topBucketIndex<max_bytes>>(static_cast<std::byte*>(dst), static_cast<const std::byte*>(src), byte_count);
		}

		FUSED_KEY_INLINE void jsonifierMemcpy(void* __restrict dst, const void* __restrict src, uint64_t byte_count) noexcept {
			jsonifierMemcpyUpTo<std::numeric_limits<uint64_t>::max()>(dst, src, byte_count);
		}

		template<uint64_t remaining, uint64_t offset> FUSED_KEY_INLINE void pow2DecompMemcpy(std::byte* __restrict dst, const std::byte* __restrict src) noexcept {
			if constexpr (remaining > 0) {
				static constexpr uint64_t chunk{ std::bit_floor(remaining) };
				pow2MemcpyWrapper<chunk>(dst + offset, src + offset);
				pow2DecompMemcpy<remaining - chunk, offset + chunk>(dst, src);
			}
		}

		struct copy_bucketed {
			template<uint64_t byte_count> FUSED_KEY_INLINE static void impl(void* __restrict dst, const void* __restrict src) noexcept {
				jsonifierMemcpyUpTo<15>(dst, src, byte_count);
			}
		};

		struct copy_jsonifier_memcpy {
			template<uint64_t byte_count> FUSED_KEY_INLINE static void impl(void* __restrict dst, const void* __restrict src) noexcept {
				jsonifierMemcpy(dst, src, byte_count);
			}
		};

		struct copy_pow2_decomp {
			template<uint64_t byte_count> FUSED_KEY_INLINE static void impl(void* __restrict dst, const void* __restrict src) noexcept {
				pow2DecompMemcpy<byte_count, 0>(static_cast<std::byte*>(dst), static_cast<const std::byte*>(src));
			}
		};

		template<const std::string_view& text>
			requires(text.size() > 8 && text.size() < 16)
		struct comparator_u64 {
			static constexpr uint64_t newCount{ text.size() };
			static constexpr uint64_t loValuesNew{ packEight(text, 0) };
			static constexpr uint64_t hiValuesNew{ packEight(text, newCount - 8) };

			FUSED_KEY_INLINE static const char* impl(const char* __restrict input) noexcept {
				uint64_t lo;
				uint64_t hi;
				pow2MemcpyWrapper<8>(&lo, input);
				pow2MemcpyWrapper<8>(&hi, input + newCount - 8);
				return !((lo ^ loValuesNew) | (hi ^ hiValuesNew)) ? input + newCount : nullptr;
			}
		};

		template<const std::string_view& text, typename copy_type>
			requires(text.size() > 8 && text.size() < 16)
		struct comparator_xmm {
			static constexpr uint64_t newCount{ text.size() };
			alignas(64) static constexpr std::array<uint8_t, 16> valuesNew{ packSixteen(text) };

#if FUSED_KEY_NEON
			FUSED_KEY_INLINE static const char* impl(const char* __restrict input) noexcept {
				uint8x16_t data1{ vdupq_n_u8(0) };
				copy_type::template impl<newCount>(&data1, input);
				const uint8x16_t data2{ vld1q_u8(valuesNew.data()) };
				return vmaxvq_u8(veorq_u8(data1, data2)) == 0 ? input + newCount : nullptr;
			}
#else
			FUSED_KEY_INLINE static const char* impl(const char* __restrict input) noexcept {
				__m128i data1{ _mm_setzero_si128() };
				copy_type::template impl<newCount>(&data1, input);
				const __m128i data2{ _mm_load_si128(static_cast<const __m128i*>(static_cast<const void*>(valuesNew.data()))) };
				const __m128i diff{ _mm_xor_si128(data1, data2) };
				return _mm_testz_si128(diff, diff) ? input + newCount : nullptr;
			}
#endif
		};

		template<typename comparator_type> FUSED_KEY_INLINE uint64_t scan(const key_corpus& corpus) noexcept {
			const char* __restrict base{ corpus.data };
			const uint32_t* __restrict offsets{ corpus.offsets };
			uint64_t matched{};
			for (uint64_t i = 0; i < corpus.count; ++i) {
				matched += static_cast<uint64_t>(comparator_type::impl(base + offsets[i]) != nullptr);
			}
			return matched;
		}

		struct u64_family {
			template<const std::string_view& text> using type = comparator_u64<text>;
		};

		template<typename copy_type> struct xmm_family {
			template<const std::string_view& text> using type = comparator_xmm<text, copy_type>;
		};

		template<typename family> FUSED_KEY_INLINE const char* compareMixed(uint8_t keyId, const char* __restrict input) noexcept {
			switch (keyId) {
				case 0:
					return family::template type<key9>::impl(input);
				case 1:
					return family::template type<key10>::impl(input);
				case 2:
					return family::template type<key11>::impl(input);
				case 3:
					return family::template type<key12>::impl(input);
				case 4:
					return family::template type<key13>::impl(input);
				case 5:
					return family::template type<key14>::impl(input);
				case 6:
					return family::template type<key15>::impl(input);
				default:
					return nullptr;
			}
		}

		template<typename family> FUSED_KEY_INLINE uint64_t scanMixed(const key_corpus& corpus) noexcept {
			const char* __restrict base{ corpus.data };
			const uint32_t* __restrict offsets{ corpus.offsets };
			const uint8_t* __restrict keyIds{ corpus.keyIds };
			uint64_t matched{};
			for (uint64_t i = 0; i < corpus.count; ++i) {
				matched += static_cast<uint64_t>(compareMixed<family>(keyIds[i], base + offsets[i]) != nullptr);
			}
			return matched;
		}

	}

	uint64_t scanU64Key9(const key_corpus& corpus) noexcept {
		return scan<comparator_u64<key9>>(corpus);
	}

	uint64_t scanU64Key11(const key_corpus& corpus) noexcept {
		return scan<comparator_u64<key11>>(corpus);
	}

	uint64_t scanU64Key14(const key_corpus& corpus) noexcept {
		return scan<comparator_u64<key14>>(corpus);
	}

	uint64_t scanXmmKey9(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key9, copy_bucketed>>(corpus);
	}

	uint64_t scanXmmJsonifierMemcpyKey9(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key9, copy_jsonifier_memcpy>>(corpus);
	}

	uint64_t scanXmmPow2Key9(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key9, copy_pow2_decomp>>(corpus);
	}

	uint64_t scanXmmKey11(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key11, copy_bucketed>>(corpus);
	}

	uint64_t scanXmmJsonifierMemcpyKey11(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key11, copy_jsonifier_memcpy>>(corpus);
	}

	uint64_t scanXmmPow2Key11(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key11, copy_pow2_decomp>>(corpus);
	}

	uint64_t scanXmmKey14(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key14, copy_bucketed>>(corpus);
	}

	uint64_t scanXmmJsonifierMemcpyKey14(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key14, copy_jsonifier_memcpy>>(corpus);
	}

	uint64_t scanXmmPow2Key14(const key_corpus& corpus) noexcept {
		return scan<comparator_xmm<key14, copy_pow2_decomp>>(corpus);
	}

	uint64_t scanU64Mixed(const key_corpus& corpus) noexcept {
		return scanMixed<u64_family>(corpus);
	}

	uint64_t scanXmmMixed(const key_corpus& corpus) noexcept {
		return scanMixed<xmm_family<copy_bucketed>>(corpus);
	}

	uint64_t scanXmmJsonifierMemcpyMixed(const key_corpus& corpus) noexcept {
		return scanMixed<xmm_family<copy_jsonifier_memcpy>>(corpus);
	}

	uint64_t scanXmmPow2Mixed(const key_corpus& corpus) noexcept {
		return scanMixed<xmm_family<copy_pow2_decomp>>(corpus);
	}

}
