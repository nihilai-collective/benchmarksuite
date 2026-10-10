/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/main.cpp
 */

#include <simdjson.h>
#include <jsonifier>
#include <benchmarksuite>
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace string_processing {

	namespace simd = jsonifier::internal::simd;

	using simd_type = jsonifier::internal::jsonifier_simd_int_128;

	static constexpr uint64_t register_bytes{ sizeof(simd_type) };
	static constexpr uint64_t input_padding{ 128 };

	struct lane_masks {
		JSONIFIER_INLINE static uint32_t quotes(const simd_type values) noexcept {
			return static_cast<uint32_t>(simd::opBitMask(simd::opCmpEqRaw(values, simd::gatherValue<simd_type>('"'))));
		}
	};

	struct unescaper {
		JSONIFIER_INLINE static bool impl(const uint8_t*& src, const uint8_t* src_end, uint8_t*& dst) noexcept {
			if (*src < 0x20) {
				return false;
			}
			if (src[1] == 'u') {
				return jsonifier::internal::handleUnicodeCodePoint(src, dst, src_end);
			}
			const uint8_t decoded{ static_cast<uint8_t>(jsonifier::internal::escapeMap[src[1]]) };
			*dst = decoded;
			++dst;
			src += 2;
			return decoded != 0;
		}
	};

	template<uint64_t width> using simd_reg = jsonifier::internal::simd_register_t<width>;

	template<uint64_t width> struct register_type {
		using type = simd_reg<width>;
	};

	template<uint64_t width> using register_validator = jsonifier::internal::utf8_register_validator<register_type<width>>;

	using validation_state = jsonifier::internal::utf8_validation_state;

	static constexpr uint64_t widest_bytes{ jsonifier::internal::simdBytesPerRegister };
	static constexpr uint64_t mid_bytes{ std::min<uint64_t>(widest_bytes, 32) };
	static constexpr uint64_t swar_highs{ 0x8080808080808080ull };
	static constexpr uint64_t swar_lows{ 0x0101010101010101ull };
	static constexpr uint64_t no_stop{ std::numeric_limits<uint64_t>::max() };
	static constexpr uint64_t swar_bytes{ 8 };

	enum class scan_status : uint8_t {
		proceed,
		escape,
		done,
		failed,
	};

	template<uint64_t width> struct lanes {
		using type = simd_reg<width>;

		JSONIFIER_INLINE static type load(const uint8_t* src) noexcept {
			return simd::gatherValuesU<type>(src);
		}

		JSONIFIER_INLINE static type splat(const uint8_t value) noexcept {
			return simd::gatherValue<type>(static_cast<char>(value));
		}

		JSONIFIER_INLINE static uint64_t bits(const type values) noexcept {
			if constexpr (width == 64) {
				return static_cast<uint64_t>(simd::opBitMask(values));
			} else {
				return static_cast<uint64_t>(static_cast<uint32_t>(simd::opBitMask(values)));
			}
		}

		JSONIFIER_INLINE static uint64_t escapes(const type values) noexcept {
			return bits(simd::opOr(simd::opCmpEqRaw(values, splat('\\')), simd::opCmpLtRaw(values, splat(0x20))));
		}

		JSONIFIER_INLINE static uint64_t stops(const type values) noexcept {
			return escapes(values) | bits(simd::opCmpEqRaw(values, splat('"')));
		}

		JSONIFIER_INLINE static uint64_t below(const uint64_t limit) noexcept {
			return limit < width ? (uint64_t{ 1 } << limit) - 1 : std::numeric_limits<uint64_t>::max();
		}
	};

	struct swar {
		JSONIFIER_INLINE static uint64_t load(const uint8_t* src) noexcept {
			uint64_t value;
			std::memcpy(&value, src, sizeof(value));
			return value;
		}

		JSONIFIER_INLINE static void store(const uint64_t value, uint8_t* dst) noexcept {
			std::memcpy(dst, &value, sizeof(value));
		}

		JSONIFIER_INLINE static uint64_t has_byte(const uint64_t value, const uint8_t target) noexcept {
			const uint64_t matched{ value ^ (swar_lows * target) };
			return (matched - swar_lows) & ~matched & swar_highs;
		}

		JSONIFIER_INLINE static uint64_t has_less(const uint64_t value, const uint8_t bound) noexcept {
			return (value - swar_lows * bound) & ~value & swar_highs;
		}

		JSONIFIER_INLINE static uint64_t escapes(const uint64_t value) noexcept {
			return has_byte(value, '\\') | has_less(value, 0x20);
		}

		JSONIFIER_INLINE static uint64_t stops(const uint64_t value) noexcept {
			return escapes(value) | has_byte(value, '"');
		}

		JSONIFIER_INLINE static uint64_t below(const uint64_t limit) noexcept {
			return (uint64_t{ 1 } << (limit * 8)) - 1;
		}

		JSONIFIER_INLINE static uint64_t through(const uint64_t limit) noexcept {
			return (uint64_t{ 2 } << (limit * 8 + 7)) - 1;
		}

		JSONIFIER_INLINE static uint64_t first(const uint64_t mask) noexcept {
			return mask != 0 ? static_cast<uint64_t>(std::countr_zero(mask)) >> 3 : no_stop;
		}
	};

	struct tail_copy {
		template<uint64_t bytes> JSONIFIER_INLINE static void move(const uint8_t* src, uint8_t* dst) noexcept {
			std::memcpy(dst, src, bytes);
		}

		JSONIFIER_INLINE static void impl(const uint8_t* src, uint8_t* dst, const uint64_t count) noexcept {
			switch (std::bit_width(count)) {
				case 4:
					move<8>(src, dst);
					move<8>(src + count - 8, dst + count - 8);
					return;
				case 3:
					move<4>(src, dst);
					move<4>(src + count - 4, dst + count - 4);
					return;
				case 2:
					move<2>(src, dst);
					move<2>(src + count - 2, dst + count - 2);
					return;
				case 1:
					*dst = *src;
					return;
				default:
					return;
			}
		}
	};

	template<uint64_t width> struct exact_width_loop {
		JSONIFIER_INLINE static bool impl(const uint8_t*& src, const uint8_t* content_end, uint8_t*& dst, validation_state& state) noexcept {
			using lane = lanes<width>;
			register_validator<width> validator{ state };
			while (static_cast<uint64_t>(content_end - src) >= width) {
				const simd_reg<width> values{ lane::load(src) };
				validator.checkRegister(values);
				simd::storeU(values, dst);
				const uint64_t stop_mask{ lane::escapes(values) };
				if (stop_mask != 0) {
					const uint64_t offset{ static_cast<uint64_t>(std::countr_zero(stop_mask)) };
					src += offset;
					dst += offset;
					if (validator.hardErrors() || !unescaper::impl(src, content_end, dst)) {
						return false;
					}
					validator.reset();
					continue;
				}
				src += width;
				dst += width;
			}
			validator.flush();
			return !state.error;
		}
	};

	struct exact_tail {
		JSONIFIER_INLINE static uint64_t vector_offset(const uint8_t* src, validation_state& state, const uint64_t remaining, bool& valid) noexcept {
			using lane = lanes<16>;
			register_validator<16> validator{ state };
			const simd_reg<16> values{ lane::load(src) };
			validator.checkRegister(values);
			valid = !validator.hardErrors();
			const uint64_t stop_mask{ lane::escapes(values) & lane::below(remaining) };
			return stop_mask != 0 ? static_cast<uint64_t>(std::countr_zero(stop_mask)) : no_stop;
		}

		JSONIFIER_INLINE static uint64_t offset(const uint8_t* src, validation_state& state, const uint64_t remaining, bool& valid) noexcept {
			if (remaining < swar_bytes) {
				const uint64_t word{ swar::load(src) };
				if ((word & swar_highs & swar::through(remaining)) == 0) {
					valid = !state.prevIncomplete && !state.error;
					return swar::first(swar::escapes(word) & swar::below(remaining));
				}
			}
			return vector_offset(src, state, remaining, valid);
		}

		JSONIFIER_INLINE static uint8_t* impl(const uint8_t* src, const uint8_t* content_end, uint8_t* dst, validation_state& state) noexcept {
			while (true) {
				const uint64_t remaining{ static_cast<uint64_t>(content_end - src) };
				bool valid{};
				const uint64_t stop{ offset(src, state, remaining, valid) };
				if (!valid) {
					return nullptr;
				}
				tail_copy::impl(src, dst, stop != no_stop ? stop : remaining);
				if (stop == no_stop) {
					return dst + remaining;
				}
				src += stop;
				dst += stop;
				if (!unescaper::impl(src, content_end, dst)) {
					return nullptr;
				}
				state.reset();
			}
		}
	};

	struct padded_tail {
		JSONIFIER_INLINE static uint8_t* impl(const uint8_t* src, const uint8_t* content_end, uint8_t* dst, validation_state& state) noexcept {
			using lane = lanes<16>;
			while (true) {
				const uint64_t remaining{ static_cast<uint64_t>(content_end - src) };
				register_validator<16> validator{ state };
				const simd_reg<16> values{ lane::load(src) };
				validator.checkRegister(values);
				simd::storeU(values, dst);
				const uint64_t stop_mask{ lane::escapes(values) & lane::below(remaining) };
				if (validator.hardErrors()) {
					return nullptr;
				}
				if (stop_mask == 0) {
					return dst + remaining;
				}
				const uint64_t offset{ static_cast<uint64_t>(std::countr_zero(stop_mask)) };
				src += offset;
				dst += offset;
				if (!unescaper::impl(src, content_end, dst)) {
					return nullptr;
				}
				state.reset();
			}
		}
	};

	template<bool padded_output> struct exact_cascade_parser {
		JSONIFIER_INLINE static uint8_t* impl(const uint8_t* quote, const uint8_t* end, uint8_t* dst) noexcept {
			return resume(quote + 1, end - 1, dst);
		}

		JSONIFIER_INLINE static uint8_t* resume(const uint8_t* src, const uint8_t* const content_end, uint8_t* dst) noexcept {
			validation_state state{};
			if (!exact_width_loop<widest_bytes>::impl(src, content_end, dst, state)) {
				return nullptr;
			}
			if constexpr (mid_bytes < widest_bytes) {
				if (!exact_width_loop<mid_bytes>::impl(src, content_end, dst, state)) {
					return nullptr;
				}
			}
			if constexpr (16 < mid_bytes) {
				if (!exact_width_loop<16>::impl(src, content_end, dst, state)) {
					return nullptr;
				}
			}
			if constexpr (padded_output) {
				return padded_tail::impl(src, content_end, dst, state);
			} else {
				return exact_tail::impl(src, content_end, dst, state);
			}
		}
	};

	using parse_exact_cascade		 = exact_cascade_parser<false>;
	using parse_exact_cascade_padded = exact_cascade_parser<true>;

	struct parse_to_token_cascade {
		JSONIFIER_INLINE static const uint8_t* find_end(const uint8_t* next) noexcept {
			const uint8_t* window{ next - register_bytes };
			uint32_t quotes{ lane_masks::quotes(simd::gatherValuesU<simd_type>(window)) };
			while (quotes == 0) {
				window -= register_bytes;
				quotes = lane_masks::quotes(simd::gatherValuesU<simd_type>(window));
			}
			return window + std::bit_width(quotes);
		}
	};

	struct open_swar_probe {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src, const uint8_t* src_end, uint8_t*& dst) noexcept {
			const uint64_t word{ swar::load(src) };
			const uint64_t stop_mask{ swar::stops(word) };
			const uint64_t lanes_used{ stop_mask != 0 ? swar::through(static_cast<uint64_t>(std::countr_zero(stop_mask)) >> 3) : std::numeric_limits<uint64_t>::max() };
			if ((word & swar_highs & lanes_used) != 0) {
				return scan_status::proceed;
			}
			swar::store(word, dst);
			if (stop_mask == 0) {
				src += swar_bytes;
				dst += swar_bytes;
				return scan_status::proceed;
			}
			const uint64_t offset{ static_cast<uint64_t>(std::countr_zero(stop_mask)) >> 3 };
			src += offset;
			dst += offset;
			if (*src == '"') {
				return scan_status::done;
			}
			return unescaper::impl(src, src_end, dst) ? scan_status::proceed : scan_status::failed;
		}
	};

	template<bool repeat> struct scan_repeat {
		JSONIFIER_INLINE static bool impl(const uint8_t* src, const uint8_t* src_end) noexcept {
			return src < src_end;
		}
	};

	template<> struct scan_repeat<false> {
		JSONIFIER_INLINE static bool impl(const uint8_t*, const uint8_t*) noexcept {
			return false;
		}
	};

	template<uint64_t width, bool repeat> struct open_width_scan {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src, const uint8_t* src_end, uint8_t*& dst, validation_state& state) noexcept {
			using lane = lanes<width>;
			register_validator<width> validator{ state };
			do {
				const simd_reg<width> values{ lane::load(src) };
				validator.checkRegister(values);
				simd::storeU(values, dst);
				const uint64_t stop_mask{ lane::stops(values) };
				if (stop_mask != 0) {
					const uint64_t offset{ static_cast<uint64_t>(std::countr_zero(stop_mask)) };
					src += offset;
					dst += offset;
					if (validator.hardErrors()) {
						return scan_status::failed;
					}
					if (*src == '"') {
						return scan_status::done;
					}
					if (!unescaper::impl(src, src_end, dst)) {
						return scan_status::failed;
					}
					validator.reset();
					continue;
				}
				src += width;
				dst += width;
			} while (scan_repeat<repeat>::impl(src, src_end));
			validator.flush();
			return state.error ? scan_status::failed : scan_status::proceed;
		}
	};

	struct parse_open_cascade {
		JSONIFIER_INLINE static uint8_t* impl(const uint8_t* quote, const uint8_t* src_end, uint8_t* dst) noexcept {
			const uint8_t* src{ quote + 1 };
			validation_state state{};
			scan_status status{ open_swar_probe::impl(src, src_end, dst) };
			if (status == scan_status::proceed) {
				status = open_width_scan<16, false>::impl(src, src_end, dst, state);
			}
			if constexpr (16 < mid_bytes) {
				if (status == scan_status::proceed) {
					status = open_width_scan<mid_bytes, false>::impl(src, src_end, dst, state);
				}
			}
			if (status == scan_status::proceed) {
				status = open_width_scan<widest_bytes, true>::impl(src, src_end, dst, state);
			}
			return status == scan_status::done ? dst : nullptr;
		}
	};

	template<uint64_t width> struct overlapping_copy {
		JSONIFIER_INLINE static void impl(const uint8_t* src, uint8_t* dst, const uint64_t count) noexcept {
			if constexpr (width >= 16) {
				const simd_reg<width> head{ lanes<width>::load(src) };
				const simd_reg<width> tail{ lanes<width>::load(src + count - width) };
				simd::storeU(head, dst);
				simd::storeU(tail, dst + count - width);
			} else {
				std::memcpy(dst, src, width);
				std::memcpy(dst + count - width, src + count - width, width);
			}
		}
	};

	template<uint64_t width> struct block_copy {
		JSONIFIER_INLINE static void impl(const uint8_t* src, uint8_t* dst, uint64_t count) noexcept {
			while (count > width) {
				simd::storeU(lanes<width>::load(src), dst);
				src += width;
				dst += width;
				count -= width;
			}
			simd::storeU(lanes<width>::load(src + count - width), dst + count - width);
		}
	};

	struct cascade_copy {
		JSONIFIER_INLINE static void impl(const uint8_t* src, uint8_t* dst, const uint64_t count) noexcept {
			switch (std::bit_width(count)) {
				case 0:
					return;
				case 1:
				case 2:
					dst[0]			= src[0];
					dst[count >> 1] = src[count >> 1];
					dst[count - 1]	= src[count - 1];
					return;
				case 3:
					overlapping_copy<4>::impl(src, dst, count);
					return;
				case 4:
					overlapping_copy<8>::impl(src, dst, count);
					return;
				case 5:
					overlapping_copy<16>::impl(src, dst, count);
					return;
				case 6:
					if constexpr (mid_bytes >= 32) {
						overlapping_copy<32>::impl(src, dst, count);
					} else {
						block_copy<16>::impl(src, dst, count);
					}
					return;
				case 7:
					if constexpr (widest_bytes >= 64) {
						overlapping_copy<64>::impl(src, dst, count);
					} else {
						block_copy<widest_bytes>::impl(src, dst, count);
					}
					return;
				default:
					block_copy<widest_bytes>::impl(src, dst, count);
					return;
			}
		}
	};

	template<uint64_t width> struct plain_exact_loop {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src, const uint8_t* content_end, uint8_t*& dst, validation_state& state) noexcept {
			using lane = lanes<width>;
			register_validator<width> validator{ state };
			while (static_cast<uint64_t>(content_end - src) >= width) {
				const simd_reg<width> values{ lane::load(src) };
				validator.checkRegister(values);
				simd::storeU(values, dst);
				const uint64_t stop_mask{ lane::escapes(values) };
				if (stop_mask != 0) {
					const uint64_t offset{ static_cast<uint64_t>(std::countr_zero(stop_mask)) };
					src += offset;
					dst += offset;
					return validator.hardErrors() ? scan_status::failed : scan_status::escape;
				}
				src += width;
				dst += width;
			}
			validator.flush();
			return state.error ? scan_status::failed : scan_status::proceed;
		}
	};

	struct plain_exact_tail {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src, const uint8_t* content_end, uint8_t*& dst, validation_state& state) noexcept {
			const uint64_t remaining{ static_cast<uint64_t>(content_end - src) };
			bool valid{};
			const uint64_t stop{ exact_tail::offset(src, state, remaining, valid) };
			if (!valid) {
				return scan_status::failed;
			}
			const uint64_t count{ stop != no_stop ? stop : remaining };
			cascade_copy::impl(src, dst, count);
			src += count;
			dst += count;
			return stop != no_stop ? scan_status::escape : scan_status::done;
		}
	};

	struct exact_escape_continuation {
		JSONIFIER_NOINLINE static uint8_t* impl(const uint8_t* src, const uint8_t* content_end, uint8_t* dst) noexcept {
			if (!unescaper::impl(src, content_end, dst)) {
				return nullptr;
			}
			return parse_exact_cascade::resume(src, content_end, dst);
		}
	};

	struct simdonifier_exact {
		JSONIFIER_INLINE static uint8_t* impl(const uint8_t* quote, const uint8_t* end, uint8_t* dst) noexcept {
			const uint8_t* const content_end{ end - 1 };
			const uint8_t* src{ quote + 1 };
			validation_state state{};
			scan_status status{ plain_exact_loop<widest_bytes>::impl(src, content_end, dst, state) };
			if constexpr (mid_bytes < widest_bytes) {
				if (status == scan_status::proceed) {
					status = plain_exact_loop<mid_bytes>::impl(src, content_end, dst, state);
				}
			}
			if constexpr (16 < mid_bytes) {
				if (status == scan_status::proceed) {
					status = plain_exact_loop<16>::impl(src, content_end, dst, state);
				}
			}
			if (status == scan_status::proceed) {
				status = plain_exact_tail::impl(src, content_end, dst, state);
			}
			if (status == scan_status::done) {
				return dst;
			}
			if (status == scan_status::escape) {
				return exact_escape_continuation::impl(src, content_end, dst);
			}
			return nullptr;
		}
	};

	struct open_swar_scan {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src) noexcept {
			const uint64_t word{ swar::load(src) };
			const uint64_t stop_mask{ swar::stops(word) };
			const uint64_t lanes_used{ stop_mask != 0 ? swar::through(static_cast<uint64_t>(std::countr_zero(stop_mask)) >> 3) : std::numeric_limits<uint64_t>::max() };
			if ((word & swar_highs & lanes_used) != 0) {
				return scan_status::proceed;
			}
			if (stop_mask == 0) {
				src += swar_bytes;
				return scan_status::proceed;
			}
			src += static_cast<uint64_t>(std::countr_zero(stop_mask)) >> 3;
			return *src == '"' ? scan_status::done : scan_status::escape;
		}
	};

	template<uint64_t width, bool repeat> struct open_width_probe {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src, const uint8_t* src_end, validation_state& state) noexcept {
			using lane = lanes<width>;
			register_validator<width> validator{ state };
			do {
				const simd_reg<width> values{ lane::load(src) };
				validator.checkRegister(values);
				const uint64_t stop_mask{ lane::stops(values) };
				if (stop_mask != 0) {
					src += static_cast<uint64_t>(std::countr_zero(stop_mask));
					if (validator.hardErrors()) {
						return scan_status::failed;
					}
					return *src == '"' ? scan_status::done : scan_status::escape;
				}
				src += width;
			} while (scan_repeat<repeat>::impl(src, src_end));
			validator.flush();
			return state.error ? scan_status::failed : scan_status::proceed;
		}
	};

	struct open_scan {
		JSONIFIER_INLINE static scan_status impl(const uint8_t*& src, const uint8_t* src_end) noexcept {
			validation_state state{};
			scan_status status{ open_swar_scan::impl(src) };
			if (status == scan_status::proceed) {
				status = open_width_probe<16, false>::impl(src, src_end, state);
			}
			if constexpr (16 < mid_bytes) {
				if (status == scan_status::proceed) {
					status = open_width_probe<mid_bytes, false>::impl(src, src_end, state);
				}
			}
			if (status == scan_status::proceed) {
				status = open_width_probe<widest_bytes, true>::impl(src, src_end, state);
			}
			return status;
		}
	};

	struct open_escape_continuation {
		JSONIFIER_NOINLINE static uint8_t* impl(const uint8_t* quote, const uint8_t* src_end, uint8_t* dst) noexcept {
			return parse_open_cascade::impl(quote, src_end, dst);
		}
	};

	struct simdonifier_open {
		JSONIFIER_INLINE static uint8_t* impl(const uint8_t* quote, const uint8_t* src_end, uint8_t* dst) noexcept {
			const uint8_t* const start{ quote + 1 };
			const uint8_t* stop{ start };
			const scan_status status{ open_scan::impl(stop, src_end) };
			if (status == scan_status::done) {
				const uint64_t length{ static_cast<uint64_t>(stop - start) };
				cascade_copy::impl(start, dst, length);
				return dst + length;
			}
			if (status == scan_status::escape) {
				return open_escape_continuation::impl(quote, src_end, dst);
			}
			return nullptr;
		}
	};

}

