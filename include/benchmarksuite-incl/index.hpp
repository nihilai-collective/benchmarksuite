/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/index.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>
#include <benchmarksuite-incl/benchmarksuite_cpu_properties.hpp>
#include <benchmarksuite-incl/benchmarksuite_gpu_properties.hpp>
#include <benchmarksuite-incl/random_generators.hpp>
#include <benchmarksuite-incl/do_not_optimize.hpp>
#include <benchmarksuite-incl/thread_affinity.hpp>
#include <benchmarksuite-incl/string_literal.hpp>
#include <benchmarksuite-incl/event_counter.hpp>
#include <benchmarksuite-incl/cache_clearer.hpp>
#include <benchmarksuite-incl/file_handle.hpp>
#include <benchmarksuite-incl/printable.hpp>
#include <benchmarksuite-incl/system_info.hpp>
#include <benchmarksuite-incl/utility.hpp>

namespace benchmarksuite {

	enum class position_type : uint8_t { win, tie, loss, none };

	struct stage_config_data {
		bool clear_cpu_caches_before_iterations{ true };
		uint64_t measured_iteration_count{ 100 };
		uint64_t max_iteration_count{ 1000 };
		double convergence_threshold{ 1.0 };
		benchmark_types benchmark_type{};
		uint64_t max_time_in_s{ 5 };
		double rse_threshold{ 2.5 };
		uint64_t max_k{ 100000 };
		uint64_t min_k{ 30 };
	};

	struct printable_milliseconds {
		double value;
		static constexpr const char* units{ "ms" };
		BNCH_SWT_HOST printable_milliseconds& operator=(double ns_value) {
			value = ns_value / 1'000'000.0;
			return *this;
		}

		BNCH_SWT_HOST printable_milliseconds(double ns_value = 0) {
			*this = ns_value;
		}

		BNCH_SWT_HOST friend std::ostream& operator<<(std::ostream& os BNCH_SWT_LIFETIME_BOUND, const printable_milliseconds& ms) {
			os << ms.value << units;
			return os;
		}
	};

	struct library_benchmark_data {
		std::optional<double> final_cache_references_per_byte;
		std::optional<double> final_branch_misses_per_byte;
		std::optional<double> final_instructions_per_byte;
		std::optional<double> final_cache_misses_per_byte;
		std::optional<double> final_branches_per_byte;
		std::optional<double> final_cycles_per_byte;
		printable_milliseconds final_ms_spent;
		uint64_t final_sample_size;
		double final_rse{ 100.0 };
		uint64_t bytes_processed;
		double final_throughput;
		double final_variance;
		double final_mean;
		bool converged;
	};

	struct library_completion_data : public library_benchmark_data {
		benchmarksuite::position_type position_type_val{ benchmarksuite::position_type::none };
		std::string_view name;
		uint64_t position;
		BNCH_SWT_HOST bool operator>(const library_completion_data& rhs) const {
			return position > rhs.position;
		}
	};

