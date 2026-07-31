// MIT License @ /License.md
// Copyright (c) 2026 Nihilai Collective Corp
// https://github.com/nihilai-collective/benchmarksuite
// include/benchmarksuite-incl/aligned_const.hpp

#pragma once

#include <benchmarksuite-incl/config.hpp>
#include <benchmarksuite-incl/benchmarksuite_cpu_properties.hpp>

namespace benchmarksuite {

	static constexpr uint64_t device_alignment{ [] {
		if constexpr (BNCH_SWT_COMPILER_CUDA) {
			return 16ULL;
		} else {
			return cpu_properties::cpu_alignment;
		}
	}() };

	template<typename value_type>
	concept derivable_from = std::is_class_v<base_t<value_type>> && !std::is_final_v<base_t<value_type>>;

	template<typename value_type_new, uint64_t device_alignment = 16> struct BNCH_SWT_ALIGN(device_alignment) aligned_const {
		using value_type = value_type_new;
		BNCH_SWT_ALIGN(16) value_type value {};

		BNCH_SWT_HOST_DEVICE constexpr operator const value_type&() const& {
			return value;
		}

		BNCH_SWT_HOST_DEVICE explicit constexpr operator value_type&() & {
			return value;
		}

		BNCH_SWT_HOST_DEVICE explicit constexpr operator value_type&&() && {
			return std::move(value);
		}

		BNCH_SWT_HOST_DEVICE constexpr const value_type& operator*() const {
			return value;
		}

		template<typename value_type_newer> BNCH_SWT_HOST_DEVICE constexpr void emplace(value_type_newer&& value_new) {
			value = std::forward<value_type_newer>(value_new);
		}

		BNCH_SWT_HOST_DEVICE value_type& operator*() {
			return value;
		}

		BNCH_SWT_HOST_DEVICE constexpr value_type operator*(const aligned_const& other) const {
			return value * other.value;
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator==(const aligned_const& other) const {
			return value == other.value;
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator!=(const aligned_const& other) const {
			return value != other.value;
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator<(const aligned_const& other) const {
			return value < other.value;
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator>(const aligned_const& other) const {
			return value > other.value;
		}
	};

	template<derivable_from value_type_new, uint64_t device_alignment> struct BNCH_SWT_ALIGN(device_alignment) aligned_const<value_type_new, device_alignment> : public value_type_new {
		using value_type = value_type_new;

		BNCH_SWT_HOST_DEVICE constexpr const value_type& operator*() const {
			return *static_cast<const value_type*>(this);
		}

		template<typename value_type_newer> BNCH_SWT_HOST_DEVICE constexpr void emplace(value_type_newer&& value_new) {
			*static_cast<value_type*>(this) = std::forward<value_type_newer>(value_new);
		}

		BNCH_SWT_HOST_DEVICE value_type& operator*() {
			return *static_cast<value_type*>(this);
		}

		BNCH_SWT_HOST_DEVICE constexpr value_type operator*(const aligned_const& other) const {
			return *static_cast<const value_type*>(this) * *static_cast<const value_type*>(&other);
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator==(const aligned_const& other) const {
			return *static_cast<const value_type*>(this) == *static_cast<const value_type*>(&other);
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator!=(const aligned_const& other) const {
			return *static_cast<const value_type*>(this) != *static_cast<const value_type*>(&other);
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator<(const aligned_const& other) const {
			return *static_cast<const value_type*>(this) < *static_cast<const value_type*>(&other);
		}

		BNCH_SWT_HOST_DEVICE constexpr bool operator>(const aligned_const& other) const {
			return *static_cast<const value_type*>(this) > *static_cast<const value_type*>(&other);
		}
	};

	template<typename value_type> aligned_const(value_type) -> aligned_const<value_type>;

}