struct string_case {
	uint64_t quote{};
	uint64_t next{};
	uint64_t end{};
};

struct bin_data {
	simdjson::internal::dom_parser_implementation* simdjson_parser{};
	simdjson::ondemand::parser* ondemand_parser{};
	std::vector<std::string> strings;
	std::vector<string_case> cases;
	std::vector<uint8_t> output;
	std::vector<uint8_t> input;
	uint64_t byte_count{};
};

struct kernel_context {
	simdjson::internal::dom_parser_implementation* simdjson_parser{};
	simdjson::ondemand::parser* ondemand_parser{};
	const uint8_t* src_end{};
	const uint8_t* base{};
	uint8_t* scratch{};

	JSONIFIER_INLINE explicit kernel_context(bin_data& data) noexcept
		: simdjson_parser{ data.simdjson_parser }, ondemand_parser{ data.ondemand_parser }, src_end{ data.input.data() + data.input.size() }, base{ data.input.data() },
		  scratch{ data.output.data() } {
	}
};

struct exact_cascade_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return string_processing::parse_exact_cascade_padded::impl(context.base + entry.quote, context.base + entry.end, dst);
	}
};

struct to_token_cascade_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return string_processing::parse_exact_cascade_padded::impl(context.base + entry.quote, string_processing::parse_to_token_cascade::find_end(context.base + entry.next), dst);
	}
};

