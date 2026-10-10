# Contributing to benchmarksuite

Thanks for your interest in improving benchmarksuite. This guide covers how to report problems, propose changes, and get a pull request merged.

## Reporting Bugs

Open an issue and include:

- A minimal benchmark that reproduces the problem.
- Compiler and version, operating system, and target architecture (CPU model, plus GPU model and CUDA version for GPU benchmarks).
- Expected behavior versus actual behavior.
- For measurement problems, the counters involved and the output you got.

## Proposing Changes

For anything larger than a small fix, open an issue first so the approach can be discussed before you invest time in it.

1. Fork the repository and create a branch from `main`.
2. Make your change.
3. Build and run the unit tests locally (see below).
4. Open a pull request against `main` describing what changed and why.

## Building and Testing

benchmarksuite is built with CMake; presets are in `CMakePresets.json`. Tests live in `unit-tests` and are enabled with `-DBNCH_SWT_UNIT_TESTS=TRUE`. CI runs them with `-DBNCH_SWT_ASAN=TRUE -DBNCH_SWT_UBSAN=TRUE`, so please do the same locally.

Every pull request is built by CI on the following matrix, and all of it must pass:

| OS      | Compilers |
|---------|-----------|
| Ubuntu  | Clang, GCC |
| macOS   | Clang, GCC |
| Windows | MSVC |

Hardware performance counter access differs per platform, and CUDA is not covered by CI, so please note which platforms and backends your change touches and which you tested on.

## Code Style

- Run `run-clang-format.ps1` before committing; formatting follows the repository `.clang-format`.
- Do not add explanatory comments to code. Write self-documenting code instead. Comments that attribute algorithms, techniques, or implementations to their original authors or sources are explicitly permitted. If current-us cannot write self-documenting code, then future-us does not deserve to understand it.
- Keep includes lean. Only include what the file actually uses.
- Keep changes focused. Unrelated refactors belong in their own pull request.

### Declarations and Qualifiers

- Declare namespace-scope `constexpr` values as `inline constexpr`.
- Declare class-scope `constexpr` values as `static constexpr`.
- Declare function-level `constexpr` values as `static constexpr` once the project targets C++23 or later. While it targets C++20, leave them as plain `constexpr`, since C++20 does not allow `static` variables in `constexpr` functions.
- Mark every member function that does not modify its object as `const`.
- Mark every function that cannot throw as `noexcept`. Functions that invoke a user-supplied benchmark callable stay without it, so an exception from user code propagates instead of calling `std::terminate`.
- Make every function that is only ever evaluated at compile time `consteval`. Keep `constexpr` for functions that are also called on runtime objects, such as `size()` or a generator's `min()` and `max()`.
- Mark every function that is not `consteval` with `BNCH_SWT_HOST`, `BNCH_SWT_DEVICE`, `BNCH_SWT_HOST_DEVICE` or `BNCH_SWT_NOINLINE` (each includes `inline`), or with `inline` outside the library headers. The exceptions are `main`, CUDA `__global__` kernels, `= default`/`= delete` declarations and friend declarations.
- Never put `inline` or an inline macro on a `consteval` function, since it is already implicitly inline. A `consteval` function that device code calls takes bare `__host__ __device__` instead of `BNCH_SWT_HOST_DEVICE`.
- Give every enum the smallest unsigned underlying type that holds all of its values, for example `enum class position_type : uint8_t`.

### Member Ordering

Order the members of every struct and class like this, within each `public:`/`protected:`/`private:` section:

1. Type aliases, nested types and `static constexpr` values (an alias or value always comes after anything it uses)
2. Default constructor
3. Parameterized constructors
4. Move assignment operator
5. Move constructor
6. Copy assignment operator
7. Copy constructor
8. All other functions, in descending order of the longest line in each function
9. Destructor
10. Data members, ordered first by alignment (largest first), then in descending order of line length

Structs that mirror an external ABI, such as the kperf structs in `apple_arm_perf_events.hpp`, keep their layout. Because data member order is also initialization order, reordering data members means checking every positional or designated initializer and every mem-init list of that struct.

Order namespace-level functions the same way: first by what each one needs declared before it, then in descending order of their longest line. Overload sets that call each other by unqualified name, such as `print_value`, keep their order, since it decides which overloads each one can see.

## Measurement Changes

benchmarksuite exists to measure other code, so its own overhead matters. A change to the timing, counter, or measurement loop should state how it affects overhead and measurement stability, with before and after numbers on at least one compiler.

## Pull Request Checklist

- [ ] Builds and passes tests on all CI platforms
- [ ] Formatted with clang-format
- [ ] No added explanatory code comments (source attributions are permitted)
- [ ] Qualifiers and member ordering follow the Code Style rules
- [ ] Overhead numbers included for measurement changes
- [ ] Linked to the issue it addresses, if any

## Licensing

By contributing, you agree that your contributions are licensed under the project's MIT license (see `License.md`).
