<p align="center"><img src="./Logo.png?raw=true" width="350"/></p>

![Lines of Code](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/nihilai-collective/benchmarksuite/badges/loc.json&style=plastic)
[![License](https://img.shields.io/badge/license-MIT-blue.svg?style=plastic)](License.md)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=plastic)](https://isocpp.org/)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg?style=plastic)](include/)

---
## Compiler Support
![MSVC](https://img.shields.io/github/actions/workflow/status/nihilai-collective/benchmarksuite/unit-tests.yml?style=plastic&logo=microsoft&logoColor=green&label=MSVC&labelColor=pewter&color=blue&branch=main)
![GCC](https://img.shields.io/github/actions/workflow/status/nihilai-collective/benchmarksuite/unit-tests.yml?style=plastic&logo=linux&logoColor=green&label=GCC&labelColor=pewter&color=blue&branch=main)
![CLANG](https://img.shields.io/github/actions/workflow/status/nihilai-collective/benchmarksuite/unit-tests.yml?style=plastic&logo=apple&logoColor=green&label=CLANG&labelColor=pewter&color=blue&branch=main)

## Operating System Support
![Windows](https://img.shields.io/github/actions/workflow/status/nihilai-collective/benchmarksuite/unit-tests.yml?style=plastic&logo=microsoft&logoColor=green&label=Windows&labelColor=pewter&color=blue&branch=main)
![Linux](https://img.shields.io/github/actions/workflow/status/nihilai-collective/benchmarksuite/unit-tests.yml?style=plastic&logo=linux&logoColor=green&label=Linux&labelColor=pewter&color=blue&branch=main)
![Mac](https://img.shields.io/github/actions/workflow/status/nihilai-collective/benchmarksuite/unit-tests.yml?style=plastic&logo=apple&logoColor=green&label=MacOS&labelColor=pewter&color=blue&branch=main)

---

A header-only C++20 benchmarking library with cross-platform hardware performance counter integration, providing precise measurements of cycles, instructions, branches, cache behavior, latency, and throughput with minimal overhead. Also supports CUDA GPU benchmarking.

## Used In
- **[Nihilus](https://nihilai-collective.net/nihilus)**
- **[Json-Performance](https://github.com/nihilai-collective/Json-Performance)**: JSON parsing/serialization benchmarks comparing Jsonifier, Glaze, and Simdjson.
- **[rtc-digit-count](https://github.com/nihilai-collective/rtc-digit-count)**: comparing the rtc-digit-count algorithm to the lemire algorithm.
- **[StringInt-Benchmarks](https://github.com/nihilai-collective/stringint-benchmarks)**: comparing the void-numerics library to jeaiii and std::from_chars.

## Features

- **Header-only**: just include and go, no linking required
- **Hardware performance counters** via native OS APIs, measured per iteration:
  - macOS: kperf/kpc private frameworks (Apple Silicon and Intel, needs root) for cycles, instructions, branches, branch misses and cache misses
  - Linux: perf_event for cycles, instructions, branches, branch misses, cache references and cache misses, falling back to rdtsc cycles when perf_event is unavailable
  - Windows: `__rdtsc` cycles
  - Android: wall-clock timing only
- **CUDA GPU benchmarking**: cudaEvent timing, with cycles derived from the device clock rate
- **Adaptive convergence loop**: doubles the epoch size each pass until the time or iteration budget runs out, then reports the epoch with the lowest combined RSE and mean drift
- **Statistical tie detection**: Welch's t-test with Welch-Satterthwaite degrees of freedom to tell real winners from noise, with rank sharing across tied libraries
- **Cache eviction**: one cache eviction before each `run_benchmark` call so every run starts from the same state, toggleable per stage
- **Thread affinity and priority pinning** via `pin_for_benchmark()`: pins to a P-core on Intel hybrid CPUs (and to the fastest core on ARM Linux and Android), then raises priority with `HIGH_PRIORITY_CLASS`/`THREAD_PRIORITY_HIGHEST` on Windows, `SCHED_FIFO` (falling back to nice -20) on Linux and Android, and `QOS_CLASS_USER_INTERACTIVE` on macOS
- **Compile-time CPU/GPU property injection**: CMake detects cache sizes, alignment, SM count and more at configure time and bakes them in as `constexpr`
- **Multi-format output**: Markdown tables and CSV, with system info preambles, both per-test and per-stage (win/tie/loss rollups)
- **Do-not-optimize barriers**: inline asm on GCC and Clang, `_ReadWriteBarrier` on MSVC
- **Random data generation**: xoshiro256++ with time-based or deterministic seeding

## Requirements

- C++20 or later
- CMake 3.28 or later
- Supported platforms:
  - Windows x64 (MSVC)
  - Linux x64/ARM64 (GCC, Clang)
  - Android (timing only, no hardware counters)
  - macOS x64/ARM64 (AppleClang, GCC via Homebrew)
- Optional: CUDA toolkit for GPU benchmarks (enable with `-DBNCH_SWT_CUDA=ON`)

## Installation

### vcpkg

```
vcpkg install rtc-benchmarksuite
```

Then in your CMakeLists.txt:

```cmake
find_package(benchmarksuite CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE benchmarksuite::benchmarksuite)
```

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
  benchmarksuite
  GIT_REPOSITORY https://github.com/nihilai-collective/benchmarksuite.git
  GIT_TAG main
)
FetchContent_MakeAvailable(benchmarksuite)
target_link_libraries(your_target PRIVATE benchmarksuite::benchmarksuite)
```

## Quick Start

```cpp
#include <benchmarksuite>
#include <algorithm>

static constexpr benchmarksuite::stage_config_data config{
    .measured_iteration_count = 100,
    .max_iteration_count = 10000,
    .convergence_threshold = 1.0,
    .max_time_in_s = 5,
    .rse_threshold = 2.5,
    .max_k = 100000,
    .min_k = 30,
    .clear_cpu_caches_before_iterations = true,
    .include_non_converged_results = false,
    .benchmark_type = benchmarksuite::benchmark_types::cpu,
};

using bench = benchmarksuite::benchmark_stage<"my-stage", config>;

int32_t main() {
    std::vector<int32_t> my_vector(1 << 16);
    bench::run_benchmark<"sort-test", "std-sort", +[](std::vector<int32_t>& v) -> uint64_t {
        std::sort(v.begin(), v.end());
        benchmarksuite::do_not_optimize_away(v);
        return v.size() * sizeof(int32_t);
    }>(my_vector);

    for (const auto& test : bench::get_finished_tests()) {
        std::cout << test.to_markdown();
    }
    return 0;
}
```

The functor or lambda returns the number of bytes processed, which the throughput calculations use.

## Two Ways to Register a Benchmark

`run_benchmark` has two overloads, and they work differently:

**1. Stateless lambda as a non-type template parameter** (shown above): the `+[](...)` syntax decays the lambda to a function pointer, passed as `auto function` in the template argument list. This bakes the callable into the type itself at compile time. Requires no captures.

```cpp
bench::run_benchmark<"test-name", "library-name", +[](args...) -> uint64_t {
    return bytes_processed;
}>(runtime_args...);
```

**2. A functor type passed as a normal template argument**, invoked at runtime via a static `impl(args...)` method:

```cpp
struct my_functor {
    static uint64_t impl(std::vector<int32_t>& v) {
        std::sort(v.begin(), v.end());
        return v.size() * sizeof(int32_t);
    }
};

bench::run_benchmark<"test-name", "library-name", my_functor>(my_vector);
```

Use the NTTP lambda form for quick one-off inline benchmarks. Use the functor form when the benchmark body needs to be reused, is non-trivial, or needs per-iteration setup via `before`.

### Per-Iteration Setup

A functor may optionally define a static `before(args...)`. When present, it is called before every iteration with the same arguments as `impl`, and it runs **outside** the timed and counted region. Use it to reset state that `impl` mutates, so each iteration measures the same work:

```cpp
struct parse_into_vector {
    static void before(std::string& json, std::vector<uint64_t>& out) {
        out.clear();
    }

    static uint64_t impl(std::string& json, std::vector<uint64_t>& out) {
        parse(json, out);
        return json.size();
    }
};

bench::run_benchmark<"test-name", "library-name", parse_into_vector>(json, out);
```

Functors without `before` behave exactly as before.

## Example: Benchmarking Atomic Wait/Notify

Multi-threaded benchmarks work the same way: spin up your threads inside the functor's `impl`, join before returning. This example compares `std::atomic<uint64_t>` against `std::atomic_unsigned_lock_free` for wait/notify throughput, and shows `benchmarksuite::pin_for_benchmark()` being called once up front to lock thread affinity/priority for the whole run.

```cpp
#include <benchmarksuite>
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

int32_t main() {
    using stage_type = benchmark_stage<"test_stage_01", stage_config_data{}>;
    benchmarksuite::pin_for_benchmark();

    stage_type::run_benchmark<"test-test", "test_atomic_signed_lock_free", test_atomic_signed_lock_free>();
    stage_type::run_benchmark<"test-test", "test_atomic_uint64", test_atomic_uint64::impl>();

    auto test_rankings = stage_type::get_test_results("test-test");
    std::cout << test_rankings.to_csv() << std::endl;

    auto all_rankings = stage_type::get_all_results();
    std::cout << all_rankings.to_csv() << std::endl;
    return 0;
}
```

Note the two registration styles side by side: `test_atomic_signed_lock_free` is passed as a functor type (invoked via its `impl` method at runtime), while `test_atomic_uint64::impl` is passed directly as a function pointer NTTP. Both are valid, so pick whichever reads cleaner for the call site.

## Stage Configuration

`stage_config_data` controls the adaptive benchmarking loop:

- `measured_iteration_count`: initial epoch size (default 100)
- `max_iteration_count`: hard ceiling on total iterations, also sizes the preallocated metrics buffer (default 1000)
- `convergence_threshold`: mean-stability threshold between epochs, as a percentage (default 1.0)
- `max_time_in_s`: wall-clock budget per benchmark (default 5)
- `rse_threshold`: target Relative Standard Error % for convergence (default 2.5)
- `max_k`: statistical window ceiling (default 100000)
- `min_k`: statistical window floor, must be > 1 for valid Bessel-corrected variance (default 30)
- `clear_cpu_caches_before_iterations`: evict CPU caches once before each `run_benchmark` call (default `true`)
- `include_non_converged_results`: rank a test even when some of its libraries did not converge, marking those rows and adding a Converged column (default `false`, which leaves such tests unranked)
- `benchmark_type`: `cpu` or `cuda` (default `cpu`)

Designated initializers must follow this order, and the constraints between fields are checked with `static_assert`.

The loop doubles the epoch size each pass and keeps going until the time or iteration budget runs out. A result counts as converged when its RSE is at or below `rse_threshold` and its mean moved by no more than `convergence_threshold` since the previous epoch. The reported epoch is the one with the lowest RSE plus mean drift.

## Output Formats

Results can be emitted as Markdown or CSV, per-test or rolled up per-stage:

```cpp
for (const auto& test : bench::get_finished_tests()) {
    test.to_markdown(true, true, "./output_dir");
    test.to_csv(true, "./output_dir");
}
bench::get_all_results().to_csv("./output_dir");
```

`get_test_results("test-name")` returns the ranked results for a single test, and `get_finished_tests()` returns every test that was ranked. Each result also offers `print()`, `converged()`, `size()`, `get_test_name()` and `get_stage_name()`. Passing a directory saves the file there as `<os>-<compiler>-<stage>-<test>.md` or `.csv`, and the stage rollup as `<os>-<compiler>-<stage>.csv`.

Per-test output includes throughput (MB/s), RSE %, window duration, bytes processed, sample size, variance, latency, mean drift between the last two epochs, per-byte hardware counters when available (cycles, instructions, branches, branch misses, cache references, cache misses), and Win/Tie/Loss position. Stage-level CSV rolls this up into win/tie/loss counts per library across all tests in the stage, alongside the stage config used to produce them.

## CUDA Support

Configure with `-DBNCH_SWT_CUDA=ON` and set `benchmark_type = benchmarksuite::benchmark_types::cuda`. Register a functor whose `impl` launches your kernels and returns the bytes processed; each iteration is timed with CUDA events, and cache eviction does nothing for CUDA stages. See `src/main.cu` for a full example benchmarking native GPU division vs. Granlund-Montgomery magic-number division across constant memory, compile-time, and runtime dispatch paths.

## Statistical Methodology

- **Bessel's correction** on variance (dividing by k-1)
- **Welch's t-test** for pairwise comparison, tolerant of unequal variances and sample sizes
- **Welch-Satterthwaite** approximation for degrees of freedom
- **Rank sharing**: statistically tied libraries share the same position on the leaderboard

## Sanitizer Support

The unit-tests CMake exposes `BNCH_SWT_ASAN` and `BNCH_SWT_UBSAN` options. UBSan has no MSVC equivalent and is turned off there with a warning, and both sanitizers are turned off for GCC on macOS, where they don't work.

## Testing

Configure with `-DBNCH_SWT_UNIT_TESTS=TRUE` to build the unit tests. They use [rt-ut](https://github.com/nihilai-collective/rt-ut), our header-only C++20 unit testing framework. CMake fetches it automatically through `FetchContent` when the unit tests are built, so there is nothing to install.

## License

MIT © Nihilai Collective Corp, see [License.md](License.md).

---
