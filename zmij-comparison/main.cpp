#include <benchmarksuite>
#include <simdjson.h>
#include <jsonifier>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

inline constexpr uint64_t max_chars_per_value = 64;
inline constexpr uint64_t buffer_slack		  = 64;

enum class dataset_kind : uint64_t {
	canada,
	mesh,
	count,
};

inline constexpr std::array<std::string_view, static_cast<uint64_t>(dataset_kind::count)> dataset_files{ "canada.txt", "mesh.txt" };

inline std::array<std::vector<double>, static_cast<uint64_t>(dataset_kind::count)> datasets{};

inline std::vector<double> load_dataset(dataset_kind kind) {
	const std::filesystem::path path{ std::filesystem::path{ BNCH_SWT_FLOAT_CORPUS_DIR } / dataset_files[static_cast<uint64_t>(kind)] };
	std::ifstream stream{ path, std::ios::binary };
	if (!stream) {
		std::cerr << "Failed to open " << path.string() << "\n";
		std::exit(1);
	}
	std::vector<double> values;
	std::string line;
	while (std::getline(stream, line)) {
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		double value{};
		if (std::from_chars(line.data(), line.data() + line.size(), value).ec == std::errc{} && std::isfinite(value)) {
			values.emplace_back(value);
		}
	}
	return values;
}

inline uint64_t largest_dataset_size() {
	uint64_t largest = 0;
	for (const auto& dataset: datasets) {
		largest = std::max<uint64_t>(largest, dataset.size());
	}
	return largest;
}

inline std::vector<uint8_t> jsonifier_buffer{};
inline simdjson::builder::string_builder simdjson_buffer{};

template<typename value_type> struct jsonifier_baseline_writer {
	BNCH_SWT_HOST static uint8_t* write(uint8_t* out, value_type value) {
		return jsonifier::internal::to_chars<value_type>::impl(out, value);
	}
};

template<dataset_kind kind> struct jsonifier_benchmark {
	BNCH_SWT_HOST static uint64_t impl() {
		uint8_t* const start = jsonifier_buffer.data();
		const auto& dataset	 = datasets[static_cast<uint64_t>(kind)];
		uint8_t* out		 = start;
		*out++				 = '[';
		for (uint64_t i = 0; i < dataset.size(); ++i) {
			*out = ',';
			out += i != 0;
			out = jsonifier_baseline_writer<double>::write(out, dataset[i]);
		}
		*out++ = ']';
		return static_cast<uint64_t>(out - start);
	}
};

template<dataset_kind kind> struct simdjson_to_json_benchmark {
	BNCH_SWT_HOST static uint64_t impl() {
		const auto& dataset = datasets[static_cast<uint64_t>(kind)];
		simdjson_buffer.clear();
		simdjson_buffer.append('[');
		for (uint64_t i = 0; i < dataset.size(); ++i) {
			if (i != 0) {
				simdjson_buffer.append(',');
			}
			simdjson_buffer.append(dataset[i]);
		}
		simdjson_buffer.append(']');
		return simdjson_buffer.size();
	}
};

inline std::string format_simdjson(double value) {
	std::string result;
	if (simdjson::to_json(value, result) != simdjson::SUCCESS) {
		result.clear();
	}
	return result;
}

inline std::string format_jsonifier(double value) {
	std::array<uint8_t, max_chars_per_value> buffer{};
	uint8_t* end = jsonifier_baseline_writer<double>::write(buffer.data(), value);
	std::string result;
	for (const uint8_t* current = buffer.data(); current != end; ++current) {
		result.push_back(static_cast<char>(*current));
	}
	return result;
}

inline bool round_trips(const std::string& text, double expected) {
	const double parsed = std::strtod(text.c_str(), nullptr);
	return std::bit_cast<uint64_t>(parsed) == std::bit_cast<uint64_t>(expected);
}

