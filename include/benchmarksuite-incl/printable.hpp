/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/printable.hpp
 */

#pragma once

#include <benchmarksuite-incl/concepts.hpp>

namespace benchmarksuite::internal {

	template<const auto& function, uint64_t current_index = 0, typename variant_type, typename... arg_types>
	BNCH_SWT_HOST constexpr void visit(variant_type&& variant, arg_types&&... args) noexcept {
		if constexpr (current_index < std::variant_size_v<base_t<variant_type>>) {
			variant_type&& variant_new = std::forward<variant_type>(variant);
			if (variant_new.index() == current_index) {
				function(std::get<current_index>(std::forward<variant_type>(variant_new)), std::forward<arg_types>(args)...);
				return;
			}
			visit<function, current_index + 1>(std::forward<variant_type>(variant_new), std::forward<arg_types>(args)...);
		}
	}

	template<internal::printable_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << value;
	}

	template<internal::bool_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << std::boolalpha << value;
	}

	template<internal::string_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << "\"" << value << "\"";
	}

	template<internal::string_view_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << "\"" << value << "\"";
	}

	template<internal::vector_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << "[";
		for (uint64_t x = 0; x < value.size(); ++x) {
			print_value(os, value[x]);
			if (x < value.size() - 1) {
				os << ',';
			}
		}
		os << "]";
	}

	template<internal::map_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << "{";
		uint64_t index{};
		for (auto iter = value.begin(); iter != value.end(); ++iter) {
			print_value(os, iter->first);
			os << ":";
			print_value(os, iter->second);
			if (index < value.size() - 1) {
				os << ",";
			}
			++index;
		}
		os << "}";
	}

	template<internal::variant_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		static constexpr auto lambda = [](auto&& value_new, auto& os_new) {
			print_value(os_new, value_new);
		};
		visit<lambda>(value, os);
	}

	template<internal::optional_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		if (value.has_value()) {
			print_value(os, value.value());
		}
	}

	template<internal::pair_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << "{";
		print_value(os, std::get<0>(value));
		os << ",";
		print_value(os, std::get<1>(value));
		os << "}";
	}

	template<internal::tuple_types value_type, uint64_t index> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		if constexpr (index < std::tuple_size_v<base_t<value_type>>) {
			print_value(os, std::get<index>(value));
			if constexpr (index < std::tuple_size_v<base_t<value_type>> - 1) {
				os << ",";
			}
			print_value<value_type, index + 1>(os, value);
		}
	}

	template<internal::tuple_types value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) noexcept {
		os << "{";
		print_value<value_type, 0>(os, value);
		os << "}";
	}

}

template<benchmarksuite::internal::optional_types value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) noexcept {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::variant_types value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) noexcept {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::vector_types value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) noexcept {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::tuple_types value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) noexcept {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::pair_types value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) noexcept {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::map_types value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) noexcept {
	benchmarksuite::internal::print_value(os, value);
	return os;
}
