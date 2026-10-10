/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/main.cpp
 */

#include <benchmarksuite>
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wall"
	#pragma GCC diagnostic ignored "-Wextra"
	#pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#include "fast_float.h"
#if defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
	#pragma warning(pop)
#endif
#include "jsonifier_fast_float.hpp"
#include <string_view>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <bit>

struct number_range {
	size_t offset{};
	size_t length{};
};

struct corpus_data {
	std::vector<number_range> ranges{};
	std::vector<double> results{};
	std::string buffer{};
	uint64_t volume{};
};

struct baseline_parser {
	BNCH_SWT_HOST static std::pair<bool, char const*> impl(char const* first, char const* last, double& value) {
		auto result = fast_float::from_chars(first, last, value, fast_float::chars_format::json);
		return { result.ec == std::errc{}, result.ptr };
	}
};

struct jsonifier_parser {
	BNCH_SWT_HOST static std::pair<bool, char const*> impl(char const* first, char const* last, double& value) {
		auto result = jsonifier_fast_float::from_chars(first, last, value);
		return { result.ec == std::errc{}, result.ptr };
	}
};

template<typename parser_type> struct corpus_parse {
	BNCH_SWT_HOST static uint64_t impl(corpus_data& data) {
		char const* const base	= data.buffer.data();
		double* const results	= data.results.data();
		size_t const count		= data.ranges.size();
		uint64_t failure_count{};
		for (size_t index{}; index < count; ++index) {
			char const* const first = base + data.ranges[index].offset;
			char const* const last	= first + data.ranges[index].length;
			failure_count += static_cast<uint64_t>(!parser_type::impl(first, last, results[index]).first);
		}
		benchmarksuite::do_not_optimize_away(failure_count);
		benchmarksuite::do_not_optimize_away(data.results);
		return data.volume;
	}
};

BNCH_SWT_HOST corpus_data load_corpus(std::string const& path) {
	corpus_data data{};
	std::ifstream input_file{ path };
	if (!input_file) {
		std::cerr << "Failed to open corpus file: " << path << std::endl;
		return data;
	}
	std::string line{};
	while (std::getline(input_file, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		data.ranges.push_back(number_range{ data.buffer.size(), line.size() });
		data.buffer += line;
		data.volume += line.size();
	}
	data.results.resize(data.ranges.size());
	return data;
}

BNCH_SWT_HOST bool verify_corpus(std::string_view corpus_name, corpus_data const& data) {
	constexpr size_t max_reported_mismatches{ 8 };
	char const* const base = data.buffer.data();
	size_t mismatch_count{};
	size_t rejected_count{};
	for (size_t index{}; index < data.ranges.size(); ++index) {
		char const* const first = base + data.ranges[index].offset;
		char const* const last	= first + data.ranges[index].length;
		double baseline_value{};
		double jsonifier_value{};
		auto const baseline_result	 = baseline_parser::impl(first, last, baseline_value);
		auto const jsonifier_result	 = jsonifier_parser::impl(first, last, jsonifier_value);
		rejected_count += static_cast<size_t>(!baseline_result.first);
		bool const same_status = baseline_result.first == jsonifier_result.first;
		bool const same_end	   = baseline_result.second == jsonifier_result.second;
		bool const same_bits   = std::bit_cast<uint64_t>(baseline_value) == std::bit_cast<uint64_t>(jsonifier_value);
		if (!(same_status && same_end && same_bits)) {
			if (mismatch_count < max_reported_mismatches) {
				std::cout << "MISMATCH in " << corpus_name << " line " << index << ": " << std::string_view{ first, data.ranges[index].length }.substr(0, 64) << std::endl;
			}
			++mismatch_count;
		}
	}
	std::cout << "verify " << corpus_name << ": " << data.ranges.size() << " numbers, " << rejected_count << " with non-ok status," << mismatch_count << " mismatches" << std::endl;
	return mismatch_count == 0;
}

using stage_type = benchmarksuite::benchmark_stage<"fast_float_json_stage", benchmarksuite::stage_config_data{ .max_time_in_s = 10, .include_non_converged_results = true }>;

template<benchmarksuite::string_literal corpus_name> BNCH_SWT_HOST void run_corpus(corpus_data& data) {
	stage_type::run_benchmark<corpus_name, "fast_float", corpus_parse<baseline_parser>>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float", corpus_parse<jsonifier_parser>>(data);
	auto rankings = stage_type::get_test_results(corpus_name.operator std::string_view());
	std::cout << rankings.to_markdown() << std::endl;
}

int32_t main() {
	std::string const corpus_directory{ BNCH_SWT_FAST_FLOAT_CORPUS_DIR };
	corpus_data canada		 = load_corpus(corpus_directory + "/canada.txt");
	corpus_data canada_short = load_corpus(corpus_directory + "/canada_short.txt");
	corpus_data mesh		 = load_corpus(corpus_directory + "/mesh.txt");
	corpus_data contrived	 = load_corpus(corpus_directory + "/contrived.txt");
	bool all_verified{ true };
	all_verified &= verify_corpus("canada", canada);
	all_verified &= verify_corpus("canada_short", canada_short);
	all_verified &= verify_corpus("mesh", mesh);
	all_verified &= verify_corpus("contrived", contrived);
	if (!all_verified) {
		std::cerr << "Correctness check failed, refusing to benchmark." << std::endl;
		return 1;
	}
	benchmarksuite::pin_for_benchmark();
	run_corpus<"canada">(canada);
	run_corpus<"canada_short">(canada_short);
	run_corpus<"mesh">(mesh);
	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return 0;
}
