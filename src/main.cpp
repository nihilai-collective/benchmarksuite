// MIT License @ /License.md
// Copyright (c) 2026 Nihilai Collective Corp
// https://github.com/nihilai-collective/benchmarksuite
// src/main.cpp

#include <benchmarksuite>
#include <source_location>
#include <atomic>
#include <thread>

using namespace benchmarksuite;

static constexpr uint64_t wait_notify_cycles{ 1000 };

struct test_atomic_uint64 {
	BNCH_SWT_HOST static uint64_t impl() {
		std::atomic<uint64_t> flag{ 0 };
		std::thread waiter([&]() {
			uint64_t value{};
			for (uint64_t i = 0; i < wait_notify_cycles; ++i) {
				uint64_t expected = i;
				++value;
				flag.wait(expected);
				benchmarksuite::do_not_optimize_away(value);
			}
			//std::this_thread::sleep_for(std::chrono::microseconds{ rand() % 1000 });
		});
		uint64_t value{};
		for (uint64_t i = 1; i <= wait_notify_cycles; ++i) {
			flag.store(i, std::memory_order_release);
			flag.notify_one();
			value = flag.load();
			benchmarksuite::do_not_optimize_away(value);
		}
		waiter.join();
		return 20000;
	}
};

struct test_atomic_signed_lock_free {
	BNCH_SWT_HOST static uint64_t impl() {
		std::atomic_unsigned_lock_free flag{ 0 };
		std::thread waiter([&]() {
			typename std::atomic_unsigned_lock_free::value_type value{};
			for (typename std::atomic_unsigned_lock_free::value_type i = 0; i < wait_notify_cycles; ++i) {
				typename std::atomic_unsigned_lock_free::value_type expected = i;
				++value;
				flag.wait(expected);
				benchmarksuite::do_not_optimize_away(value);
			}
		});
		typename std::atomic_unsigned_lock_free::value_type value{};
		for (typename std::atomic_unsigned_lock_free::value_type i = 1; i <= wait_notify_cycles; ++i) {
			flag.store(i, std::memory_order_release);
			flag.notify_one();
			value = flag.load();
			benchmarksuite::do_not_optimize_away(value);
		}
		waiter.join();
		return 20000;
	}
};

template<typename function_type> void test_function() {
	static constexpr function_type function{};
	function();
}

int main() {
	using stage_type = benchmark_stage<"test_stage_01", stage_config_data{}>;
	benchmarksuite::pin_for_benchmark();
	stage_type ::run_benchmark<"test-test", "test_atomic_signed_lock_free", test_atomic_signed_lock_free>();
	stage_type ::run_benchmark<"test-test", "test_atomic_uint64", test_atomic_uint64::impl>();
	auto test_rankings = stage_type::get_test_results("test-test");
	std::cout << test_rankings.to_csv() << std::endl;
	auto all_rankings = stage_type::get_all_results();
	std::cout << all_rankings.to_csv() << std::endl;
	return 0;
}
