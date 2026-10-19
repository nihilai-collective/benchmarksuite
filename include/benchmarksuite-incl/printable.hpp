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
	BNCH_SWT_HOST constexpr void visit(variant_type&& variant, arg_types&&... args) {
		if constexpr (current_index < std::variant_size_v<base_t<variant_type>>) {
			variant_type&& variant_new = std::forward<variant_type>(variant);
			if (variant_new.index() == current_index) {
				function(std::get<current_index>(std::forward<variant_type>(variant_new)), std::forward<arg_types>(args)...);
				return;
			}
			visit<function, current_index + 1>(std::forward<variant_type>(variant_new), std::forward<arg_types>(args)...);
		}
	}

	template<internal::printable value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << value;
	}

	template<internal::bool_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << std::boolalpha << value;
	}

	template<internal::string_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << "\"" << value << "\"";
	}

	template<internal::string_view_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << "\"" << value << "\"";
	}

	template<internal::vector_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << "[";
		for (uint64_t x = 0; x < value.size(); ++x) {
			print_value(os, value[x]);
			if (x < value.size() - 1) {
				os << ',';
			}
		}
		os << "]";
	}

	template<internal::map_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
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

	template<internal::variant_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		static constexpr auto lambda = [](auto&& value_new, auto& os_new) {
			print_value(os_new, value_new);
		};
		visit<lambda>(value, os);
	}

	template<internal::optional_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		if (value.has_value()) {
			print_value(os, value.value());
		}
	}

	template<internal::pair_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << "{";
		print_value(os, std::get<0>(value));
		os << ",";
		print_value(os, std::get<1>(value));
		os << "}";
	}

	template<internal::tuple_t value_type, uint64_t index> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		if constexpr (index < std::tuple_size_v<base_t<value_type>>) {
			print_value(os, std::get<index>(value));
			if constexpr (index < std::tuple_size_v<base_t<value_type>> - 1) {
				os << ",";
			}
			print_value<value_type, index + 1>(os, value);
		}
	}

	template<internal::tuple_t value_type> BNCH_SWT_HOST void print_value(std::ostream& os, const value_type& value) {
		os << "{";
		print_value<value_type, 0>(os, value);
		os << "}";
	}

}

template<benchmarksuite::internal::vector_t value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::map_t value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::variant_t value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::optional_t value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::pair_t value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) {
	benchmarksuite::internal::print_value(os, value);
	return os;
}

template<benchmarksuite::internal::tuple_t value_type> BNCH_SWT_HOST std::ostream& operator<<(std::ostream& os, const value_type& value) {
	benchmarksuite::internal::print_value(os, value);
	return os;
}