struct open_cascade_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return string_processing::parse_open_cascade::impl(context.base + entry.quote, context.src_end, dst);
	}
};

struct jsonifier_current_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		const uint64_t length{ jsonifier::internal::string_parser::impl(context.base + entry.quote + 1, context.src_end, dst).outLength };
		return length == std::numeric_limits<uint64_t>::max() ? nullptr : dst + length;
	}
};

struct simdjson_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return context.simdjson_parser->parse_string(context.base + entry.quote + 1, dst, false);
	}
};

struct simdonifier_exact_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return string_processing::simdonifier_exact::impl(context.base + entry.quote, context.base + entry.end, dst);
	}
};

struct simdonifier_to_token_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return string_processing::simdonifier_exact::impl(context.base + entry.quote, string_processing::parse_to_token_cascade::find_end(context.base + entry.next), dst);
	}
};

struct simdonifier_open_raw {
	JSONIFIER_INLINE static uint8_t* impl(const kernel_context& context, const string_case& entry, uint8_t* dst) noexcept {
		return string_processing::simdonifier_open::impl(context.base + entry.quote, context.src_end, dst);
	}
};

template<typename parser_type> struct bounded_parse_context {
	const uint8_t* quote{};
	const uint8_t* end{};
	bool& failed;

	bounded_parse_context() noexcept = delete;

	JSONIFIER_INLINE bounded_parse_context(const uint8_t* quote_new, const uint8_t* end_new, bool& failed_new) noexcept : quote{ quote_new }, end{ end_new }, failed{ failed_new } {
	}

	bounded_parse_context& operator=(bounded_parse_context&&) noexcept		= delete;
	bounded_parse_context(bounded_parse_context&&) noexcept					= delete;
	bounded_parse_context& operator=(const bounded_parse_context&) noexcept = delete;
	bounded_parse_context(const bounded_parse_context&) noexcept			= delete;

	JSONIFIER_INLINE_EXCEPT_GCC uint64_t operator()(char* ptr, uint64_t) const noexcept {
		uint8_t* const begin{ std::bit_cast<uint8_t*>(ptr) };
		uint8_t* const finish{ parser_type::impl(quote, end, begin) };
		failed = finish == nullptr;
		return finish != nullptr ? static_cast<uint64_t>(finish - begin) : 0;
	}
};

