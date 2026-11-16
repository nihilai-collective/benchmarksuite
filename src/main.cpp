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
#include <simdjson.h>
#if defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
	#pragma warning(pop)
#endif
#include "jsonifier_fast_float.hpp"
#include <string_view>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <array>
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

#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#endif
struct simdjson_parser {
	BNCH_SWT_HOST static std::pair<bool, char const*> impl(char const* first, char const*, double& value) {
		return { simdjson::builtin::numberparsing::parse_double(reinterpret_cast<uint8_t const*>(first)).get(value) == simdjson::SUCCESS, nullptr };
	}
};
#if defined(__clang__)
	#pragma clang diagnostic pop
#endif

struct jsonifier_parser {
	BNCH_SWT_HOST static std::pair<bool, char const*> impl(char const* first, char const* last, double& value) {
		auto result = jsonifier_fast_float::fromChars(first, last, value);
		return { result.ec == std::errc{}, result.ptr };
	}
};

struct jsonifier_predicted_parser {
	BNCH_SWT_HOST static std::pair<bool, char const*> impl(char const* first, char const* last, double& value, jsonifier_fast_float::number_shape_predictor& predictor) {
		auto result = jsonifier_fast_float::fromChars(first, last, value, predictor);
		return { result.ec == std::errc{}, result.ptr };
	}
};

struct corpus_parse_predicted {
	BNCH_SWT_HOST static uint64_t impl(corpus_data& data) {
		char const* const base = data.buffer.data();
		double* const results  = data.results.data();
		size_t const count	   = data.ranges.size();
		jsonifier_fast_float::number_shape_predictor predictor{};
		uint64_t failure_count{};
		for (size_t index{}; index < count; ++index) {
			char const* const first = base + data.ranges[index].offset;
			char const* const last	= first + data.ranges[index].length;
			failure_count += static_cast<uint64_t>(!jsonifier_predicted_parser::impl(first, last, results[index], predictor).first);
		}
		benchmarksuite::do_not_optimize_away(failure_count);
		benchmarksuite::do_not_optimize_away(data.results);
		return data.volume;
	}
};

template<jsonifier_fast_float::number_shape shape> struct locked_shape_loop {
	BNCH_SWT_NOINLINE static uint64_t impl(corpus_data& data, size_t start_index) {
		char const* const base = data.buffer.data();
		double* const results  = data.results.data();
		size_t const count	   = data.ranges.size();
		uint64_t failure_count{};
		for (size_t index{ start_index }; index < count; ++index) {
			char const* const first = base + data.ranges[index].offset;
			char const* const last	= first + data.ranges[index].length;
			jsonifier_fast_float::parsed_number_string pns{};
			auto const result = jsonifier_fast_float::fromCharsShaped<shape>(first, last, results[index], pns);
			failure_count += static_cast<uint64_t>(result.ec != std::errc{});
		}
		return failure_count;
	}
};

template<bool force_general> struct corpus_parse_locked {
	static constexpr size_t max_sample_scan{ 8 };

	BNCH_SWT_HOST static jsonifier_fast_float::number_shape sample_shape(corpus_data& data, size_t& sampled, uint64_t& failure_count) {
		using jsonifier_fast_float::number_shape;
		char const* const base	= data.buffer.data();
		double* const results	= data.results.data();
		size_t const scan_limit = std::min(max_sample_scan, data.ranges.size());
		for (sampled = 0; sampled < scan_limit;) {
			char const* const first = base + data.ranges[sampled].offset;
			char const* const last	= first + data.ranges[sampled].length;
			jsonifier_fast_float::parsed_number_string pns{};
			auto const result = jsonifier_fast_float::fromCharsShaped<number_shape::general>(first, last, results[sampled], pns);
			failure_count += static_cast<uint64_t>(result.ec != std::errc{});
			++sampled;
			if (pns.fraction.len() > 0) {
				return jsonifier_fast_float::number_shape_predictor::classify(pns.integer.len(), pns.fraction.len());
			}
		}
		return number_shape::general;
	}

	BNCH_SWT_HOST static uint64_t impl(corpus_data& data) {
		using jsonifier_fast_float::number_shape;
		size_t sampled{};
		uint64_t failure_count{};
		number_shape const detected = sample_shape(data, sampled, failure_count);
		switch (static_cast<uint64_t>(force_general ? number_shape::general : detected)) {
			case static_cast<uint64_t>(number_shape::short_fraction): {
				failure_count += locked_shape_loop<number_shape::short_fraction>::impl(data, sampled);
				break;
			}
			case static_cast<uint64_t>(number_shape::medium_fraction): {
				failure_count += locked_shape_loop<number_shape::medium_fraction>::impl(data, sampled);
				break;
			}
			case static_cast<uint64_t>(number_shape::long_fraction): {
				failure_count += locked_shape_loop<number_shape::long_fraction>::impl(data, sampled);
				break;
			}
			case static_cast<uint64_t>(number_shape::long_integer): {
				failure_count += locked_shape_loop<number_shape::long_integer>::impl(data, sampled);
				break;
			}
			default: {
				failure_count += locked_shape_loop<number_shape::general>::impl(data, sampled);
				break;
			}
		}
		benchmarksuite::do_not_optimize_away(failure_count);
		benchmarksuite::do_not_optimize_away(data.results);
		return data.volume;
	}
};

