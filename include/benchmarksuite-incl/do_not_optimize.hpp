/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/do_not_optimize.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

namespace benchmarksuite {

	namespace internal {

		[[maybe_unused]] inline void const volatile* volatile global_force_escape_pointer;

		[[maybe_unused]] BNCH_SWT_HOST void use_char_pointer(void const volatile* const v) noexcept {
			global_force_escape_pointer = v;
		}

#if BNCH_SWT_COMPILER_MSVC

		template<typename value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type const& value) noexcept {
			use_char_pointer(static_cast<void const volatile* const>(&value));
			_ReadWriteBarrier();
		}

		template<typename value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type&& value) noexcept {
			use_char_pointer(static_cast<void const volatile* const>(&value));
			_ReadWriteBarrier();
		}

#elif BNCH_SWT_COMPILER_CLANG
		template<typename value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type const& value) noexcept {
			asm volatile("" : : "r,m"(value) : "memory");
		}

		template<typename value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type&& value) noexcept {
			asm volatile("" : "+r,m"(value) : : "memory");
		}

#elif BNCH_SWT_COMPILER_GCC
		template<small_trivially_copyable value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type const& value) noexcept {
			asm volatile("" : : "r,m"(value) : "memory");
		}

		template<large_or_non_trivially_copyable value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type const& value) noexcept {
			asm volatile("" : : "m"(value) : "memory");
		}

		template<small_trivially_copyable value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type&& value) noexcept {
			asm volatile("" : "+m,r"(value) : : "memory");
		}

		template<large_or_non_trivially_copyable value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type&& value) noexcept {
			asm volatile("" : "+m"(value) : : "memory");
		}
#else

		template<class value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_impl(value_type&& value) noexcept {
			internal::use_char_pointer(&std::bit_cast<char const volatile&>(value));
		}

#endif

		[[maybe_unused]] BNCH_SWT_HOST void clobber_memory() noexcept {
#if BNCH_SWT_COMPILER_MSVC
			_ReadWriteBarrier();
#elif BNCH_SWT_COMPILER_CLANG || BNCH_SWT_COMPILER_GCC
			asm volatile("" ::: "memory");
#endif
		}
	}

	template<internal::not_invocable value_type> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_away(value_type&& value) noexcept {
		internal::do_not_optimize_impl(value);
	}

	template<internal::invocable_void function_type, typename... arg_types> [[maybe_unused]] BNCH_SWT_HOST void do_not_optimize_away(function_type&& value, arg_types&&... args) {
		std::forward<function_type>(value)(std::forward<arg_types>(args)...);
		internal::clobber_memory();
	}

	template<internal::invocable_not_void function_type, typename... arg_types>
	[[maybe_unused]] BNCH_SWT_HOST auto do_not_optimize_away(function_type&& value, arg_types&&... args) {
		auto result_val = std::forward<function_type>(value)(std::forward<arg_types>(args)...);
		internal::do_not_optimize_impl(result_val);
		return result_val;
	}

}
