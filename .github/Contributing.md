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

## Measurement Changes

benchmarksuite exists to measure other code, so its own overhead matters. A change to the timing, counter, or measurement loop should state how it affects overhead and measurement stability, with before and after numbers on at least one compiler.

## Pull Request Checklist

- [ ] Builds and passes tests on all CI platforms
- [ ] Formatted with clang-format
- [ ] No added explanatory code comments (source attributions are permitted)
- [ ] Overhead numbers included for measurement changes
- [ ] Linked to the issue it addresses, if any

## Licensing

By contributing, you agree that your contributions are licensed under the project's MIT license (see `License.md`).
