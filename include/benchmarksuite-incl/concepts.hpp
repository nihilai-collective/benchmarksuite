/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/concepts.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

namespace benchmarksuite::internal {

	template<typename value_type>
	concept derivable_types = std::is_class_v<base_t<value_type>> && !std::is_final_v<base_t<value_type>>;

	template<typename value_type>
	concept iterable_types = requires(base_t<value_type> value) {
		{ value.begin() };
		{ value.end() };
	};

	template<typename functor_type, typename... arg_types>
	concept before_iteration_types = requires(arg_types&... args) { functor_type::before(args...); };

	template<typename value_type>
	concept map_subscriptable_types = requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::key_type>()] } -> std::same_as<const typename base_t<value_type>::mapped_type&>;
	} || requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::key_type>()] } -> std::same_as<typename base_t<value_type>::mapped_type&>;
	};

	template<typename value_type>
	concept vector_subscriptable_types = requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::size_type>()] } -> std::same_as<typename base_t<value_type>::const_reference>;
	} || requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::size_type>()] } -> std::same_as<typename base_t<value_type>::reference>;
	};

	template<typename value_type>
	concept sized_types = requires(base_t<value_type> value) {
		{ value.size() } -> std::same_as<typename base_t<value_type>::size_type>;
	};

	template<typename value_type>
	concept empty_checkable_types = requires(base_t<value_type> value) {
		{ value.empty() } -> std::same_as<bool>;
	};

	template<typename value_type>
	concept variant_types = requires(base_t<value_type> var) {
		{ var.index() } -> std::same_as<uint64_t>;
		{ var.valueless_by_exception() } -> std::same_as<bool>;
		{ std::holds_alternative<decltype(std::get<0>(var))>(var) } -> std::same_as<bool>;
		{ std::get<0>(var) } -> std::same_as<decltype(std::get<0>(var))&>;
		{ std::get_if<0>(&var) } -> std::same_as<base_t<decltype(std::get<0>(var))>*>;
	};

	template<typename value_type>
	concept resizeable_types = requires(base_t<value_type> value) { value.resize(std::declval<typename base_t<value_type>::size_type>()); };

	template<typename value_type>
	concept reserveable_types = requires(base_t<value_type> value) { value.reserve(std::declval<typename base_t<value_type>::size_type>()); };

	template<typename value_type>
	concept data_accessible_types = requires(base_t<value_type> value) {
		{ value.data() } -> std::same_as<typename base_t<value_type>::const_pointer>;
	} || requires(base_t<value_type> value) {
		{ value.data() } -> std::same_as<typename base_t<value_type>::pointer>;
	};

	template<typename value_type>
	concept stateless_types = std::is_empty_v<base_t<value_type>>;

	template<typename value_type>
	concept bool_types = std::same_as<base_t<value_type>, bool> || std::same_as<base_t<value_type>, std::vector<bool>::reference> ||
		std::same_as<base_t<value_type>, std::vector<bool>::const_reference>;

	template<typename value_type>
	concept always_null_types =
		std::same_as<base_t<value_type>, std::nullptr_t> || std::same_as<base_t<value_type>, std::monostate> || std::same_as<base_t<value_type>, std::nullopt_t>;

	template<typename value_type>
	concept pointer_types =
		(std::is_pointer_v<base_t<value_type>> || ( std::is_null_pointer_v<base_t<value_type>> && !std::is_array_v<base_t<value_type>> )) && !always_null_types<value_type>;

	template<typename value_type>
	concept float_types = std::floating_point<base_t<value_type>>;

	template<typename value_type>
	concept substringable_types = requires(base_t<value_type> value) {
		{ value.substr(std::declval<typename base_t<value_type>::size_type>(), std::declval<typename base_t<value_type>::size_type>()) } -> std::same_as<base_t<value_type>>;
	};

	template<typename value_type>
	concept function_pointer_types = std::is_pointer_v<value_type> && std::is_function_v<std::remove_pointer_t<value_type>>;

	template<typename value_type>
	concept findable_types = requires(base_t<value_type> value) {
		{ value.find(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<typename base_t<value_type>::size_type>;
	} || requires(base_t<value_type> value) {
		{ value.find(std::declval<typename base_t<value_type>::key_type>()) } -> std::same_as<typename base_t<value_type>::iterator>;
	} || requires(base_t<value_type> value) {
		{ value.find(std::declval<typename base_t<value_type>::key_type>()) } -> std::same_as<typename base_t<value_type>::const_iterator>;
	};

	template<typename value_type>
	concept string_types =
		substringable_types<value_type> && data_accessible_types<value_type> && sized_types<value_type> && vector_subscriptable_types<value_type> && findable_types<value_type>;

	template<typename value_type>
	concept string_view_types = substringable_types<value_type> && data_accessible_types<value_type> && sized_types<value_type> && vector_subscriptable_types<value_type> &&
		findable_types<value_type> && !resizeable_types<value_type>;

	template<typename value_type>
	concept map_types =
		map_subscriptable_types<value_type> && iterable_types<value_type> && sized_types<value_type> && findable_types<value_type> && empty_checkable_types<value_type>;

	template<typename value_type>
	concept pair_types = requires(base_t<value_type> value) {
		typename base_t<value_type>::first_type;
		typename base_t<value_type>::second_type;
	};

	template<typename value_type>
	concept fillable_types = requires(base_t<value_type> value) {
		{ value.fill(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<void>;
	};

	template<typename value_type>
	concept emplace_backable_types = requires(base_t<value_type> value) {
		{ value.emplace_back(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<typename base_t<value_type>::reference>;
	};

	template<typename value_type>
	concept releasable_types = requires(base_t<value_type> value) {
		{ value.release() } -> std::same_as<typename base_t<value_type>::pointer>;
	};

	template<typename value_type>
	concept resettable_types = requires(base_t<value_type> value) {
		{ value.reset() } -> std::same_as<void>;
	};

	template<typename value_type>
	concept gettable_types = requires(base_t<value_type> value) {
		{ value.get() } -> std::same_as<typename base_t<value_type>::element_type*>;
	};

	template<typename value_type>
	concept copyable_types = std::copyable<base_t<value_type>>;

	template<typename value_type>
	concept unique_ptr_types = requires(base_t<value_type> value) {
		typename base_t<value_type>::element_type;
		typename base_t<value_type>::deleter_type;
	} && releasable_types<value_type> && gettable_types<value_type>;

	template<typename value_type>
	concept shared_ptr_types = resettable_types<value_type> && gettable_types<value_type> && copyable_types<value_type>;

	template<typename value_type>
	concept nullable_types = !string_types<value_type> && requires(base_t<value_type> value) {
		bool(value);
		{ *value };
	};

	template<typename value_type>
	concept null_types = nullable_types<value_type> || always_null_types<value_type>;

	template<typename value_type> inline constexpr bool has_size_equal_to_zero{ std::tuple_size_v<base_t<value_type>> == 0 };

	template<typename value_type>
	concept template_gettable_types = requires(base_t<value_type> value) {
		{ std::get<0>(value) } -> std::same_as<decltype(std::get<0>(value))&>;
	};

	template<typename value_type>
	concept tuple_types = requires(base_t<value_type> t) { std::tuple_size<base_t<value_type>>::value; } &&
		(has_size_equal_to_zero<value_type> || template_gettable_types<value_type>) && !data_accessible_types<value_type>;

	template<typename value_type>
	concept optional_types = requires(base_t<value_type> opt) {
		{ opt.has_value() } -> std::same_as<bool>;
		{ opt.value() } -> std::same_as<typename base_t<value_type>::value_type&>;
		{ *opt } -> std::same_as<typename base_t<value_type>::value_type&>;
		{ opt.reset() } -> std::same_as<void>;
		{ opt.emplace(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<typename base_t<value_type>::value_type&>;
	};

	template<typename value_type>
	concept any_enum_types = std::is_enum_v<base_t<value_type>>;

	template<typename value_type>
	concept vector_types = vector_subscriptable_types<value_type> && !string_types<value_type> && !requires() { value_type::values; };

	template<typename value_type>
	concept raw_array_types = ( std::is_array_v<base_t<value_type>> && !std::is_pointer_v<base_t<value_type>> ) ||
		(vector_subscriptable_types<value_type> && !vector_types<value_type> && !substringable_types<value_type> && !tuple_types<value_type>);

	template<typename value_type>
	concept integer_types = std::integral<base_t<value_type>> && !bool_types<value_type> && !std::floating_point<base_t<value_type>>;

	template<typename value_type>
	concept printable_types = requires(base_t<value_type> value) { std::cout << value << std::endl; };

	template<typename value_type>
	concept not_printable_types = !printable_types<value_type>;

	template<typename value_type, typename... arg_types>
	concept invocable_types = std::is_invocable_v<base_t<value_type>, arg_types...>;

	template<typename value_type, typename... arg_types>
	concept not_invocable_types = !invocable_types<value_type, arg_types...>;

	template<typename value_type, typename... arg_types>
	concept invocable_void_types = invocable_types<value_type, arg_types...> && std::is_void_v<std::invoke_result_t<value_type, arg_types...>>;

	template<typename value_type, typename... arg_types>
	concept invocable_not_void_types = invocable_types<value_type, arg_types...> && !std::is_void_v<std::invoke_result_t<value_type, arg_types...>>;

	template<typename value_type>
	concept small_trivially_copyable_types = std::is_trivially_copyable_v<value_type> && (sizeof(value_type) <= sizeof(value_type*));

	template<typename value_type>
	concept large_or_non_trivially_copyable_types = !std::is_trivially_copyable_v<value_type> || (sizeof(value_type) > sizeof(value_type*));

}
