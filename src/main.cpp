/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * src/main.cpp
 */

#include <benchmarksuite>
#include <source_location>
#include <jsonifier>
#include <atomic>
#include <thread>

struct test_atomic_uint64 {
	BNCH_SWT_HOST static uint64_t impl() {
		std::this_thread::sleep_for(std::chrono::microseconds{ 100 });
		return 20000;
	}
};

struct test_atomic_signed_lock_free {
	BNCH_SWT_HOST static uint64_t impl() {
		std::this_thread::sleep_for(std::chrono::microseconds{ 100 });
		return 20000;
	}
};

int32_t main() {
	using stage_type = benchmarksuite::benchmark_stage<"test_stage_01", benchmarksuite::stage_config_data{ .max_time_in_s = 10 }>;
	benchmarksuite::pin_for_benchmark();
	stage_type::run_benchmark<"test-test", "test_atomic_signed_lock_free", test_atomic_signed_lock_free>();
	stage_type::run_benchmark<"test-test", "test_atomic_uint64", test_atomic_uint64::impl>();
	auto test_rankings = stage_type::get_test_results("test-test");
	std::cout << test_rankings.to_markdown() << std::endl;
	std::cout << test_rankings.to_csv() << std::endl;
	auto all_rankings = stage_type::get_all_results();
	std::cout << all_rankings.to_csv() << std::endl;
	return 0;
}