template<typename parser_type, uint64_t slack> struct bounded_string_kernel {
	JSONIFIER_INLINE static bool impl(const uint8_t* quote, const uint8_t* end, std::string& out) noexcept {
		bool failed{};
		out.resize_and_overwrite(static_cast<uint64_t>(end - quote) - 2 + slack, bounded_parse_context<parser_type>{ quote, end, failed });
		return !failed;
	}
};

struct exact_cascade_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		return bounded_string_kernel<string_processing::parse_exact_cascade, 0>::impl(context.base + entry.quote, context.base + entry.end, out);
	}
};

struct to_token_cascade_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		const uint8_t* end{ string_processing::parse_to_token_cascade::find_end(context.base + entry.next) };
		return bounded_string_kernel<string_processing::parse_exact_cascade, 0>::impl(context.base + entry.quote, end, out);
	}
};

struct jsonifier_current_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		uint64_t length{};
		out.resize_and_overwrite(entry.end - entry.quote - 2 + 64, jsonifier::internal::string_parse_context{ context.base + entry.quote + 1, context.src_end, length });
		return length != std::numeric_limits<uint64_t>::max();
	}
};

template<typename raw_type> struct scratch_string_kernel {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		uint8_t* const finish{ raw_type::impl(context, entry, context.scratch) };
		if (finish == nullptr) {
			return false;
		}
		out.assign(std::bit_cast<const char*>(context.scratch), static_cast<uint64_t>(finish - context.scratch));
		return true;
	}
};