template<jsonifier_fast_float::performance_hints hints, jsonifier_fast_float::number_shape shape = jsonifier_fast_float::number_shape::general> struct jsonifier_hinted_parser {
	BNCH_SWT_HOST static std::pair<bool, char const*> impl(char const* first, char const* last, double& value) {
		auto result = jsonifier_fast_float::fromCharsHinted<hints, shape>(first, last, value);
		return { result.ec == std::errc{}, result.ptr };
	}
};

template<jsonifier_fast_float::performance_hints hints, jsonifier_fast_float::number_shape shape> struct hinted_loop {
	BNCH_SWT_NOINLINE static uint64_t impl(corpus_data& data, size_t start_index) {
		char const* const base = data.buffer.data();
		double* const results  = data.results.data();
		size_t const count	   = data.ranges.size();
		uint64_t failure_count{};
		for (size_t index{ start_index }; index < count; ++index) {
			char const* const first = base + data.ranges[index].offset;
			char const* const last	= first + data.ranges[index].length;
			failure_count += static_cast<uint64_t>(!jsonifier_hinted_parser<hints, shape>::impl(first, last, results[index]).first);
		}
		return failure_count;
	}
};

template<jsonifier_fast_float::performance_hints hints> struct hinted_shape_dispatch {
	BNCH_SWT_HOST static uint64_t impl(corpus_data& data, size_t start_index, jsonifier_fast_float::number_shape shape) {
		using jsonifier_fast_float::number_shape;
		switch (static_cast<uint64_t>(shape)) {
			case static_cast<uint64_t>(number_shape::short_fraction): {
				return hinted_loop<hints, number_shape::short_fraction>::impl(data, start_index);
			}
			case static_cast<uint64_t>(number_shape::medium_fraction): {
				return hinted_loop<hints, number_shape::medium_fraction>::impl(data, start_index);
			}
			case static_cast<uint64_t>(number_shape::long_fraction): {
				return hinted_loop<hints, number_shape::long_fraction>::impl(data, start_index);
			}
			case static_cast<uint64_t>(number_shape::long_integer): {
				return hinted_loop<hints, number_shape::long_integer>::impl(data, start_index);
			}
			default: {
				return hinted_loop<hints, number_shape::general>::impl(data, start_index);
			}
		}
	}
};

struct locked_shape_dispatch {
	BNCH_SWT_HOST static uint64_t impl(corpus_data& data, size_t start_index, jsonifier_fast_float::number_shape shape) {
		using jsonifier_fast_float::number_shape;
		switch (static_cast<uint64_t>(shape)) {
			case static_cast<uint64_t>(number_shape::short_fraction): {
				return locked_shape_loop<number_shape::short_fraction>::impl(data, start_index);
			}
			case static_cast<uint64_t>(number_shape::medium_fraction): {
				return locked_shape_loop<number_shape::medium_fraction>::impl(data, start_index);
			}
			case static_cast<uint64_t>(number_shape::long_fraction): {
				return locked_shape_loop<number_shape::long_fraction>::impl(data, start_index);
			}
			case static_cast<uint64_t>(number_shape::long_integer): {
				return locked_shape_loop<number_shape::long_integer>::impl(data, start_index);
			}
			default: {
				return locked_shape_loop<number_shape::general>::impl(data, start_index);
			}
		}
	}
};

struct sampled_number_shape {
	size_t fraction_digits{};
	size_t integer_digits{};
	size_t digit_length{};
	bool negative{};
};

struct sampled_plan {
	jsonifier_fast_float::performance_hints hints{ jsonifier_fast_float::performance_hints::none };
	jsonifier_fast_float::number_shape shape{ jsonifier_fast_float::number_shape::general };
};

