/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/jsonifier_stage1.cpp
 */

#include <benchmarksuite>
#include <jsonifier>
#include <string>

extern "C" {
BNCH_SWT_NOINLINE uint32_t* bnch_swt_jsonifier_stage1_minified(jsonifier::jsonifier_core<>& parser, const std::string& json_data) {
	return parser.collectStructurals<jsonifier::parse_options{ .partialRead = true, .minified = true }>(json_data);
}

BNCH_SWT_NOINLINE uint32_t* bnch_swt_jsonifier_stage1_prettified(jsonifier::jsonifier_core<>& parser, const std::string& json_data) {
	return parser.collectStructurals<jsonifier::parse_options{ .partialRead = true, .minified = false }>(json_data);
}
}

static std::string generate_json(bool prettified, size_t record_count) {
	const std::string newline{ prettified ? "\n" : "" };
	const std::string indent_one{ prettified ? "  " : "" };
	const std::string indent_two{ prettified ? "    " : "" };
	const std::string separator{ prettified ? ": " : ":" };
	std::string json_data{ "[" + newline };
	for (size_t x = 0; x < record_count; ++x) {
		json_data += indent_one + "{" + newline;
		json_data += indent_two + "\"id\"" + separator + std::to_string(x * 7919u) + "," + newline;
		json_data += indent_two + "\"name\"" + separator + "\"user_" + std::to_string(x) + " says \\\"hi\\\"\"," + newline;
		json_data += indent_two + "\"score\"" + separator + std::to_string(static_cast<double>(x) * 0.731) + "," + newline;
		json_data += indent_two + "\"active\"" + separator + ((x & 1u) ? "true" : "false") + "," + newline;
		json_data += indent_two + "\"tags\"" + separator + "[\"alpha\", \"beta\", null, " + std::to_string(x % 97u) + "]," + newline;
		json_data += indent_two + "\"text\"" + separator + "\"Lorem ipsum dolor sit amet, consectetur adipiscing elit {not, structural} [nope]\"" + newline;
		json_data += indent_one + "}" + ((x + 1 < record_count) ? "," : "") + newline;
	}
	json_data += "]";
	json_data.reserve(json_data.size() + 512);
	return json_data;
}

struct jsonifier_stage1_minified {
	BNCH_SWT_HOST static size_t impl(jsonifier::jsonifier_core<>& parser, const std::string& json_data) {
		benchmarksuite::do_not_optimize_away(bnch_swt_jsonifier_stage1_minified(parser, json_data));
		return json_data.size();
	}
};

struct jsonifier_stage1_prettified {
	BNCH_SWT_HOST static size_t impl(jsonifier::jsonifier_core<>& parser, const std::string& json_data) {
		benchmarksuite::do_not_optimize_away(bnch_swt_jsonifier_stage1_prettified(parser, json_data));
		return json_data.size();
	}
};

int32_t main() {
	using stage_type = benchmarksuite::benchmark_stage<"jsonifier_stage1", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_time_in_s = 5 }>;
	benchmarksuite::pin_for_benchmark();
	const std::string minified_json{ generate_json(false, 8192) };
	const std::string prettified_json{ generate_json(true, 8192) };
	jsonifier::jsonifier_core<> parser;
	stage_type::run_benchmark<"stage1-minified", "jsonifier", jsonifier_stage1_minified>(parser, minified_json);
	stage_type::run_benchmark<"stage1-prettified", "jsonifier", jsonifier_stage1_prettified>(parser, prettified_json);
	std::cout << stage_type::get_test_results("stage1-minified").to_markdown() << std::endl;
	std::cout << stage_type::get_test_results("stage1-prettified").to_markdown() << std::endl;
	return 0;
}