using open_cascade_string = scratch_string_kernel<open_cascade_raw>;

struct simdonifier_exact_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		return bounded_string_kernel<string_processing::simdonifier_exact, 0>::impl(context.base + entry.quote, context.base + entry.end, out);
	}
};

struct simdonifier_to_token_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		const uint8_t* end{ string_processing::parse_to_token_cascade::find_end(context.base + entry.next) };
		return bounded_string_kernel<string_processing::simdonifier_exact, 0>::impl(context.base + entry.quote, end, out);
	}
};

struct plain_copy_context {
	const uint8_t* src{};
	uint64_t length{};

	JSONIFIER_INLINE_EXCEPT_GCC uint64_t operator()(char* ptr, uint64_t) const noexcept {
		string_processing::cascade_copy::impl(src, std::bit_cast<uint8_t*>(ptr), length);
		return length;
	}
};

struct simdonifier_open_cold {
	BNCH_SWT_NOINLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		return scratch_string_kernel<open_cascade_raw>::impl(context, entry, out);
	}
};

struct simdonifier_open_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		const uint8_t* const start{ context.base + entry.quote + 1 };
		const uint8_t* stop{ start };
		const string_processing::scan_status status{ string_processing::open_scan::impl(stop, context.src_end) };
		if (status == string_processing::scan_status::done) {
			const uint64_t length{ static_cast<uint64_t>(stop - start) };
			out.resize_and_overwrite(length, plain_copy_context{ start, length });
			return true;
		}
		if (status == string_processing::scan_status::escape) {
			return simdonifier_open_cold::impl(context, entry, out);
		}
		return false;
	}
};