inline uint64_t verify_against_simdjson() {
	static constexpr std::array<double, 16> edge_cases{ 0.0, -0.0, 100.0, 1.5, 0.5, -10.5, 0.001, 1e-4, 1e-5, 9.999e-5, 1e20, 1e21, 1.7976931348623157e+308,
		2.2250738585072014e-308, 4.9406564584124654e-324, 123456789012345680.0 };
	uint64_t mismatches		   = 0;
	uint64_t roundtrip_failures = 0;
	uint64_t checked			= 0;
	auto check					= [&](double value) {
		 ++checked;
		 const std::string expected = format_simdjson(value);
		 const std::string actual	= format_jsonifier(value);
		 if (!round_trips(actual, value)) {
			 ++roundtrip_failures;
		 }
		 if (expected != actual) {
			 if (mismatches < 10) {
				 std::cout << "  mismatch bits=0x" << std::hex << std::bit_cast<uint64_t>(value) << std::dec << " simdjson=\"" << expected
						   << "\" jsonifier=\"" << actual << "\"\n";
			 }
			 ++mismatches;
		 }
	};
	for (const double value: edge_cases) {
		check(value);
	}
	for (const auto& dataset: datasets) {
		for (const double value: dataset) {
			check(value);
		}
	}
	std::mt19937_64 engine{ 0xc0ffeeULL };
	for (uint64_t i = 0; i < 2'000'000; ++i) {
		const double value = std::bit_cast<double>(engine());
		if (std::isfinite(value)) {
			check(value);
		}
	}
	std::cout << "jsonifier-zmij: checked " << checked << " values, " << mismatches << " output mismatches vs simdjson, " << roundtrip_failures
			  << " round-trip failures\n";
	return mismatches + roundtrip_failures;
}

inline uint64_t verify_simdjson_round_trips() {
	uint64_t failures = 0;
	for (const auto& dataset: datasets) {
		std::string output;
		if (simdjson::to_json(dataset, output) != simdjson::SUCCESS || output.size() < 2 || output.front() != '[' || output.back() != ']') {
			++failures;
			continue;
		}
		const char* current = output.data() + 1;
		const char* end		= output.data() + output.size() - 1;
		for (const double expected: dataset) {
			double parsed{};
			const auto result = std::from_chars(current, end, parsed);
			if (result.ec != std::errc{} || std::bit_cast<uint64_t>(parsed) != std::bit_cast<uint64_t>(expected)) {
				++failures;
			}
			current = result.ptr + (result.ptr != end);
		}
	}
	std::cout << "simdjson-to_json: " << failures << " round-trip failures across benchmark datasets\n";
	return failures;
}

using stage_type = benchmarksuite::benchmark_stage<"zmij-comparison", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_time_in_s = 10 }>;

template<dataset_kind kind, benchmarksuite::string_literal test_name> void run_dataset() {
	stage_type::run_benchmark<test_name, "jsonifier-zmij", jsonifier_benchmark<kind>>();
	stage_type::run_benchmark<test_name, "simdjson-to_json", simdjson_to_json_benchmark<kind>>();
	std::cout << stage_type::get_test_results(test_name.operator std::string_view()).to_markdown() << std::endl;
}

int32_t main() {
	for (uint64_t i = 0; i < datasets.size(); ++i) {
		datasets[i] = load_dataset(static_cast<dataset_kind>(i));
		std::cout << dataset_files[i] << ": " << datasets[i].size() << " values\n";
	}
	jsonifier_buffer.resize(largest_dataset_size() * max_chars_per_value + buffer_slack);
	simdjson_buffer = simdjson::builder::string_builder{ largest_dataset_size() * max_chars_per_value + buffer_slack };

	std::cout << "JSONIFIER_AVX=" << JSONIFIER_AVX << " JSONIFIER_AVX2=" << JSONIFIER_AVX2 << " JSONIFIER_AVX512=" << JSONIFIER_AVX512 << "\n";
	uint64_t verification_failures = verify_against_simdjson();
	verification_failures += verify_simdjson_round_trips();
	std::cout << "Total verification failures: " << verification_failures << "\n" << std::endl;

	benchmarksuite::pin_for_benchmark();
	run_dataset<dataset_kind::canada, "canada">();
	run_dataset<dataset_kind::mesh, "mesh">();

	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return verification_failures == 0 ? 0 : 1;
}