	BNCH_SWT_HOST double now_in_ns() {
		return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(benchmarksuite::clock_type::now().time_since_epoch()).count());
	}

	BNCH_SWT_HOST static library_benchmark_data calculate_epoch_stats(const iteration_metrics* iteration_data, uint64_t iterations, uint64_t k) {
		if (k == 0) {
			return {};
		}
		uint64_t start_idx = iterations - k;

		double time_sum					= 0.0;
		uint64_t total_bytes			= 0;
		uint64_t total_cycles			= 0;
		uint64_t total_instructions		= 0;
		uint64_t total_branches			= 0;
		uint64_t total_branch_misses	= 0;
		uint64_t total_cache_references = 0;
		uint64_t total_cache_misses		= 0;
		bool has_cycles					= true;
		bool has_instructions			= true;
		bool has_branches				= true;
		bool has_branch_misses			= true;
		bool has_cache_references		= true;
		bool has_cache_misses			= true;
		for (uint64_t i = start_idx; i < iterations; ++i) {
			time_sum += iteration_data[i].time_in_ns;
			total_bytes += iteration_data[i].bytes_processed;
			if (iteration_data[i].cycles.has_value()) {
				total_cycles += iteration_data[i].cycles.value();
			} else {
				has_cycles = false;
			}
			if (iteration_data[i].instructions_val.has_value()) {
				total_instructions += iteration_data[i].instructions_val.value();
			} else {
				has_instructions = false;
			}
			if (iteration_data[i].branches_val.has_value()) {
				total_branches += iteration_data[i].branches_val.value();
			} else {
				has_branches = false;
			}
			if (iteration_data[i].branch_misses_val.has_value()) {
				total_branch_misses += iteration_data[i].branch_misses_val.value();
			} else {
				has_branch_misses = false;
			}
			if (iteration_data[i].cache_references_val.has_value()) {
				total_cache_references += iteration_data[i].cache_references_val.value();
			} else {
				has_cache_references = false;
			}
			if (iteration_data[i].cache_misses_val.has_value()) {
				total_cache_misses += iteration_data[i].cache_misses_val.value();
			} else {
				has_cache_misses = false;
			}
		}

		double mean = time_sum / static_cast<double>(k);

		double variance_sum = 0.0;
		for (uint64_t i = start_idx; i < iterations; ++i) {
			double diff = iteration_data[i].time_in_ns - mean;
			variance_sum += diff * diff;
		}

		double variance = 0.0;
		if (k > 1) {
			variance = variance_sum / static_cast<double>(k - 1);
		}

		double std_dev	 = std::sqrt(variance);
		double std_error = std_dev / std::sqrt(static_cast<double>(k));
		double rse		 = (mean > 0.0) ? (std_error / mean) * 100.0 : 0.0;

		double total_time_seconds = time_sum / 1'000'000'000.0;
		double total_mebibytes	  = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
		double throughput		  = (total_time_seconds > 0.0) ? (total_mebibytes / total_time_seconds) : 0.0;

		library_benchmark_data results{};
		results.final_sample_size = k;
		results.final_mean		  = mean;
		results.final_variance	  = variance;
		results.final_rse		  = rse;
		results.final_throughput  = throughput;
		results.converged		  = false;
		results.bytes_processed	  = (total_bytes + (k / 2)) / k;
		if (has_cycles && total_bytes > 0) {
			results.final_cycles_per_byte = static_cast<double>(total_cycles) / static_cast<double>(total_bytes);
		}
		if (has_instructions && total_bytes > 0) {
			results.final_instructions_per_byte = static_cast<double>(total_instructions) / static_cast<double>(total_bytes);
		}
		if (has_branches && total_bytes > 0) {
			results.final_branches_per_byte = static_cast<double>(total_branches) / static_cast<double>(total_bytes);
		}
		if (has_branch_misses && total_bytes > 0) {
			results.final_branch_misses_per_byte = static_cast<double>(total_branch_misses) / static_cast<double>(total_bytes);
		}
		if (has_cache_references && total_bytes > 0) {
			results.final_cache_references_per_byte = static_cast<double>(total_cache_references) / static_cast<double>(total_bytes);
		}
		if (has_cache_misses && total_bytes > 0) {
			results.final_cache_misses_per_byte = static_cast<double>(total_cache_misses) / static_cast<double>(total_bytes);
		}

		return results;
	}

	template<auto function> struct function_holder {
		template<typename... arg_types> BNCH_SWT_HOST static uint64_t impl(arg_types&&... args) {
			return function(std::forward<arg_types>(args)...);
		}
	};

	template<typename functor_type, stage_config_data stage_config, typename... arg_types>
	[[maybe_unused]] BNCH_SWT_HOST static library_benchmark_data impl(iteration_metrics* iteration_data, uint64_t iterations, arg_types&&... args) {
		static_assert(stage_config.max_iteration_count > 0, "MAX ITERATIONS MUST BE GREATER THAN ZERO TO ALLOCATE THE BUFFER");
		static_assert(stage_config.measured_iteration_count > 0, "MEASURED ITERATION COUNT MUST START AT GREATER THAN ZERO");
		static_assert(stage_config.measured_iteration_count <= stage_config.max_iteration_count, "INITIAL WORKLOAD STEPS CANNOT EXCEED THE ABSOLUTE MAXIMUM BUFFER LIMIT");
		static_assert(stage_config.min_k <= stage_config.max_k, "MINIMUM WINDOW SIZE FLOOR (MIN_K) CANNOT BE GREATER THAN THE CEILING (MAX_K)");
		static_assert(stage_config.min_k <= stage_config.max_iteration_count, "STATISTICAL WINDOW FLOOR CANNOT BE LARGER THAN THE TOTAL ALLOCATED MEMORY BUFFER");
		static_assert(stage_config.min_k > 1, "MIN_K MUST BE GREATER THAN 1 TO ALLOW VALID BESSEL CORRECTION VARIANCE MATH (K - 1)");
		static_assert(stage_config.rse_threshold > 0.0, "RELATIVE STANDARD ERROR THRESHOLD (PERCENT) MUST BE POSITIVE");
		static_assert(stage_config.convergence_threshold >= 0.0, "CONVERGENCE THRESHOLD (PERCENT) CANNOT BE NEGATIVE");
		static_assert(stage_config.max_time_in_s > 0, "MAX TIME LIMIT MUST BE GREATER THAN ZERO");

		if (iterations > stage_config.max_iteration_count) {
			iterations = stage_config.max_iteration_count;
		}

		for (uint64_t x = 0; x < iterations; ++x) {
			auto& metrics = iteration_data[x];
			metrics		  = iteration_metrics{};
			functor_type::impl(metrics, args...);
		}

		uint64_t k = std::max(iterations / 10, stage_config.min_k);
		k		   = std::min(k, stage_config.max_k);

		if (k > iterations) {
			k = iterations;
		}

		return calculate_epoch_stats(iteration_data, iterations, k);
	}

	struct test_data {
		std::unordered_map<std::string_view, library_benchmark_data> results{};
		std::string_view test_name{};
	};

	template<stage_config_data stage_config> struct stage_results_data;
	struct stage_data;

	template<benchmark_types benchmark_type> struct final_test_results {
		using system_info = system_info_data<benchmark_type>;

		BNCH_SWT_HOST final_test_results(std::string_view stage_name_new) : stage_name_str{ stage_name_new } {
		}

		BNCH_SWT_HOST std::string to_csv(bool include_preamble = true, const std::string& file_path = "") const {
			const column_flags flags = detect_columns();
			std::ostringstream ss{};
			if (include_preamble) {
				write_csv_preamble(ss);
			}
			write_csv_header(ss, flags.cycles, flags.instructions, flags.branches, flags.branch_misses, flags.cache_references, flags.cache_misses);
			ss << '\n';
			for (const auto& r: sorted_results) {
				write_csv_line(ss, r, flags.cycles, flags.instructions, flags.branches, flags.branch_misses, flags.cache_references, flags.cache_misses);
			}
			std::string text = std::move(ss).str();

			if (!file_path.empty()) {
				file_handle::save_file(text, file_path + "/" + output_file_stem() + ".csv");
			}

			return text;
		}

		BNCH_SWT_HOST std::string to_markdown(bool include_preamble = true, bool include_test_title = true, const std::string& file_path = "") const {
			const column_flags flags = detect_columns();
			std::ostringstream ss{};
			if (include_test_title) {
				ss << "### " << test_name << " Test Results\n\n";
			}
			if (include_preamble) {
				write_md_preamble(ss);
			}
			write_md_header(ss, flags.cycles, flags.instructions, flags.branches, flags.branch_misses, flags.cache_references, flags.cache_misses);
			ss << '\n';
			write_md_separator(ss, flags.cycles, flags.instructions, flags.branches, flags.branch_misses, flags.cache_references, flags.cache_misses);
			ss << '\n';
			for (const auto& r: sorted_results) {
				write_md_row(ss, r, flags.cycles, flags.instructions, flags.branches, flags.branch_misses, flags.cache_references, flags.cache_misses);
			}
			std::string text = std::move(ss).str();

			if (!file_path.empty()) {
				file_handle::save_file(text, file_path + "/" + output_file_stem() + ".md");
			}

			return text;
		}

		BNCH_SWT_HOST void print(bool include_preamble = true) const {
			std::cout << to_markdown(include_preamble);
		}

		BNCH_SWT_HOST uint64_t size() const {
			return sorted_results.size();
		}

		BNCH_SWT_HOST std::string_view get_test_name() const {
			return test_name;
		}

		BNCH_SWT_HOST std::string_view get_stage_name() const {
			return stage_name_str;
		}

		BNCH_SWT_HOST bool converged() const {
			return all_converged;
		}

	  protected:
		template<string_literal stage_name_newer, benchmark_types benchmark_type_new>
		friend final_test_results<benchmark_type_new> process_test_rankings(const test_data& raw_data);
		template<string_literal stage_name_newer, stage_config_data stage_config_new>
		friend stage_results_data<stage_config_new> process_stage_rankings(const stage_data& raw_data);
		std::vector<library_completion_data> sorted_results{};
		std::string_view stage_name_str{};
		std::string_view test_name{};
		bool all_converged{};

		struct column_flags {
			bool cycles{};
			bool instructions{};
			bool branches{};
			bool branch_misses{};
			bool cache_references{};
			bool cache_misses{};
		};

		BNCH_SWT_HOST column_flags detect_columns() const {
			column_flags flags{};
			for (const auto& r: sorted_results) {
				flags.cycles |= r.final_cycles_per_byte.has_value();
				flags.instructions |= r.final_instructions_per_byte.has_value();
				flags.branches |= r.final_branches_per_byte.has_value();
				flags.branch_misses |= r.final_branch_misses_per_byte.has_value();
				flags.cache_references |= r.final_cache_references_per_byte.has_value();
				flags.cache_misses |= r.final_cache_misses_per_byte.has_value();
			}
			return flags;
		}

		BNCH_SWT_HOST std::string output_file_stem() const {
			std::string safe_name = static_cast<std::string>(test_name);
			std::replace(safe_name.begin(), safe_name.end(), '.', '_');
			std::replace(safe_name.begin(), safe_name.end(), '/', '_');
			std::replace(safe_name.begin(), safe_name.end(), '\\', '_');
			std::replace(safe_name.begin(), safe_name.end(), ' ', '_');
			std::string file_name{ internal::operating_system_name.operator std::string() + "-" + internal::compiler_id.operator std::string() };
			return file_name + "-" + static_cast<std::string>(stage_name_str) + "-" + safe_name;
		}

		BNCH_SWT_HOST void write_csv_preamble(std::ostream& ss) const {
			ss << "# " << test_name << " Test Results \n";
			ss << "#**" << system_info::device_type << ":** " << system_info::device_name() << '\n';
			ss << "#**OS:** " << system_info::os_id << "-" << system_info::os_version << '\n';
			ss << "#**Compiler:** " << system_info::compiler_id << "-" << system_info::compiler_version << "\n\n";
		}

		BNCH_SWT_HOST std::string csv_preamble() const {
			std::ostringstream ss{};
			write_csv_preamble(ss);
			return std::move(ss).str();
		}

		BNCH_SWT_HOST static void write_csv_header(std::ostream& ss, bool include_cycles_per_byte, bool include_instructions_per_byte, bool include_branches_per_byte,
			bool include_branch_misses_per_byte, bool include_cache_references_per_byte, bool include_cache_misses_per_byte) {
			ss << "Library,Throughput (MB/s),RSE (%),Window Duration,File Size (Bytes),Window Samples (k),Variance,Latency / Run (ns)";
			if (include_cycles_per_byte) {
				ss << ",Cycles/Byte";
			}
			if (include_instructions_per_byte) {
				ss << ",Instructions/Byte";
			}
			if (include_branches_per_byte) {
				ss << ",Branches/Byte";
			}
			if (include_branch_misses_per_byte) {
				ss << ",Branch Misses/Byte";
			}
			if (include_cache_references_per_byte) {
				ss << ",Cache References/Byte";
			}
			if (include_cache_misses_per_byte) {
				ss << ",Cache Misses/Byte";
			}
			ss << ",Position";
		}

		BNCH_SWT_HOST static std::string csv_header(bool include_cycles_per_byte, bool include_instructions_per_byte = false, bool include_branches_per_byte = false,
			bool include_branch_misses_per_byte = false, bool include_cache_references_per_byte = false, bool include_cache_misses_per_byte = false) {
			std::ostringstream ss{};
			write_csv_header(ss, include_cycles_per_byte, include_instructions_per_byte, include_branches_per_byte, include_branch_misses_per_byte,
				include_cache_references_per_byte, include_cache_misses_per_byte);
			return std::move(ss).str();
		}

		template<typename value_type> BNCH_SWT_HOST static void write_optional_cell(std::ostream& ss, const char* separator, bool include, const std::optional<value_type>& value) {
			if (include) {
				ss << separator;
				if (value.has_value()) {
					ss << value.value();
				}
			}
		}

		BNCH_SWT_HOST static void write_csv_line(std::ostream& ss, const library_completion_data& r, bool include_cycles_per_byte, bool include_instructions_per_byte,
			bool include_branches_per_byte, bool include_branch_misses_per_byte, bool include_cache_references_per_byte, bool include_cache_misses_per_byte) {
			ss << r.name;
			ss << ',' << r.final_throughput;
			ss << ',' << r.final_rse;
			ss << ',' << r.final_ms_spent;
			ss << ',' << r.bytes_processed;
			ss << ',' << r.final_sample_size;
			ss << ',' << r.final_variance;
			ss << ',' << r.final_mean;
			write_optional_cell(ss, ",", include_cycles_per_byte, r.final_cycles_per_byte);
			write_optional_cell(ss, ",", include_instructions_per_byte, r.final_instructions_per_byte);
			write_optional_cell(ss, ",", include_branches_per_byte, r.final_branches_per_byte);
			write_optional_cell(ss, ",", include_branch_misses_per_byte, r.final_branch_misses_per_byte);
			write_optional_cell(ss, ",", include_cache_references_per_byte, r.final_cache_references_per_byte);
			write_optional_cell(ss, ",", include_cache_misses_per_byte, r.final_cache_misses_per_byte);
			switch (static_cast<uint64_t>(r.position_type_val)) {
				case static_cast<uint64_t>(position_type::win):
					ss << ",Win";
					break;
				case static_cast<uint64_t>(position_type::tie):
					ss << ",Tie";
					break;
				case static_cast<uint64_t>(position_type::loss):
					ss << ",Loss";
					break;
				case static_cast<uint64_t>(position_type::none):
					[[fallthrough]];
				default:
					ss << (r.converged ? "," : ",DNC");
					break;
			}
			ss << '\n';
		}

		BNCH_SWT_HOST static std::string result_to_csv_line(const library_completion_data& r, bool include_cycles_per_byte, bool include_instructions_per_byte = false,
			bool include_branches_per_byte = false, bool include_branch_misses_per_byte = false, bool include_cache_references_per_byte = false,
			bool include_cache_misses_per_byte = false) {
			std::ostringstream ss{};
			write_csv_line(ss, r, include_cycles_per_byte, include_instructions_per_byte, include_branches_per_byte, include_branch_misses_per_byte,
				include_cache_references_per_byte, include_cache_misses_per_byte);
			return std::move(ss).str();
		}

		BNCH_SWT_HOST void write_md_preamble(std::ostream& ss) const {
			ss << "**" << system_info::device_type << ":** " << system_info::device_name() << "  \n";
			ss << "**OS:** " << system_info::os_id << "-" << system_info::os_version << "  \n";
			ss << "**Compiler:** " << system_info::compiler_id << "-" << system_info::compiler_version << "  \n\n";
		}

		BNCH_SWT_HOST std::string md_preamble() const {
			std::ostringstream ss{};
			write_md_preamble(ss);
			return std::move(ss).str();
		}

		BNCH_SWT_HOST static void write_md_header(std::ostream& ss, bool include_cycles_per_byte, bool include_instructions_per_byte, bool include_branches_per_byte,
			bool include_branch_misses_per_byte, bool include_cache_references_per_byte, bool include_cache_misses_per_byte) {
			ss << "| Library | Throughput (MB/s) | RSE (%) | Window Duration | File Size (Bytes) | Window Samples (k) | Variance | Latency / Run (ns)";
			if (include_cycles_per_byte) {
				ss << " | Cycles/Byte";
			}
			if (include_instructions_per_byte) {
				ss << " | Instructions/Byte";
			}
			if (include_branches_per_byte) {
				ss << " | Branches/Byte";
			}
			if (include_branch_misses_per_byte) {
				ss << " | Branch Misses/Byte";
			}
			if (include_cache_references_per_byte) {
				ss << " | Cache References/Byte";
			}
			if (include_cache_misses_per_byte) {
				ss << " | Cache Misses/Byte";
			}
			ss << " | Position |";
		}

		BNCH_SWT_HOST static std::string md_header_row(bool include_cycles_per_byte, bool include_instructions_per_byte = false, bool include_branches_per_byte = false,
			bool include_branch_misses_per_byte = false, bool include_cache_references_per_byte = false, bool include_cache_misses_per_byte = false) {
			std::ostringstream ss{};
			write_md_header(ss, include_cycles_per_byte, include_instructions_per_byte, include_branches_per_byte, include_branch_misses_per_byte,
				include_cache_references_per_byte, include_cache_misses_per_byte);
			return std::move(ss).str();
		}

		BNCH_SWT_HOST static void write_md_separator(std::ostream& ss, bool include_cycles_per_byte, bool include_instructions_per_byte, bool include_branches_per_byte,
			bool include_branch_misses_per_byte, bool include_cache_references_per_byte, bool include_cache_misses_per_byte) {
			ss << "| ------- | ----------- | ------- | --------- | --------------- | -------------------- | ---------- | ----";
			const bool includes[]{ include_cycles_per_byte, include_instructions_per_byte, include_branches_per_byte, include_branch_misses_per_byte,
				include_cache_references_per_byte, include_cache_misses_per_byte };
			for (bool include: includes) {
				if (include) {
					ss << " | -----------";
				}
			}
			ss << " | -------- |";
		}

		BNCH_SWT_HOST static std::string md_separator_row(bool include_cycles_per_byte, bool include_instructions_per_byte = false, bool include_branches_per_byte = false,
			bool include_branch_misses_per_byte = false, bool include_cache_references_per_byte = false, bool include_cache_misses_per_byte = false) {
			std::ostringstream ss{};
			write_md_separator(ss, include_cycles_per_byte, include_instructions_per_byte, include_branches_per_byte, include_branch_misses_per_byte,
				include_cache_references_per_byte, include_cache_misses_per_byte);
			return std::move(ss).str();
		}

		BNCH_SWT_HOST static void write_md_row(std::ostream& ss, const library_completion_data& r, bool include_cycles_per_byte, bool include_instructions_per_byte,
			bool include_branches_per_byte, bool include_branch_misses_per_byte, bool include_cache_references_per_byte, bool include_cache_misses_per_byte) {
			ss << "| " << r.name;
			if (r.position_type_val == position_type::tie) {
				ss << " STATISTICAL TIE";
			}
			if (!r.converged) {
				ss << " DID NOT CONVERGE";
			}
			ss << " | " << r.final_throughput;
			ss << " | " << r.final_rse;
			ss << " | " << r.final_ms_spent;
			ss << " | " << r.bytes_processed;
			ss << " | " << r.final_sample_size;
			ss << " | " << r.final_variance;
			ss << " | " << r.final_mean;
			write_optional_cell(ss, " | ", include_cycles_per_byte, r.final_cycles_per_byte);
			write_optional_cell(ss, " | ", include_instructions_per_byte, r.final_instructions_per_byte);
			write_optional_cell(ss, " | ", include_branches_per_byte, r.final_branches_per_byte);
			write_optional_cell(ss, " | ", include_branch_misses_per_byte, r.final_branch_misses_per_byte);
			write_optional_cell(ss, " | ", include_cache_references_per_byte, r.final_cache_references_per_byte);
			write_optional_cell(ss, " | ", include_cache_misses_per_byte, r.final_cache_misses_per_byte);
			if (r.converged) {
				ss << " | " << r.position;
			} else {
				ss << " | -";
			}
			switch (static_cast<uint64_t>(r.position_type_val)) {
				case static_cast<uint64_t>(position_type::win):
					ss << "(Win)";
					break;
				case static_cast<uint64_t>(position_type::tie):
					ss << "(Tie)";
					break;
				case static_cast<uint64_t>(position_type::loss):
					ss << "(Loss)";
					break;
				case static_cast<uint64_t>(position_type::none):
					[[fallthrough]];
				default:
					break;
			}
			ss << " |\n";
		}

		BNCH_SWT_HOST static std::string result_to_md_row(const library_completion_data& r, bool include_cycles_per_byte, bool include_instructions_per_byte = false,
			bool include_branches_per_byte = false, bool include_branch_misses_per_byte = false, bool include_cache_references_per_byte = false,
			bool include_cache_misses_per_byte = false) {
			std::ostringstream ss{};
			write_md_row(ss, r, include_cycles_per_byte, include_instructions_per_byte, include_branches_per_byte, include_branch_misses_per_byte,
				include_cache_references_per_byte, include_cache_misses_per_byte);
			return std::move(ss).str();
		}
	};

	struct stage_data {
		std::vector<test_data> results{};

		test_data* find(std::string_view test_name) {
			for (auto& entry: results) {
				if (entry.test_name == test_name) {
					return &entry;
				}
			}
			return nullptr;
		}

		test_data& get_or_create(std::string_view test_name) {
			if (test_data* existing = find(test_name)) {
				return *existing;
			}
			test_data& created = results.emplace_back();
			created.test_name  = test_name;
			return created;
		}
	};

	struct library_positions {
		std::string_view lib_name{};
		uint64_t losses{};
		uint64_t wins{};
		uint64_t ties{};
		bool operator>(const library_positions& rhs) const {
			if (wins != rhs.wins) {
				return wins > rhs.wins;
			}
			if (ties != rhs.ties) {
				return ties > rhs.ties;
			}
			if (losses != rhs.losses) {
				return losses < rhs.losses;
			}
			return lib_name < rhs.lib_name;
		}
	};

	template<stage_config_data stage_config> struct stage_results_data {
		using system_info = system_info_data<stage_config.benchmark_type>;

		BNCH_SWT_HOST stage_results_data(std::string_view stage_name_new) : stage_name_str{ stage_name_new } {
		}

		BNCH_SWT_HOST std::string to_csv(const std::string& file_path = "") const {
			std::stringstream ss{};
			ss << csv_preamble();
			ss << "# " << stage_name_str << " Stage Config" << std::endl << std::endl;
			ss << config_csv_header() << std::endl;
			ss << config_to_csv_line();
			ss << std::endl;
			ss << csv_header() << std::endl;
			for (const auto& lib: lib_positions) {
				ss << accum_to_csv_line(lib);
			}
			const std::string text = ss.str();
			if (!file_path.empty()) {
				std::string file_name{ internal::operating_system_name.operator std::string() + "-" + internal::compiler_id.operator std::string() };
				file_handle::save_file(text, file_path + "/" + file_name + "-" + static_cast<std::string>(stage_name_str) + ".csv");
			}
			return text;
		}

	  protected:
		template<string_literal stage_name_newer, benchmark_types benchmark_type_new>
		friend final_test_results<benchmark_type_new> process_test_rankings(const test_data& raw_data);
		template<string_literal stage_name_newer, stage_config_data stage_config_new>
		friend stage_results_data<stage_config_new> process_stage_rankings(const stage_data& raw_data);
		std::vector<final_test_results<stage_config.benchmark_type>> results{};
		std::vector<library_positions> lib_positions{};
		std::string_view stage_name_str{};

		BNCH_SWT_HOST std::string csv_preamble() const {
			std::stringstream ss{};
			ss << "# " << stage_name_str << " Stage Results" << std::endl;
			ss << "#" << system_info::device_type << ": " << system_info::device_name() << std::endl;
			ss << "#OS: " << system_info::os_id << "-" << system_info::os_version << std::endl;
			ss << "#Compiler: " << system_info::compiler_id << "-" << system_info::compiler_version << std::endl << std::endl;
			return ss.str();
		}

		BNCH_SWT_HOST static std::string config_csv_header() {
			std::string h = "ClearCpuCachesBeforeIterations";
			h += ",MeasuredIterationCount";
			h += ",MaxIterationCount";
			h += ",ConvergenceThreshold (%)";
			h += ",MaxTimeInS";
			h += ",RseThreshold (%)";
			h += ",MaxK";
			h += ",MinK";
			return h;
		}

		BNCH_SWT_HOST static std::string config_to_csv_line() {
			std::stringstream ss{};
			ss << (stage_config.clear_cpu_caches_before_iterations ? "true" : "false") << ",";
			ss << stage_config.measured_iteration_count << ",";
			ss << stage_config.max_iteration_count << ",";
			ss << stage_config.convergence_threshold << ",";
			ss << stage_config.max_time_in_s << ",";
			ss << stage_config.rse_threshold << ",";
			ss << stage_config.max_k << ",";
			ss << stage_config.min_k << std::endl;
			return ss.str();
		}

		BNCH_SWT_HOST static std::string csv_header() {
			std::string h = "Library";
			h += ",Wins";
			h += ",Ties";
			h += ",Losses";
			return h;
		}

		BNCH_SWT_HOST std::string accum_to_csv_line(const library_positions& lib) const {
			std::stringstream ss{};
			ss << lib.lib_name << "," << lib.wins << "," << lib.ties << "," << lib.losses << std::endl;
			return ss.str();
		}
	};

	template<typename functor_type_new, stage_config_data stage_config, typename... arg_types> BNCH_SWT_HOST library_benchmark_data run_adaptive_benchmark(arg_types&&... args) {
		static constexpr double max_time_in_ns{ static_cast<double>(stage_config.max_time_in_s) * 1e9 };
		using functor_type = internal::iteration_metric_collector<stage_config.benchmark_type, functor_type_new>;
		std::vector<iteration_metrics> iteration_data(stage_config.max_iteration_count);
		uint64_t target_count		  = stage_config.measured_iteration_count;
		uint64_t total_iterations_run = 0;
		double start_time_ns		  = now_in_ns();
		library_benchmark_data current_stats{};
		double previous_mean = -1.0;
		double elapsed_time	 = 0;

		while (true) {
			if (target_count > stage_config.max_iteration_count - total_iterations_run) {
				target_count = stage_config.max_iteration_count - total_iterations_run;
			}

			current_stats = impl<functor_type, stage_config>(iteration_data.data(), target_count, args...);
			total_iterations_run += target_count;

			elapsed_time = now_in_ns() - start_time_ns;

			bool rse_converged	= (current_stats.final_rse <= stage_config.rse_threshold) && (current_stats.final_mean > 0.0);
			bool mean_converged = false;

			if (previous_mean > 0.0) {
				double mean_delta		 = std::abs(current_stats.final_mean - previous_mean);
				double percentage_change = (mean_delta / previous_mean) * 100.0;
				if (percentage_change <= stage_config.convergence_threshold) {
					mean_converged = true;
				}
			}

			if (rse_converged && mean_converged) {
				current_stats.converged		 = true;
				current_stats.final_ms_spent = elapsed_time;
				return current_stats;
			}

			bool time_exhausted		  = elapsed_time >= max_time_in_ns;
			bool iterations_exhausted = total_iterations_run >= stage_config.max_iteration_count;

			if (time_exhausted || iterations_exhausted) {
				current_stats.converged		 = false;
				current_stats.final_ms_spent = elapsed_time;
				return current_stats;
			}

			previous_mean = current_stats.final_mean;

			target_count *= 2;
			if (target_count > stage_config.max_iteration_count) {
				target_count = stage_config.max_iteration_count;
			}
		}
	}

	BNCH_SWT_HOST static bool is_statistically_tied(const library_benchmark_data& a, const library_benchmark_data& b) {
		double n1 = static_cast<double>(a.final_sample_size);
		double n2 = static_cast<double>(b.final_sample_size);

		if (n1 <= 1.0 || n2 <= 1.0) {
			return true;
		}

		double v1 = a.final_variance;
		double v2 = b.final_variance;

		double se1		   = v1 / n1;
		double se2		   = v2 / n2;
		double denominator = std::sqrt(se1 + se2);

		if (denominator == 0.0) {
			return !(a.final_mean < b.final_mean) && !(b.final_mean < a.final_mean);
		}

		double t_stat = std::abs(a.final_mean - b.final_mean) / denominator;

		double num = (se1 + se2) * (se1 + se2);
		double den = ((se1 * se1) / (n1 - 1.0)) + ((se2 * se2) / (n2 - 1.0));
		double df  = num / den;

		double t_critical = 1.95996 + (2.4132 / df) + (4.4212 / (df * df));

		return t_stat < t_critical;
	}

	template<string_literal stage_name_new, benchmark_types benchmark_type> BNCH_SWT_HOST final_test_results<benchmark_type> process_test_rankings(const test_data& raw_data) {
		static constexpr string_literal stage_name{ stage_name_new };
		std::vector<library_completion_data> leaderboard;
		leaderboard.reserve(raw_data.results.size());
		final_test_results<benchmark_type> return_value{ stage_name.operator std::string_view() };

		return_value.test_name = raw_data.test_name;
		bool all_converged{ true };
		for (const auto& [lib_name, bench]: raw_data.results) {
			if (!bench.converged) {
				all_converged = false;
				break;
			}
		}

		if (!all_converged) {
			return return_value;
		}

		return_value.all_converged = true;
		for (const auto& [lib_name, bench]: raw_data.results) {
			library_completion_data item{};
			static_cast<library_benchmark_data&>(item) = bench;
			item.name								   = lib_name;
			leaderboard.push_back(item);
		}

		std::sort(leaderboard.begin(), leaderboard.end(), [](const library_completion_data& a, const library_completion_data& b) {
			return a.final_throughput > b.final_throughput;
		});

		if (!leaderboard.empty()) {
			leaderboard[0].position = 1;
			size_t group_start		= 0;

			for (size_t i = 1; i < leaderboard.size(); ++i) {
				if (is_statistically_tied(leaderboard[group_start], leaderboard[i])) {
					leaderboard[i].position = leaderboard[group_start].position;
				} else {
					group_start				= i;
					leaderboard[i].position = i + 1;
				}
			}

			for (size_t i = 0; i < leaderboard.size(); ++i) {
				uint64_t r = leaderboard[i].position;

				bool ties_with_someone = false;
				if (i > 0 && leaderboard[i - 1].position == r) {
					ties_with_someone = true;
				}
				if (i < leaderboard.size() - 1 && leaderboard[i + 1].position == r) {
					ties_with_someone = true;
				}

				if (r == 1) {
					leaderboard[i].position_type_val = ties_with_someone ? benchmarksuite::position_type::tie : benchmarksuite::position_type::win;
				} else {
					leaderboard[i].position_type_val = ties_with_someone ? benchmarksuite::position_type::tie : benchmarksuite::position_type::loss;
				}
			}
		}
		return_value.sorted_results = std::move(leaderboard);
		return return_value;
	}

	template<string_literal stage_name_new, stage_config_data stage_config> BNCH_SWT_HOST stage_results_data<stage_config> process_stage_rankings(const stage_data& raw_data) {
		static constexpr string_literal stage_name{ stage_name_new };
		std::vector<final_test_results<stage_config.benchmark_type>> leaderboard;
		leaderboard.reserve(raw_data.results.size());
		std::unordered_map<std::string_view, library_positions> lib_positions{};
		stage_results_data<stage_config> return_value{ stage_name.operator std::string_view() };

		for (const auto& bench: raw_data.results) {
			leaderboard.push_back(process_test_rankings<stage_name_new, stage_config.benchmark_type>(bench));
		}

		for (size_t i = 0; i < leaderboard.size(); ++i) {
			for (auto& value: leaderboard[i].sorted_results) {
				library_completion_data& val_temp = value;
				if (val_temp.position_type_val == position_type::win) {
					++lib_positions[val_temp.name].wins;
				} else if (val_temp.position_type_val == position_type::loss) {
					++lib_positions[val_temp.name].losses;
				} else if (val_temp.position_type_val == position_type::tie) {
					++lib_positions[val_temp.name].ties;
				}
			}
		}
		for (auto& [k, v]: lib_positions) {
			library_positions lib_pos{};
			lib_pos.lib_name = k;
			lib_pos.wins	 = v.wins;
			lib_pos.ties	 = v.ties;
			lib_pos.losses	 = v.losses;
			return_value.lib_positions.emplace_back(lib_pos);
		}
		std::sort(return_value.lib_positions.begin(), return_value.lib_positions.end(), std::greater<library_positions>{});
		for (const final_test_results<stage_config.benchmark_type>& entry: leaderboard) {
			if (entry.all_converged) {
				return_value.results.emplace_back(entry);
			}
		}

		return return_value;
	}

	template<string_literal stage_name_new, const stage_config_data stage_config> struct benchmark_stage {
		static constexpr string_literal stage_name{ stage_name_new };
		BNCH_SWT_HOST static stage_data& get_raw_test_data() {
			static stage_data* raw_test_data{ new stage_data{} };
			return *raw_test_data;
		}

		template<benchmarksuite::string_literal test_name_new, benchmarksuite::string_literal library_name_new, typename functor_type, typename... arg_types>
		BNCH_SWT_HOST static void run_benchmark(arg_types&&... args) {
			static constexpr string_literal test_name{ test_name_new };
			static constexpr string_literal library_name{ library_name_new };
			test_data& test_data_val = get_raw_test_data().get_or_create(test_name.operator std::string_view());
			test_data_val.test_name	 = test_name.operator std::string_view();
			if constexpr (stage_config.clear_cpu_caches_before_iterations) {
				internal::cache_clearer<stage_config.benchmark_type> cclearer{};
				cclearer.evict_caches();
			}
			test_data_val.results[library_name.operator std::string_view()] = run_adaptive_benchmark<functor_type, stage_config>(std::forward<arg_types>(args)...);
		}

		template<benchmarksuite::string_literal test_name_new, benchmarksuite::string_literal library_name_new, auto function, typename... arg_types>
		BNCH_SWT_HOST static void run_benchmark(arg_types&&... args) {
			static constexpr string_literal test_name{ test_name_new };
			static constexpr string_literal library_name{ library_name_new };
			using function_type		 = function_holder<function>;
			test_data& test_data_val = get_raw_test_data().get_or_create(test_name.operator std::string_view());
			test_data_val.test_name	 = test_name.operator std::string_view();
			if constexpr (stage_config.clear_cpu_caches_before_iterations) {
				internal::cache_clearer<stage_config.benchmark_type> cclearer{};
				cclearer.evict_caches();
			}
			test_data_val.results[library_name.operator std::string_view()] = run_adaptive_benchmark<function_type, stage_config>(std::forward<arg_types>(args)...);
		}

		BNCH_SWT_HOST static auto get_test_results(const std::string& test_name) {
			const test_data* found = get_raw_test_data().find(test_name);
			if (found == nullptr) {
				return final_test_results<stage_config.benchmark_type>{ stage_name.operator std::string_view() };
			}
			return process_test_rankings<stage_name_new, stage_config.benchmark_type>(*found);
		}

		BNCH_SWT_HOST static std::vector<final_test_results<stage_config.benchmark_type>> get_finished_tests() {
			std::vector<final_test_results<stage_config.benchmark_type>> return_values{};
			const auto& results = get_raw_test_data().results;
			return_values.reserve(results.size());
			for (const auto& bench: results) {
				auto test_results = process_test_rankings<stage_name_new, stage_config.benchmark_type>(bench);
				if (test_results.converged()) {
					return_values.emplace_back(std::move(test_results));
				}
			}
			return return_values;
		}

		BNCH_SWT_HOST static auto get_all_results() {
			return process_stage_rankings<stage_name_new, stage_config>(get_raw_test_data());
		}
	};

}