struct simdjson_from_string {
	JSONIFIER_INLINE static bool impl(const kernel_context& context, const string_case& entry, std::string& out) noexcept {
		const uint8_t* const quote{ context.base + entry.quote };
		const simdjson::padded_string_view view{ std::bit_cast<const char*>(quote), entry.end - entry.quote, static_cast<uint64_t>(context.src_end - quote) };
		return simdjson::from(*context.ondemand_parser, view).get(out) == simdjson::SUCCESS;
	}
};

template<typename kernel_type> struct raw_corpus_runner {
	BNCH_SWT_HOST static uint64_t impl(bin_data& data) {
		const kernel_context context{ data };
		uint8_t* dst{ context.scratch };
		uint64_t failures{};
		for (const string_case& entry: data.cases) {
			uint8_t* next{ kernel_type::impl(context, entry, dst) };
			failures += next == nullptr;
			dst = next != nullptr ? next : dst;
		}
		benchmarksuite::do_not_optimize_away(dst);
		benchmarksuite::do_not_optimize_away(failures);
		return data.byte_count;
	}
};

template<typename kernel_type> struct string_corpus_runner {
	BNCH_SWT_HOST static uint64_t impl(bin_data& data) {
		const kernel_context context{ data };
		uint64_t failures{};
		for (uint64_t x = 0; x < data.cases.size(); ++x) {
			std::string value{};
			failures += !kernel_type::impl(context, data.cases[x], value);
			data.strings[x] = std::move(value);
		}
		benchmarksuite::do_not_optimize_away(failures);
		return data.byte_count;
	}
};

template<typename kernel_type> struct raw_collector {
	BNCH_SWT_NOINLINE static std::string impl(bin_data& data, uint64_t& failures) {
		std::vector<uint8_t> buffer(data.output.size());
		const kernel_context context{ data };
		uint8_t* dst{ buffer.data() };
		for (const string_case& entry: data.cases) {
			uint8_t* next{ kernel_type::impl(context, entry, dst) };
			failures += next == nullptr;
			dst = next != nullptr ? next : dst;
		}
		return std::string{ std::bit_cast<const char*>(buffer.data()), static_cast<uint64_t>(dst - buffer.data()) };
	}
};

template<typename kernel_type> struct string_collector {
	BNCH_SWT_NOINLINE static std::string impl(bin_data& data, uint64_t& failures) {
		const kernel_context context{ data };
		std::string output{};
		for (const string_case& entry: data.cases) {
			std::string value{};
			failures += !kernel_type::impl(context, entry, value);
			output += value;
		}
		return output;
	}
};

template<typename collector_type, benchmarksuite::string_literal kernel_name> struct kernel_verifier {
	BNCH_SWT_NOINLINE static void impl(bin_data& data, std::string_view test_name, const std::string& reference) {
		uint64_t failures{};
		const std::string output{ collector_type::impl(data, failures) };
		const bool matches{ output == reference };
		std::cout << "verify " << test_name << " " << kernel_name.operator std::string_view() << ": " << (failures == 0 && matches ? "ok" : "MISMATCH") << " (failures " << failures
				  << ", bytes " << output.size() << " vs " << reference.size() << ")" << std::endl;
	}
};

enum class bin_kind : uint64_t {
	mixed,
	ascii_0_15,
	ascii_16_31,
	ascii_32_63,
	ascii_64_plus,
	unicode,
	escaped,
	apache_builds,
	canada,
	citm_catalog,
	discord,
	github_events,
	google_maps,
	instruments,
	marine_ik,
	random,
	string_test,
	twitter,
	count,
};

static constexpr uint64_t bin_count{ static_cast<uint64_t>(bin_kind::count) };

struct corpus_entry {
	std::string body;
	std::string gap;
	bin_kind corpus{};
};

struct corpus_name {
	std::string_view stem;
	bin_kind kind{};
};

struct corpus_classifier {
	static constexpr std::array<corpus_name, 11> names{ { { "apache_builds_test", bin_kind::apache_builds }, { "canada_test", bin_kind::canada },
		{ "citmcatalog_test", bin_kind::citm_catalog }, { "discord_test", bin_kind::discord }, { "github_events_test", bin_kind::github_events },
		{ "google_maps_response_test", bin_kind::google_maps }, { "instruments_test", bin_kind::instruments }, { "marine_ik_test", bin_kind::marine_ik },
		{ "random_test", bin_kind::random }, { "string_test", bin_kind::string_test }, { "twitter_test", bin_kind::twitter } } };

	BNCH_SWT_NOINLINE static bin_kind impl(const std::string& stem) {
		for (const corpus_name& name: names) {
			if (name.stem == stem) {
				return name.kind;
			}
		}
		return bin_kind::count;
	}
};

struct corpus_loader {
	BNCH_SWT_NOINLINE static std::string expand_gap(std::string_view code) {
		std::string gap{};
		for (const char value: code) {
			switch (value) {
				case 's':
					gap.push_back(' ');
					break;
				case 'n':
					gap.push_back('\n');
					break;
				case 'r':
					gap.push_back('\r');
					break;
				case 't':
					gap.push_back('\t');
					break;
				default:
					gap.push_back(value);
					break;
			}
		}
		if (gap.empty() || gap.back() == ' ' || gap.back() == '\n' || gap.back() == '\r' || gap.back() == '\t') {
			gap.push_back(',');
		}
		return gap;
	}

