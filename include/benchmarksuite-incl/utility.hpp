/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/utility.hpp
 */

#pragma once

#include <benchmarksuite-incl/concepts.hpp>

namespace benchmarksuite {

	namespace internal {

		consteval bool is_url_unreserved(char value) {
			return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '-' || value == '_' || value == '.' ||
				value == '~';
		}

		template<uint64_t size_val> consteval uint64_t url_encoded_length(const string_literal<size_val>& value) {
			uint64_t total{};
			for (uint64_t index{}; index < value.size(); ++index) {
				total += is_url_unreserved(value[index]) ? 1 : 3;
			}
			return total;
		}

	}

	[[maybe_unused]] BNCH_SWT_HOST std::string get_time() {
		std::string new_time_string{};
		new_time_string.resize(1024);
#if BNCH_SWT_PLATFORM_WINDOWS
		std::time_t result = std::time(nullptr);
		std::tm result_two{};
		localtime_s(&result_two, &result);
#else
		std::time_t result = std::time(nullptr);
		std::tm result_two{ *localtime(&result) };
#endif
		new_time_string.resize(strftime(new_time_string.data(), 1024, "%b %d, %Y", &result_two));
		return new_time_string;
	}

	[[maybe_unused]] BNCH_SWT_HOST std::string url_encode(std::string_view value) {
		std::ostringstream escaped;
		escaped.fill('0');
		escaped << std::hex;

		for (char c: value) {
			if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
				escaped << c;
			} else if (c == ':') {
				escaped << '%' << std::setw(2) << static_cast<int32_t>(static_cast<unsigned char>(' '));
			} else {
				escaped << '%' << std::setw(2) << static_cast<int32_t>(static_cast<unsigned char>(c));
			}
		}

		return escaped.str();
	}

	template<string_literal value> consteval auto url_encode_literal() {
		constexpr char hex_digits[]{ "0123456789abcdef" };
		string_literal<internal::url_encoded_length(value) + 1> result{};
		uint64_t write_index{};
		for (uint64_t index{}; index < value.size(); ++index) {
			const char current = value[index];
			if (internal::is_url_unreserved(current)) {
				result[write_index++] = current;
			} else {
				const auto byte_val	  = static_cast<unsigned char>(current == ':' ? ' ' : current);
				result[write_index++] = '%';
				result[write_index++] = hex_digits[byte_val >> 4];
				result[write_index++] = hex_digits[byte_val & 0xF];
			}
		}
		return result;
	}

	inline constexpr decltype(auto) current_path{ []() consteval {
		return internal::operating_system_name + "-" + internal::compiler_id;
	}() };

	[[maybe_unused]] BNCH_SWT_HOST consteval decltype(current_path) get_current_path_impl() {
		return current_path;
	}

	[[maybe_unused]] BNCH_SWT_HOST int32_t execute_python_script(std::string_view script_path, std::string_view argument_01, std::string_view argument_02) {
#if BNCH_SWT_PLATFORM_WINDOWS
		static constexpr std::string_view python_name{ "python" };
#else
		static constexpr std::string_view python_name{ "python3" };
#endif
		auto quote = [](std::string_view s) {
			std::string out{ "\"" };
			for (char c: s) {
				if (c == '"' || c == '\\') {
					out += '\\';
				}
				out += c;
			}
			out += '"';
			return out;
		};
		std::string command{ python_name };
		command += ' ';
		command += quote(script_path);
		command += ' ';
		command += quote(argument_01);
		command += ' ';
		command += quote(argument_02);
		int32_t raw_result = system(command.c_str());
#if BNCH_SWT_PLATFORM_WINDOWS
		int32_t exit_code = raw_result;
#else
		int32_t exit_code = (raw_result == -1) ? -1 : (WIFEXITED(raw_result) ? WEXITSTATUS(raw_result) : 128 + WTERMSIG(raw_result));
#endif
		if (exit_code != 0) {
			std::cout << "Error: Failed to execute Python script. Command exited with code " << exit_code << std::endl;
		}
		return exit_code;
	}

}
