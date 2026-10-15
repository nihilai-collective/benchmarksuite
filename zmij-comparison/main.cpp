#include <benchmarksuite>
#include <zmij.h>
#include <jsonifier>

#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

inline constexpr uint64_t value_count		  = 10000;
inline constexpr uint64_t max_chars_per_value = 64;
inline constexpr uint64_t buffer_slack		  = 64;

enum class dataset_kind : uint64_t {
	random_bits,
	unit_interval,
	short_decimals,
	large_integers,
	count,
};

inline std::vector<double> generate_dataset(dataset_kind kind) {
	std::mt19937_64 engine{ 0x5eed0000ULL + static_cast<uint64_t>(kind) };
	std::vector<double> values;
	values.reserve(value_count);
	while (values.size() < value_count) {
		double value{};
		switch (kind) {
			case dataset_kind::random_bits: {
				value = std::bit_cast<double>(engine());
				break;
			}
			case dataset_kind::unit_interval: {
				value = std::uniform_real_distribution<double>{ 0.0, 1.0 }(engine);
				break;
			}
			case dataset_kind::short_decimals: {
				static constexpr std::array<double, 5> divisors{ 1.0, 10.0, 100.0, 1000.0, 10000.0 };
				const auto mantissa = static_cast<double>(std::uniform_int_distribution<int64_t>{ -999999, 999999 }(engine));
				value				= mantissa / divisors[std::uniform_int_distribution<uint64_t>{ 0, divisors.size() - 1 }(engine)];
				break;
			}
			case dataset_kind::large_integers: {
				value = static_cast<double>(std::uniform_int_distribution<int64_t>{ -(int64_t{ 1 } << 53), int64_t{ 1 } << 53 }(engine));
				break;
			}
			case dataset_kind::count: {
				break;
			}
		}
		if (std::isfinite(value)) {
			values.emplace_back(value);
		}
	}
	return values;
}

inline std::array<std::vector<double>, static_cast<uint64_t>(dataset_kind::count)> datasets{};
inline std::vector<char> upstream_buffer(value_count * max_chars_per_value + buffer_slack);
inline std::vector<uint8_t> jsonifier_buffer(value_count * max_chars_per_value + buffer_slack);

template<typename value_type> struct zmij_upstream_writer {
	BNCH_SWT_HOST static char* write(char* out, value_type value) {
		return zmij::detail::write(out, value);
	}
};

template<typename value_type> struct jsonifier_baseline_writer {
	BNCH_SWT_HOST static uint8_t* write(uint8_t* out, value_type value) {
		return jsonifier::zmij::detail::write(value, out);
	}
};

template<dataset_kind kind> struct zmij_upstream_benchmark {
	BNCH_SWT_HOST static uint64_t impl() {
		char* const start = upstream_buffer.data();
		char* out		  = start;
		for (const double value: datasets[static_cast<uint64_t>(kind)]) {
			out = zmij_upstream_writer<double>::write(out, value);
		}
		return static_cast<uint64_t>(out - start);
	}
};

template<dataset_kind kind, template<typename> typename writer> struct jsonifier_benchmark {
	BNCH_SWT_HOST static uint64_t impl() {
		uint8_t* const start = jsonifier_buffer.data();
		uint8_t* out		 = start;
		for (const double value: datasets[static_cast<uint64_t>(kind)]) {
			out = writer<double>::write(out, value);
		}
		return static_cast<uint64_t>(out - start);
	}
};

inline std::string format_upstream(double value) {
	std::array<char, max_chars_per_value> buffer{};
	char* end = zmij_upstream_writer<double>::write(buffer.data(), value);
	return std::string{ buffer.data(), end };
}

template<template<typename> typename writer> std::string format_jsonifier(double value) {
	std::array<uint8_t, max_chars_per_value> buffer{};
	uint8_t* end = writer<double>::write(buffer.data(), value);
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

template<template<typename> typename writer> uint64_t verify_against_upstream(std::string_view writer_name) {
	static constexpr std::array<double, 16> edge_cases{ 0.0, -0.0, 100.0, 1.5, 0.5, -10.5, 0.001, 1e-4, 1e-5, 9.999e-5, 1e20, 1e21, 1.7976931348623157e+308,
		2.2250738585072014e-308, 4.9406564584124654e-324, 123456789012345680.0 };
	uint64_t mismatches		   = 0;
	uint64_t roundtrip_failures = 0;
	uint64_t checked			= 0;
	auto check					= [&](double value) {
		 ++checked;
		 const std::string expected = format_upstream(value);
		 const std::string actual	= format_jsonifier<writer>(value);
		 if (!round_trips(actual, value)) {
			 ++roundtrip_failures;
		 }
		 if (expected != actual) {
			 if (mismatches < 10) {
				 std::cout << "  mismatch [" << writer_name << "] bits=0x" << std::hex << std::bit_cast<uint64_t>(value) << std::dec << " upstream=\"" << expected
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
	std::cout << writer_name << ": checked " << checked << " values, " << mismatches << " output mismatches vs upstream, " << roundtrip_failures
			  << " round-trip failures\n";
	return mismatches + roundtrip_failures;
}

inline uint64_t verify_upstream_round_trips() {
	uint64_t failures = 0;
	for (const auto& dataset: datasets) {
		for (const double value: dataset) {
			if (!round_trips(format_upstream(value), value)) {
				++failures;
			}
		}
	}
	std::cout << "zmij-upstream: " << failures << " round-trip failures across benchmark datasets\n";
	return failures;
}

using stage_type = benchmarksuite::benchmark_stage<"zmij-comparison", benchmarksuite::stage_config_data{ .include_non_converged_results = true, .max_time_in_s = 10 }>;

template<dataset_kind kind, benchmarksuite::string_literal test_name> void run_dataset() {
	stage_type::run_benchmark<test_name, "zmij-upstream", zmij_upstream_benchmark<kind>>();
	stage_type::run_benchmark<test_name, "jsonifier-zmij", jsonifier_benchmark<kind, jsonifier_baseline_writer>>();
	std::cout << stage_type::get_test_results(test_name.operator std::string_view()).to_markdown() << std::endl;
}

int32_t main() {
	for (uint64_t i = 0; i < datasets.size(); ++i) {
		datasets[i] = generate_dataset(static_cast<dataset_kind>(i));
	}

	std::cout << "JSONIFIER_CONFIGURED_AVX_TIER=" << JSONIFIER_CONFIGURED_AVX_TIER << "\n";
	uint64_t verification_failures = verify_upstream_round_trips();
	verification_failures += verify_against_upstream<jsonifier_baseline_writer>("jsonifier-zmij");
	std::cout << "Total verification failures: " << verification_failures << "\n" << std::endl;

	benchmarksuite::pin_for_benchmark();
	run_dataset<dataset_kind::random_bits, "random-bit-patterns">();
	run_dataset<dataset_kind::unit_interval, "uniform-unit-interval">();
	run_dataset<dataset_kind::short_decimals, "short-decimals">();
	run_dataset<dataset_kind::large_integers, "large-integers">();

	std::cout << stage_type::get_all_results().to_csv() << std::endl;
	return verification_failures == 0 ? 0 : 1;
}