	BNCH_SWT_NOINLINE static std::vector<corpus_entry> impl() {
		std::vector<std::filesystem::path> paths{};
		for (const std::filesystem::directory_entry& file: std::filesystem::directory_iterator{ BNCH_SWT_STRING_CORPUS_DIR }) {
			if (file.path().extension() == ".txt") {
				paths.emplace_back(file.path());
			}
		}
		std::sort(paths.begin(), paths.end());
		std::vector<corpus_entry> entries{};
		for (const std::filesystem::path& path: paths) {
			const bin_kind corpus{ corpus_classifier::impl(path.stem().string()) };
			std::ifstream stream{ path, std::ios::binary };
			std::string line{};
			while (std::getline(stream, line)) {
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				const uint64_t tab{ line.find('\t') };
				if (tab == std::string::npos) {
					continue;
				}
				entries.emplace_back(corpus_entry{ line.substr(tab + 1), expand_gap(std::string_view{ line }.substr(0, tab)), corpus });
			}
		}
		return entries;
	}
};

struct bin_classifier {
	BNCH_SWT_NOINLINE static bin_kind impl(std::string_view body) {
		if (body.find('\\') != std::string_view::npos) {
			return bin_kind::escaped;
		}
		for (const char value: body) {
			if (static_cast<uint8_t>(value) >= 0x80u) {
				return bin_kind::unicode;
			}
		}
		if (body.size() < 16) {
			return bin_kind::ascii_0_15;
		}
		if (body.size() < 32) {
			return bin_kind::ascii_16_31;
		}
		if (body.size() < 64) {
			return bin_kind::ascii_32_63;
		}
		return bin_kind::ascii_64_plus;
	}
};

struct bin_builder {
	BNCH_SWT_NOINLINE static bin_data build(std::vector<const corpus_entry*> entries, uint64_t seed) {
		std::mt19937_64 engine{ seed };
		std::shuffle(entries.begin(), entries.end(), engine);
		bin_data data{};
		data.input.assign(string_processing::input_padding, ' ');
		for (const corpus_entry* entry: entries) {
			string_case value{};
			value.quote = data.input.size();
			data.input.push_back('"');
			data.input.insert(data.input.end(), entry->body.begin(), entry->body.end());
			data.input.push_back('"');
			value.end  = data.input.size();
			value.next = value.end + entry->gap.size() - 1;
			data.input.insert(data.input.end(), entry->gap.begin(), entry->gap.end());
			data.byte_count += entry->body.size();
			data.cases.emplace_back(value);
		}
		data.input.insert(data.input.end(), string_processing::input_padding, ' ');
		data.output.resize(data.input.size() + string_processing::input_padding);
		data.strings.resize(data.cases.size());
		return data;
	}

	BNCH_SWT_NOINLINE static std::array<bin_data, bin_count> impl(const std::vector<corpus_entry>& corpus) {
		std::array<std::vector<const corpus_entry*>, bin_count> grouped{};
		for (const corpus_entry& entry: corpus) {
			grouped[static_cast<uint64_t>(bin_classifier::impl(entry.body))].emplace_back(&entry);
			grouped[static_cast<uint64_t>(bin_kind::mixed)].emplace_back(&entry);
			if (entry.corpus != bin_kind::count) {
				grouped[static_cast<uint64_t>(entry.corpus)].emplace_back(&entry);
			}
		}
		std::array<bin_data, bin_count> bins{};
		for (uint64_t x = 0; x < bin_count; ++x) {
			bins[x] = build(grouped[x], 0x5EEDull + x);
		}
		return bins;
	}
};

using stage_type = benchmarksuite::benchmark_stage<"string-processing", benchmarksuite::stage_config_data{ .max_time_in_s = 3, .include_non_converged_results = true }>;

