// MIT License @ /License.md
// Copyright (c) 2026 Nihilai Collective Corp
// https://github.com/nihilai-collective/benchmarksuite
// include/benchmarksuite-incl/event_counter.hpp

#pragma once

#include <benchmarksuite-incl/config.hpp>
#include <benchmarksuite-incl/counters/apple_arm_perf_events.hpp>
#include <benchmarksuite-incl/counters/windows_perf_events.hpp>
#include <benchmarksuite-incl/counters/linux_perf_events.hpp>
#include <benchmarksuite-incl/counters/cuda_perf_events.hpp>
#include <benchmarksuite-incl/counters/android_perf_events.hpp>

namespace benchmarksuite::internal {

	template<benchmark_types benchmark_type> struct event_count;

	template<> struct event_count<benchmark_types::cpu> {
		template<typename value_type, benchmark_types, uint64_t count> friend struct event_collector_type;

		BNCH_SWT_HOST event_count() noexcept {
		}

		BNCH_SWT_HOST bool elapsed_ns(double& elapsed_ns_new) const noexcept {
			if (elapsed_ns_val.has_value()) {
				elapsed_ns_new = elapsed_ns_val.value().count();
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool bytes_processed(uint64_t& bytes_processed_new) const noexcept {
			if (bytes_processed_val.has_value()) {
				bytes_processed_new = bytes_processed_val.value();
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool cycles(double& cycles_new) const {
			if (cycles_val.has_value()) {
				cycles_new = static_cast<double>(cycles_val.value());
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool instructions(double& instructions_new) const noexcept {
			if (instructions_val.has_value()) {
				instructions_new = static_cast<double>(instructions_val.value());
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool branches(double& branches_new) const noexcept {
			if (branches_val.has_value()) {
				branches_new = static_cast<double>(branches_val.value());
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool branch_misses(double& branch_misses_new) const noexcept {
			if (branch_misses_val.has_value()) {
				branch_misses_new = static_cast<double>(branch_misses_val.value());
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool cache_misses(double& cache_misses_new) const noexcept {
			if (cache_misses_val.has_value()) {
				cache_misses_new = static_cast<double>(cache_misses_val.value());
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool cache_references(double& cache_references_new) const noexcept {
			if (cache_references_val.has_value()) {
				cache_references_new = static_cast<double>(cache_references_val.value());
				return true;
			} else {
				return false;
			}
		}

	  protected:
		std::optional<uint64_t> cache_references_val{};
		std::optional<uint64_t> bytes_processed_val{};
		std::optional<nanoseconds> elapsed_ns_val{};
		std::optional<uint64_t> branch_misses_val{};
		std::optional<uint64_t> instructions_val{};
		std::optional<uint64_t> cache_misses_val{};
		std::optional<uint64_t> branches_val{};
		std::optional<uint64_t> cycles_val{};
	};

	template<> struct event_count<benchmark_types::cuda> {
		BNCH_SWT_HOST event_count() noexcept {
		}

		BNCH_SWT_HOST bool elapsed_ns(double& elapsed_ns_new) const noexcept {
			if (elapsed_ns_val.has_value()) {
				elapsed_ns_new = elapsed_ns_val.value().count();
				return true;
			} else {
				return false;
			}
		}

		BNCH_SWT_HOST bool cuda_event_ms(double& cuda_event_ms_new) const noexcept {
			if (cuda_event_ms_val.has_value()) {
				cuda_event_ms_new = cuda_event_ms_val.value();
				return true;
			}
			return false;
		}

		BNCH_SWT_HOST bool bytes_processed(uint64_t& bytes_processed_new) const noexcept {
			if (bytes_processed_val.has_value()) {
				bytes_processed_new = bytes_processed_val.value();
				return true;
			}
			return false;
		}

		BNCH_SWT_HOST bool cycles(double& cycles_new) const {
			if (cycles_val.has_value()) {
				cycles_new = static_cast<double>(cycles_val.value());
				return true;
			}
			return false;
		}

	  protected:
		template<typename event_count, benchmark_types, uint64_t count> friend struct event_collector_type;
		std::optional<uint64_t> bytes_processed_val{};
		std::optional<nanoseconds> elapsed_ns_val{};
		std::optional<double> cuda_event_ms_val{};
		std::optional<uint64_t> cycles_val{};
	};

	template<uint64_t count, benchmark_types benchmark_type> using event_collector = event_collector_type<event_count<benchmark_type>, benchmark_type, count>;

}
