/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/main.cpp
 */

#include <benchmarksuite>
#include <bit>
#include <cstring>
#include <random>
#include <vector>

static constexpr uint64_t padding_bytes{ 64 };
static constexpr uint64_t token_count{ 1ull << 16 };

namespace templated_approach {

	template<uint64_t size_val>
		requires(size_val == 5)
	struct string_literal {
		using value_type	= char;
		using const_pointer = const value_type*;
		using size_type		= uint64_t;
		static constexpr size_type length{ size_val - 1 };

		BNCH_SWT_HOST static constexpr uint32_t shift_for(size_type index) noexcept {
			return static_cast<uint32_t>((std::endian::native == std::endian::little ? index : length - 1 - index) * 8);
		}

		BNCH_SWT_HOST constexpr string_literal() noexcept = default;

		BNCH_SWT_HOST constexpr string_literal(const value_type (&str)[size_val]) noexcept {
			for (size_type x = 0; x < length; ++x) {
				value |= static_cast<uint32_t>(static_cast<uint8_t>(str[x])) << shift_for(x);
			}
		}

		BNCH_SWT_HOST constexpr bool operator==(const string_literal&) const noexcept = default;

		uint32_t value{};
	};

	template<string_literal string> BNCH_SWT_HOST consteval uint32_t string_to_uint32() {
		return string.value;
	}

	template<string_literal atom> BNCH_SWT_HOST uint32_t str4ncmp(const uint8_t* src) {
		uint32_t srcval;
		std::memcpy(&srcval, src, sizeof(uint32_t));
		return srcval ^ string_to_uint32<atom>();
	}

}

namespace runtime_approach {

	BNCH_SWT_HOST uint32_t string_to_uint32(const char* str) {
		uint32_t val;
		std::memcpy(&val, str, sizeof(uint32_t));
		return val;
	}

	BNCH_SWT_HOST uint32_t str4ncmp(const uint8_t* src, const char* atom) {
		uint32_t srcval;
		std::memcpy(&srcval, src, sizeof(uint32_t));
		return srcval ^ string_to_uint32(atom);
	}

}

template<templated_approach::string_literal atom> struct atom_chars {
	static constexpr char value[5]{ static_cast<char>(atom.value >> templated_approach::string_literal<5>::shift_for(0)),
		static_cast<char>(atom.value >> templated_approach::string_literal<5>::shift_for(1)), static_cast<char>(atom.value >> templated_approach::string_literal<5>::shift_for(2)),
		static_cast<char>(atom.value >> templated_approach::string_literal<5>::shift_for(3)), '\0' };
};

template<templated_approach::string_literal atom> struct templated_scan {
	BNCH_SWT_HOST static uint64_t impl(const std::vector<uint8_t>& buffer) {
		const uint8_t* data{ buffer.data() };
		uint64_t matches{};
		for (uint64_t x = 0; x < token_count; ++x) {
			matches += templated_approach::str4ncmp<atom>(data + x * 4) == 0;
		}
		benchmarksuite::do_not_optimize_away(matches);
		return token_count * 4;
	}
};

template<templated_approach::string_literal atom> struct runtime_scan {
	BNCH_SWT_HOST static uint64_t impl(const std::vector<uint8_t>& buffer) {
		const uint8_t* data{ buffer.data() };
		uint64_t matches{};
		for (uint64_t x = 0; x < token_count; ++x) {
			matches += runtime_approach::str4ncmp(data + x * 4, atom_chars<atom>::value) == 0;
		}
		benchmarksuite::do_not_optimize_away(matches);
		return token_count * 4;
	}
};

static std::vector<uint8_t> generate_buffer() {
	static constexpr const char* tokens[]{ "true", "fals", "null", "nan ", "1234", "abcd", "tru ", "nul\"" };
	std::mt19937_64 engine{ std::random_device{}() };
	std::uniform_int_distribution<uint64_t> pick{ 0, std::size(tokens) - 1 };
	std::vector<uint8_t> buffer(token_count * 4 + padding_bytes);
	for (uint64_t x = 0; x < token_count; ++x) {
		std::memcpy(buffer.data() + x * 4, tokens[pick(engine)], 4);
	}
	return buffer;
}

using stage_type = benchmarksuite::benchmark_stage<"str4ncmp", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_time_in_s = 5 }>;

template<templated_approach::string_literal atom, benchmarksuite::string_literal test_name> struct run_atom {
	BNCH_SWT_HOST static void impl(const std::vector<uint8_t>& buffer) {
		stage_type::run_benchmark<test_name, "templated-consteval", templated_scan<atom>>(buffer);
		stage_type::run_benchmark<test_name, "runtime-memcpy", runtime_scan<atom>>(buffer);
		std::cout << stage_type::get_test_results(test_name.operator std::string_view()).to_markdown() << std::endl;
	}
};

int32_t main() {
	benchmarksuite::pin_for_benchmark();
	const std::vector<uint8_t> buffer{ generate_buffer() };
	run_atom<"true", "str4ncmp-true">::impl(buffer);
	run_atom<"fals", "str4ncmp-fals">::impl(buffer);
	run_atom<"null", "str4ncmp-null">::impl(buffer);
	run_atom<"nan ", "str4ncmp-nan">::impl(buffer);
	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return 0;
}