template<bin_kind kind, benchmarksuite::string_literal raw_name, benchmarksuite::string_literal string_name> struct bin_runner {
	BNCH_SWT_NOINLINE static void impl(std::array<bin_data, bin_count>& bins) {
		bin_data& data{ bins[static_cast<uint64_t>(kind)] };
		if (data.cases.empty()) {
			return;
		}
		const std::string_view raw_test{ raw_name.operator std::string_view() };
		const std::string_view string_test{ string_name.operator std::string_view() };
		uint64_t reference_failures{};
		const std::string reference{ raw_collector<simdjson_raw>::impl(data, reference_failures) };
		std::cout << raw_test << ": " << data.cases.size() << " strings, " << data.byte_count << " bytes, simdjson failures " << reference_failures << std::endl;
		kernel_verifier<raw_collector<exact_cascade_raw>, "jsonifier-exact-cascade">::impl(data, raw_test, reference);
		kernel_verifier<raw_collector<to_token_cascade_raw>, "jsonifier-to-token-cascade">::impl(data, raw_test, reference);
		kernel_verifier<raw_collector<open_cascade_raw>, "jsonifier-open-cascade">::impl(data, raw_test, reference);
		kernel_verifier<raw_collector<jsonifier_current_raw>, "jsonifier-current">::impl(data, raw_test, reference);
		kernel_verifier<raw_collector<simdonifier_exact_raw>, "simdonifier-exact-cascade">::impl(data, raw_test, reference);
		kernel_verifier<raw_collector<simdonifier_to_token_raw>, "simdonifier-to-token-cascade">::impl(data, raw_test, reference);
		kernel_verifier<raw_collector<simdonifier_open_raw>, "simdonifier-open-cascade">::impl(data, raw_test, reference);
		kernel_verifier<string_collector<exact_cascade_string>, "jsonifier-exact-cascade">::impl(data, string_test, reference);
		kernel_verifier<string_collector<to_token_cascade_string>, "jsonifier-to-token-cascade">::impl(data, string_test, reference);
		kernel_verifier<string_collector<open_cascade_string>, "jsonifier-open-cascade">::impl(data, string_test, reference);
		kernel_verifier<string_collector<jsonifier_current_string>, "jsonifier-current">::impl(data, string_test, reference);
		kernel_verifier<string_collector<simdonifier_exact_string>, "simdonifier-exact-cascade">::impl(data, string_test, reference);
		kernel_verifier<string_collector<simdonifier_to_token_string>, "simdonifier-to-token-cascade">::impl(data, string_test, reference);
		kernel_verifier<string_collector<simdonifier_open_string>, "simdonifier-open-cascade">::impl(data, string_test, reference);
		kernel_verifier<string_collector<simdjson_from_string>, "simdjson-from">::impl(data, string_test, reference);
		stage_type::run_benchmark<raw_name, "jsonifier-exact-cascade", raw_corpus_runner<exact_cascade_raw>>(data);
		stage_type::run_benchmark<raw_name, "jsonifier-to-token-cascade", raw_corpus_runner<to_token_cascade_raw>>(data);
		stage_type::run_benchmark<raw_name, "jsonifier-open-cascade", raw_corpus_runner<open_cascade_raw>>(data);
		stage_type::run_benchmark<raw_name, "jsonifier-current", raw_corpus_runner<jsonifier_current_raw>>(data);
		stage_type::run_benchmark<raw_name, "simdonifier-exact-cascade", raw_corpus_runner<simdonifier_exact_raw>>(data);
		stage_type::run_benchmark<raw_name, "simdonifier-to-token-cascade", raw_corpus_runner<simdonifier_to_token_raw>>(data);
		stage_type::run_benchmark<raw_name, "simdonifier-open-cascade", raw_corpus_runner<simdonifier_open_raw>>(data);
		stage_type::run_benchmark<raw_name, "simdjson", raw_corpus_runner<simdjson_raw>>(data);
		std::cout << stage_type::get_test_results(raw_test).to_markdown() << std::endl;
		stage_type::run_benchmark<string_name, "jsonifier-exact-cascade", string_corpus_runner<exact_cascade_string>>(data);
		stage_type::run_benchmark<string_name, "jsonifier-to-token-cascade", string_corpus_runner<to_token_cascade_string>>(data);
		stage_type::run_benchmark<string_name, "jsonifier-open-cascade", string_corpus_runner<open_cascade_string>>(data);
		stage_type::run_benchmark<string_name, "jsonifier-current", string_corpus_runner<jsonifier_current_string>>(data);
		stage_type::run_benchmark<string_name, "simdonifier-exact-cascade", string_corpus_runner<simdonifier_exact_string>>(data);
		stage_type::run_benchmark<string_name, "simdonifier-to-token-cascade", string_corpus_runner<simdonifier_to_token_string>>(data);
		stage_type::run_benchmark<string_name, "simdonifier-open-cascade", string_corpus_runner<simdonifier_open_string>>(data);
		stage_type::run_benchmark<string_name, "simdjson-from", string_corpus_runner<simdjson_from_string>>(data);
		std::cout << stage_type::get_test_results(string_test).to_markdown() << std::endl;
	}
};

int32_t main() {
	benchmarksuite::pin_for_benchmark();
	const simdjson::implementation* simdjson_active{ simdjson::get_active_implementation() };
	std::unique_ptr<simdjson::internal::dom_parser_implementation> simdjson_parser{};
	if (simdjson_active->create_dom_parser_implementation(1ull << 20, 1024, simdjson_parser) != simdjson::SUCCESS) {
		return 1;
	}
	simdjson::ondemand::parser ondemand_parser{};
	std::cout << "simdjson implementation: " << simdjson_active->name() << ", jsonifier: " << jsonifier::cpuArchName << std::endl;
	const std::vector<corpus_entry> corpus{ corpus_loader::impl() };
	std::array<bin_data, bin_count> bins{ bin_builder::impl(corpus) };
	for (bin_data& data: bins) {
		data.simdjson_parser = simdjson_parser.get();
		data.ondemand_parser = &ondemand_parser;
	}
	bin_runner<bin_kind::mixed, "strings-mixed-raw", "strings-mixed-std-string">::impl(bins);
	bin_runner<bin_kind::ascii_0_15, "strings-ascii-0-15-raw", "strings-ascii-0-15-std-string">::impl(bins);
	bin_runner<bin_kind::ascii_16_31, "strings-ascii-16-31-raw", "strings-ascii-16-31-std-string">::impl(bins);
	bin_runner<bin_kind::ascii_32_63, "strings-ascii-32-63-raw", "strings-ascii-32-63-std-string">::impl(bins);
	bin_runner<bin_kind::ascii_64_plus, "strings-ascii-64-plus-raw", "strings-ascii-64-plus-std-string">::impl(bins);
	bin_runner<bin_kind::unicode, "strings-unicode-raw", "strings-unicode-std-string">::impl(bins);
	bin_runner<bin_kind::escaped, "strings-escaped-raw", "strings-escaped-std-string">::impl(bins);
	bin_runner<bin_kind::apache_builds, "strings-apache-builds-raw", "strings-apache-builds-std-string">::impl(bins);
	bin_runner<bin_kind::canada, "strings-canada-raw", "strings-canada-std-string">::impl(bins);
	bin_runner<bin_kind::citm_catalog, "strings-citm-catalog-raw", "strings-citm-catalog-std-string">::impl(bins);
	bin_runner<bin_kind::discord, "strings-discord-raw", "strings-discord-std-string">::impl(bins);
	bin_runner<bin_kind::github_events, "strings-github-events-raw", "strings-github-events-std-string">::impl(bins);
	bin_runner<bin_kind::google_maps, "strings-google-maps-raw", "strings-google-maps-std-string">::impl(bins);
	bin_runner<bin_kind::instruments, "strings-instruments-raw", "strings-instruments-std-string">::impl(bins);
	bin_runner<bin_kind::marine_ik, "strings-marine-ik-raw", "strings-marine-ik-std-string">::impl(bins);
	bin_runner<bin_kind::random, "strings-random-raw", "strings-random-std-string">::impl(bins);
	bin_runner<bin_kind::string_test, "strings-string-test-raw", "strings-string-test-std-string">::impl(bins);
	bin_runner<bin_kind::twitter, "strings-twitter-raw", "strings-twitter-std-string">::impl(bins);
	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return 0;
}