struct corpus_parse_hinted {
	static constexpr size_t unpredictable_sign_threshold{ 4 };
	static constexpr size_t integer_change_threshold{ 4 };
	static constexpr size_t short_length_limit{ 8 };
	static constexpr size_t sample_count{ 32 };

	BNCH_SWT_HOST static sampled_number_shape describe(char const* first, size_t length) {
		sampled_number_shape shape{};
		shape.negative				 = length > 0 && first[0] == '-';
		shape.digit_length			 = length - static_cast<size_t>(shape.negative);
		std::string_view const text{ first, length };
		size_t const dot_position = text.find('.');
		bool const has_dot		  = dot_position != std::string_view::npos;
		shape.fraction_digits	  = has_dot ? length - dot_position - 1 : 0;
		shape.integer_digits	  = shape.digit_length - shape.fraction_digits - static_cast<size_t>(has_dot);
		return shape;
	}

	BNCH_SWT_HOST static sampled_plan sample_plan(corpus_data& data, size_t& sampled, uint64_t& failure_count) {
		using jsonifier_fast_float::number_shape;
		using jsonifier_fast_float::performance_hints;
		char const* const base = data.buffer.data();
		double* const results  = data.results.data();
		sampled				   = std::min(sample_count, data.ranges.size());
		size_t sign_surprises{};
		size_t integer_changes{};
		size_t short_count{};
		bool shape_found{};
		sampled_plan plan{};
		std::array<sampled_number_shape, sample_count> shapes{};
		for (size_t index{}; index < sampled; ++index) {
			char const* const first = base + data.ranges[index].offset;
			char const* const last	= first + data.ranges[index].length;
			jsonifier_fast_float::parsed_number_string pns{};
			auto const result = jsonifier_fast_float::fromCharsShaped<number_shape::general>(first, last, results[index], pns);
			failure_count += static_cast<uint64_t>(result.ec != std::errc{});
			if (!shape_found && pns.fraction.len() > 0) {
				plan.shape	= jsonifier_fast_float::number_shape_predictor::classify(pns.integer.len(), pns.fraction.len());
				shape_found = true;
			}
			shapes[index] = describe(first, data.ranges[index].length);
			short_count += static_cast<size_t>(shapes[index].digit_length <= short_length_limit);
			if (index >= 1) {
				integer_changes += static_cast<size_t>(shapes[index].integer_digits != shapes[index - 1].integer_digits);
			}
			if (index >= 2) {
				sign_surprises += static_cast<size_t>(shapes[index].negative != shapes[index - 1].negative && shapes[index].negative != shapes[index - 2].negative);
			}
		}
		if (sign_surprises >= unpredictable_sign_threshold) {
			plan.hints = performance_hints::unpredictable_sign;
		} else if (short_count * 4 >= sampled * 3 && integer_changes >= integer_change_threshold) {
			plan.hints = performance_hints::short_numbers;
		}
		return plan;
	}

	BNCH_SWT_HOST static uint64_t impl(corpus_data& data) {
		using jsonifier_fast_float::number_shape;
		using jsonifier_fast_float::performance_hints;
		size_t sampled{};
		uint64_t failure_count{};
		sampled_plan const plan = sample_plan(data, sampled, failure_count);
		switch (static_cast<uint64_t>(plan.hints)) {
			case static_cast<uint64_t>(performance_hints::unpredictable_sign): {
				failure_count += hinted_shape_dispatch<performance_hints::unpredictable_sign>::impl(data, sampled, plan.shape);
				break;
			}
			case static_cast<uint64_t>(performance_hints::short_numbers): {
				failure_count += hinted_loop<performance_hints::short_numbers, number_shape::general>::impl(data, sampled);
				break;
			}
			default: {
				failure_count += locked_shape_dispatch::impl(data, sampled, plan.shape);
				break;
			}
		}
		benchmarksuite::do_not_optimize_away(failure_count);
		benchmarksuite::do_not_optimize_away(data.results);
		return data.volume;
	}
};

template<jsonifier_fast_float::performance_hints hints, jsonifier_fast_float::number_shape shape = jsonifier_fast_float::number_shape::general> struct hinted_matches_baseline {
	BNCH_SWT_NOINLINE static bool impl(char const* first, char const* last, std::pair<bool, char const*> baseline_result, double baseline_value) {
		double hinted_value{};
		auto const result = jsonifier_hinted_parser<hints, shape>::impl(first, last, hinted_value);
		return result.first == baseline_result.first && result.second == baseline_result.second &&
			std::bit_cast<uint64_t>(hinted_value) == std::bit_cast<uint64_t>(baseline_value);
	}
};

