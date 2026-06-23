/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/counters/linux_perf_events.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>
#include <thread>

#if BNCH_SWT_PLATFORM_LINUX

namespace benchmarksuite::internal {

	class linux_events {
	  public:
		std::optional<uint64_t> cache_references_val{};
		std::optional<uint64_t> branch_misses_val{};
		std::optional<uint64_t> instructions_val{};
		std::optional<uint64_t> cache_misses_val{};
		std::optional<uint64_t> branches_val{};
		std::optional<uint64_t> cycles_val{};

		BNCH_SWT_HOST explicit linux_events() : working(true) {
			perf_event_attr attribs{};
			attribs.type		   = PERF_TYPE_HARDWARE;
			attribs.size		   = sizeof(attribs);
			attribs.disabled	   = 1;
			attribs.exclude_kernel = 1;
			attribs.exclude_hv	   = 1;

			attribs.sample_period	  = 0;
			attribs.read_format		  = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
			const int32_t pid		  = 0;
			const int32_t cpu		  = -1;
			const unsigned long flags = 0;

			int32_t group = -1;
			for (uint32_t orig_index = 0; orig_index < config_arrray.size(); ++orig_index) {
				attribs.config = static_cast<long long unsigned int>(config_arrray[orig_index]);
				int32_t _fd	   = static_cast<int32_t>(syscall(__NR_perf_event_open, &attribs, pid, cpu, group, flags));
				if (_fd == -1) {
					continue;
				}
				uint64_t id{};
				if (ioctl(_fd, PERF_EVENT_IOC_ID, &id) == -1) {
					close(_fd);
					continue;
				}
				if (group == -1) {
					group = _fd;
					fd	  = _fd;
				}
				ids[num_events]				= id;
				present_indices[num_events] = orig_index;
				++num_events;
			}
		}

		BNCH_SWT_HOST ~linux_events() {
			if (fd != -1) {
				close(fd);
			}
		}

		BNCH_SWT_HOST void start() {
			if (fd != -1) {
				if (ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1) {
					report_error("ioctl(PERF_EVENT_IOC_RESET)");
				}

				if (ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1) {
					report_error("ioctl(PERF_EVENT_IOC_ENABLE)");
				}
			}
		}

		BNCH_SWT_HOST void end() {
			const uint64_t read_count = num_events * 2 + 1;

			if (fd != -1) {
				if (ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) == -1) {
					report_error("ioctl(PERF_EVENT_IOC_DISABLE)");
				}

				if (read(fd, temp_result_vec.data(), read_count * 8) == -1) {
					report_error("read");
				}
			}

			for (uint64_t i = 1; i < read_count; i += 2) {
				const uint64_t value = temp_result_vec[i];
				switch (present_indices[(i - 1) / 2]) {
					case 0:
						cycles_val.emplace(value);
						break;
					case 1:
						instructions_val.emplace(value);
						break;
					case 2:
						branches_val.emplace(value);
						break;
					case 3:
						branch_misses_val.emplace(value);
						break;
					case 4:
						cache_references_val.emplace(value);
						break;
					case 5:
						cache_misses_val.emplace(value);
						break;
					default:
						break;
				}
			}
			for (uint64_t i = 2; i < read_count; i += 2) {
				if (ids[i / 2 - 1] != temp_result_vec[i]) {
					report_error("event mismatch");
				}
			}
		}

		BNCH_SWT_HOST bool is_working() {
			return working;
		}

		BNCH_SWT_HOST uint64_t event_count() {
			return num_events;
		}

	  protected:
		static constexpr uint64_t max_events{ 6 };

		std::array<uint64_t, max_events * 2 + 1> temp_result_vec{};
		std::array<uint32_t, max_events> present_indices{};
		std::array<uint64_t, max_events> ids{};
		uint64_t num_events{};
		int32_t fd{ -1 };
		bool working{};

		static constexpr std::array<perf_hw_id, max_events> config_arrray{ { PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
			PERF_COUNT_HW_BRANCH_MISSES, PERF_COUNT_HW_CACHE_REFERENCES, PERF_COUNT_HW_CACHE_MISSES } };

		BNCH_SWT_HOST void report_error(std::string_view error) {
			if (working) {
				std::cerr << "Error: " << error << std::endl;
				working = false;
			}
		}
	};

	BNCH_SWT_HOST static uint64_t rdtsc() {
		uint32_t a, d;
		__asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
		return static_cast<unsigned long>(a) | (static_cast<unsigned long>(d) << 32);
	}

	template<benchmark_types benchmark_types, typename function_type> struct iteration_metric_collector {
		template<typename metric_type, typename... arg_types> BNCH_SWT_NOINLINE static void impl(metric_type& iteration_data, arg_types&&... args) {
			[[maybe_unused]] linux_events linux_events_val{};
			if (linux_events_val.event_count() > 0) {
				const auto start_clock = clock_type::now();
				linux_events_val.start();
				iteration_data.bytes_processed = static_cast<uint64_t>(function_type::impl(std::forward<arg_types>(args)...));
				linux_events_val.end();
				iteration_data.cycles				= linux_events_val.cycles_val;
				iteration_data.instructions_val		= linux_events_val.instructions_val;
				iteration_data.branches_val			= linux_events_val.branches_val;
				iteration_data.branch_misses_val	= linux_events_val.branch_misses_val;
				iteration_data.cache_references_val = linux_events_val.cache_references_val;
				iteration_data.cache_misses_val		= linux_events_val.cache_misses_val;
				const auto end_clock				= clock_type::now();
				iteration_data.time_in_ns			= (end_clock - start_clock).count();
			} else {
				const auto start_clock				= clock_type::now();
				const volatile uint64_t cycle_start = rdtsc();
				iteration_data.bytes_processed		= static_cast<uint64_t>(function_type::impl(std::forward<arg_types>(args)...));
				const volatile uint64_t cycle_end	= rdtsc();
				const auto end_clock				= clock_type::now();
				iteration_data.time_in_ns			= (end_clock - start_clock).count();
				iteration_data.cycles.emplace(cycle_end - cycle_start);
			}
		}
	};

}

#endif