template<jsonifier_fast_float::number_shape shape> struct shape_matches_baseline {
	BNCH_SWT_HOST static bool impl(char const* first, char const* last, std::pair<bool, char const*> baseline_result, double baseline_value) {
		double shaped_value{};
		jsonifier_fast_float::parsed_number_string pns{};
		auto const result = jsonifier_fast_float::fromCharsShaped<shape>(first, last, shaped_value, pns);
		return (result.ec == std::errc{}) == baseline_result.first && result.ptr == baseline_result.second &&
			std::bit_cast<uint64_t>(shaped_value) == std::bit_cast<uint64_t>(baseline_value);
	}
};

template<typename parser_type> struct corpus_parse {
	BNCH_SWT_HOST static uint64_t impl(corpus_data& data) {
		char const* const base = data.buffer.data();
		double* const results  = data.results.data();
		size_t const count	   = data.ranges.size();
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

BNCH_SWT_NOINLINE corpus_data load_corpus(std::string const& path) {
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
		data.buffer += '\n';
		data.volume += line.size();
	}
	data.buffer.append(simdjson::SIMDJSON_PADDING, '\0');
	data.results.resize(data.ranges.size());
	return data;
}

BNCH_SWT_NOINLINE bool verify_corpus(std::string_view corpus_name, corpus_data const& data) {
	constexpr size_t max_reported_mismatches{ 8 };
	char const* const base = data.buffer.data();
	jsonifier_fast_float::number_shape_predictor predictor{};
	size_t simdjson_status_differences{};
	size_t simdjson_value_differences{};
	size_t mismatch_count{};
	size_t rejected_count{};
	for (size_t index{}; index < data.ranges.size(); ++index) {
		char const* const first = base + data.ranges[index].offset;
		char const* const last	= first + data.ranges[index].length;
		double baseline_value{};
		double jsonifier_value{};
		double predicted_value{};
		double simdjson_value{};
		auto const simdjson_result	= simdjson_parser::impl(first, last, simdjson_value);
		auto const baseline_result	= baseline_parser::impl(first, last, baseline_value);
		auto const jsonifier_result = jsonifier_parser::impl(first, last, jsonifier_value);
		auto const predicted_result = jsonifier_predicted_parser::impl(first, last, predicted_value, predictor);
		rejected_count += static_cast<size_t>(!baseline_result.first);
		simdjson_status_differences += static_cast<size_t>(simdjson_result.first != baseline_result.first);
		simdjson_value_differences +=
			static_cast<size_t>(simdjson_result.first && baseline_result.first && std::bit_cast<uint64_t>(simdjson_value) != std::bit_cast<uint64_t>(baseline_value));
		bool const same_status = baseline_result.first == jsonifier_result.first && baseline_result.first == predicted_result.first;
		bool const same_end	   = baseline_result.second == jsonifier_result.second && baseline_result.second == predicted_result.second;
		bool const same_bits   = std::bit_cast<uint64_t>(baseline_value) == std::bit_cast<uint64_t>(jsonifier_value) &&
			std::bit_cast<uint64_t>(baseline_value) == std::bit_cast<uint64_t>(predicted_value);
		using jsonifier_fast_float::number_shape;
		bool const every_shape_matches = shape_matches_baseline<number_shape::general>::impl(first, last, baseline_result, baseline_value) &&
			shape_matches_baseline<number_shape::short_fraction>::impl(first, last, baseline_result, baseline_value) &&
			shape_matches_baseline<number_shape::medium_fraction>::impl(first, last, baseline_result, baseline_value) &&
			shape_matches_baseline<number_shape::long_fraction>::impl(first, last, baseline_result, baseline_value) &&
			shape_matches_baseline<number_shape::long_integer>::impl(first, last, baseline_result, baseline_value);
		using jsonifier_fast_float::performance_hints;
		bool const every_hint_matches = hinted_matches_baseline<performance_hints::none>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::unpredictable_sign>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::short_numbers>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::unpredictable_sign | performance_hints::short_numbers>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::none, number_shape::short_fraction>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::unpredictable_sign, number_shape::short_fraction>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::unpredictable_sign, number_shape::medium_fraction>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::unpredictable_sign, number_shape::long_fraction>::impl(first, last, baseline_result, baseline_value) &&
			hinted_matches_baseline<performance_hints::unpredictable_sign, number_shape::long_integer>::impl(first, last, baseline_result, baseline_value);
		if (!(same_status && same_end && same_bits && every_shape_matches && every_hint_matches)) {
			if (mismatch_count < max_reported_mismatches) {
				std::cout << "MISMATCH in " << corpus_name << " line " << index << ": " << std::string_view{ first, data.ranges[index].length }.substr(0, 64) << std::endl;
			}
			++mismatch_count;
		}
	}
	std::cout << "verify " << corpus_name << ": " << data.ranges.size() << " numbers, " << rejected_count << " with non-ok status," << mismatch_count << " mismatches" << std::endl;
	std::cout << "simdjson " << corpus_name << ": " << simdjson_status_differences << " status differences, " << simdjson_value_differences << " value differences" << std::endl;
	return mismatch_count == 0;
}

using stage_type = benchmarksuite::benchmark_stage<"fast_float_json_stage", benchmarksuite::stage_config_data{ [] {
	benchmarksuite::stage_config_data return_data{};
	return_data.include_non_converged_results = true;
	return_data.convergence_threshold		  = 5;
	return_data.max_time_in_s				  = 10;
	return_data.rse_threshold				  = 10;
	return return_data;
}() }>;

template<benchmarksuite::string_literal corpus_name> BNCH_SWT_NOINLINE void run_corpus(corpus_data& data) {
	stage_type::run_benchmark<corpus_name, "fast_float", corpus_parse<baseline_parser>>(data);
	stage_type::run_benchmark<corpus_name, "simdjson", corpus_parse<simdjson_parser>>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float", corpus_parse<jsonifier_parser>>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float_locked", corpus_parse_locked<false>>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float_locked_general", corpus_parse_locked<true>>(data);
	using jsonifier_fast_float::performance_hints;
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float_hinted", corpus_parse_hinted>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float_hinted_sign", corpus_parse<jsonifier_hinted_parser<performance_hints::unpredictable_sign>>>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float_hinted_short", corpus_parse<jsonifier_hinted_parser<performance_hints::short_numbers>>>(data);
	stage_type::run_benchmark<corpus_name, "jsonifier_fast_float_hinted_sign_short",
		corpus_parse<jsonifier_hinted_parser<performance_hints::unpredictable_sign | performance_hints::short_numbers>>>(data);
	auto rankings = stage_type::get_test_results(corpus_name.operator std::string_view());
	std::cout << rankings.to_markdown() << std::endl;
}

int32_t main() {
	std::string const corpus_directory{ BNCH_SWT_FAST_FLOAT_CORPUS_DIR };
	corpus_data canada		 = load_corpus(corpus_directory + "/canada.txt");
	corpus_data canada_short = load_corpus(corpus_directory + "/canada_short.txt");
	corpus_data mesh		 = load_corpus(corpus_directory + "/mesh.txt");
	corpus_data contrived	 = load_corpus(corpus_directory + "/contrived.txt");
	corpus_data price_changes	 = load_corpus(corpus_directory + "/price_changes.txt");
	corpus_data account_balances = load_corpus(corpus_directory + "/account_balances.txt");
	corpus_data sensor_readings	 = load_corpus(corpus_directory + "/sensor_readings.txt");
	corpus_data regular_prices	 = load_corpus(corpus_directory + "/regular_prices.txt");
	bool all_verified{ true };
	all_verified &= verify_corpus("canada", canada);
	all_verified &= verify_corpus("canada_short", canada_short);
	all_verified &= verify_corpus("mesh", mesh);
	all_verified &= verify_corpus("contrived", contrived);
	all_verified &= verify_corpus("price_changes", price_changes);
	all_verified &= verify_corpus("account_balances", account_balances);
	all_verified &= verify_corpus("sensor_readings", sensor_readings);
	all_verified &= verify_corpus("regular_prices", regular_prices);
	if (!all_verified) {
		std::cerr << "Correctness check failed, refusing to benchmark." << std::endl;
		return 1;
	}
	benchmarksuite::pin_for_benchmark();
	run_corpus<"canada">(canada);
	run_corpus<"canada_short">(canada_short);
	run_corpus<"mesh">(mesh);
	run_corpus<"price_changes">(price_changes);
	run_corpus<"account_balances">(account_balances);
	run_corpus<"sensor_readings">(sensor_readings);
	run_corpus<"regular_prices">(regular_prices);
	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return 0;
}
