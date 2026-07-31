/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/benchmarksuite
 * include/benchmarksuite-incl/thread_affinity.hpp
 */

#pragma once

#include <benchmarksuite-incl/config.hpp>

#if BNCH_SWT_COMPILER_CLANG
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
	#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace benchmarksuite {

#if BNCH_SWT_PLATFORM_WINDOWS || (BNCH_SWT_PLATFORM_LINUX && BNCH_SWT_ARCH_X64)

	struct cpuid_regs {
		uint32_t eax, ebx, ecx, edx;
	};

	BNCH_SWT_HOST static cpuid_regs cpuid_call(uint32_t leaf, uint32_t subleaf) noexcept {
		cpuid_regs r{};
	#if BNCH_SWT_PLATFORM_WINDOWS
		int32_t regs[4];
		__cpuidex(regs, static_cast<int32_t>(leaf), static_cast<int32_t>(subleaf));
		r.eax = static_cast<uint32_t>(regs[0]);
		r.ebx = static_cast<uint32_t>(regs[1]);
		r.ecx = static_cast<uint32_t>(regs[2]);
		r.edx = static_cast<uint32_t>(regs[3]);
	#else
		__cpuid_count(leaf, subleaf, r.eax, r.ebx, r.ecx, r.edx);
	#endif
		return r;
	}

	BNCH_SWT_HOST static bool is_intel_hybrid() noexcept {
		auto vendor		 = cpuid_call(0, 0);
		const bool intel = (vendor.ebx == 0x756e6547u) && (vendor.edx == 0x49656e69u) && (vendor.ecx == 0x6c65746eu);
		if (!intel || vendor.eax < 0x7)
			return false;
		auto feat = cpuid_call(0x7, 0);
		if (!((feat.edx >> 15) & 0x1u))
			return false;
		return cpuid_call(0, 0).eax >= 0x1A;
	}

	BNCH_SWT_HOST static bool current_cpu_is_pcore() noexcept {
		return ((cpuid_call(0x1A, 0).eax >> 24) & 0xffu) == 0x40u;
	}

#endif

#if BNCH_SWT_PLATFORM_WINDOWS

	BNCH_SWT_HOST static bool index_to_processor_number(DWORD index, PROCESSOR_NUMBER& out) noexcept {
		DWORD seen		  = 0;
		const WORD groups = GetActiveProcessorGroupCount();
		for (WORD g = 0; g < groups; ++g) {
			const DWORD in_group = GetActiveProcessorCount(g);
			if (index < seen + in_group) {
				out.Group	 = g;
				out.Number	 = static_cast<BYTE>(index - seen);
				out.Reserved = 0;
				return true;
			}
			seen += in_group;
		}
		return false;
	}

	BNCH_SWT_HOST static int32_t find_first_pcore() noexcept {
		if (!is_intel_hybrid())
			return -1;

		const DWORD total = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		HANDLE self		  = GetCurrentThread();

		GROUP_AFFINITY original{};
		GetThreadGroupAffinity(self, &original);

		int32_t found = -1;
		for (DWORD i = 0; i < total; ++i) {
			PROCESSOR_NUMBER pn{};
			if (!index_to_processor_number(i, pn))
				continue;

			GROUP_AFFINITY ga{};
			ga.Group = pn.Group;
			ga.Mask	 = static_cast<KAFFINITY>(1ull) << pn.Number;

			if (!SetThreadGroupAffinity(self, &ga, nullptr))
				continue;
			SwitchToThread();

			if (current_cpu_is_pcore()) {
				found = static_cast<int32_t>(i);
				break;
			}
		}

		SetThreadGroupAffinity(self, &original, nullptr);
		return found;
	}

	BNCH_SWT_HOST static bool pin_for_benchmark() noexcept {
		HANDLE self = GetCurrentThread();

		bool aff_ok		= false;
		const int32_t pcore = find_first_pcore();
		if (pcore >= 0) {
			PROCESSOR_NUMBER pn{};
			if (index_to_processor_number(static_cast<DWORD>(pcore), pn)) {
				GROUP_AFFINITY ga{};
				ga.Group = pn.Group;
				ga.Mask	 = static_cast<KAFFINITY>(1ull) << pn.Number;
				aff_ok	 = SetThreadGroupAffinity(self, &ga, nullptr) != 0;
			}
		} else {
			PROCESSOR_NUMBER pn{};
			GetCurrentProcessorNumberEx(&pn);
			GROUP_AFFINITY ga{};
			ga.Group = pn.Group;
			ga.Mask	 = static_cast<KAFFINITY>(1ull) << pn.Number;
			aff_ok	 = SetThreadGroupAffinity(self, &ga, nullptr) != 0;
		}

		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		const bool prio_ok = SetThreadPriority(self, THREAD_PRIORITY_TIME_CRITICAL) != 0;
		SetThreadPriorityBoost(self, TRUE);

		if (!aff_ok)
			std::fprintf(stderr, "[bench] affinity pin failed\n");
		if (!prio_ok)
			std::fprintf(stderr, "[bench] priority raise failed\n");
		return aff_ok && prio_ok;
	}

#elif BNCH_SWT_PLATFORM_LINUX || BNCH_SWT_PLATFORM_ANDROID

	#if defined(__aarch64__) || defined(__arm__)

	BNCH_SWT_HOST static long read_cpu_max_freq_khz(int32_t cpu_index) noexcept {
		char path[128];
		std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu_index);
		FILE* f = std::fopen(path, "r");
		if (!f)
			return -1;
		long khz = -1;
		if (std::fscanf(f, "%ld", &khz) != 1)
			khz = -1;
		std::fclose(f);
		return khz;
	}

	BNCH_SWT_HOST static int32_t find_first_pcore() noexcept {
		const long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
		if (ncpu <= 0)
			return -1;

		int32_t best_cpu   = -1;
		long best_freq = -1;
		for (long i = 0; i < ncpu; ++i) {
			const long freq = read_cpu_max_freq_khz(static_cast<int32_t>(i));
			if (freq > best_freq) {
				best_freq = freq;
				best_cpu  = static_cast<int32_t>(i);
			}
		}

		if (best_cpu < 0 || best_freq <= 0)
			return -1;

		return best_cpu;
	}

	#elif defined(__x86_64__) || defined(__i386__)

	BNCH_SWT_HOST static int32_t find_first_pcore() noexcept {
		if (!is_intel_hybrid())
			return -1;

		const long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
		if (ncpu <= 0)
			return -1;

		pthread_t self = pthread_self();
		cpu_set_t original;
		CPU_ZERO(&original);
		pthread_getaffinity_np(self, sizeof(original), &original);

		int32_t found = -1;
		for (long i = 0; i < ncpu; ++i) {
			cpu_set_t one;
			CPU_ZERO(&one);
			CPU_SET(static_cast<int32_t>(i), &one);
			if (pthread_setaffinity_np(self, sizeof(one), &one) != 0)
				continue;
			sched_yield();
			if (current_cpu_is_pcore()) {
				found = static_cast<int32_t>(i);
				break;
			}
		}

		pthread_setaffinity_np(self, sizeof(original), &original);
		return found;
	}

	#else

	BNCH_SWT_HOST static int32_t find_first_pcore() noexcept {
		return -1;
	}

	#endif

	BNCH_SWT_HOST static bool pin_for_benchmark() noexcept {
		pthread_t self = pthread_self();

		cpu_set_t target;
		CPU_ZERO(&target);
		const int32_t pcore = find_first_pcore();
		if (pcore >= 0) {
			CPU_SET(pcore, &target);
		} else {
			CPU_SET(sched_getcpu(), &target);
		}

	#if BNCH_SWT_PLATFORM_ANDROID
		const bool aff_ok = sched_setaffinity(0, sizeof(target), &target) == 0;
	#else
		const bool aff_ok = pthread_setaffinity_np(self, sizeof(target), &target) == 0;
	#endif

		bool prio_ok = false;
		sched_param sp{};
		sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
		if (pthread_setschedparam(self, SCHED_FIFO, &sp) == 0) {
			prio_ok = true;
		} else {
			errno		 = 0;
			const int32_t rc = setpriority(PRIO_PROCESS, 0, -20);
			prio_ok		 = (rc == 0 && errno == 0);
		}

		if (!aff_ok)
			std::fprintf(stderr, "[bench] affinity pin failed\n");
		if (!prio_ok)
			std::fprintf(stderr, "[bench] priority raise failed (need CAP_SYS_NICE or root for SCHED_FIFO)\n");
		return aff_ok && prio_ok;
	}

#elif BNCH_SWT_PLATFORM_MAC

	BNCH_SWT_HOST static bool pin_for_benchmark() noexcept {
		const int32_t qos_rc = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

		thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());

		thread_affinity_policy_data_t aff{ 1 };
		thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, reinterpret_cast<thread_policy_t>(&aff), THREAD_AFFINITY_POLICY_COUNT);

		thread_precedence_policy_data_t prec{ 63 };
		thread_policy_set(mach_thread, THREAD_PRECEDENCE_POLICY, reinterpret_cast<thread_policy_t>(&prec), THREAD_PRECEDENCE_POLICY_COUNT);

		if (qos_rc != 0) {
			std::fprintf(stderr, "[bench] QoS set failed (rc=%d) - scheduler will not bias toward P-cores\n", qos_rc);
			return false;
		}
		return true;
	}

#else
	BNCH_SWT_HOST static bool pin_for_benchmark() noexcept {
		std::fprintf(stderr, "[bench] unsupported platform - no pin performed\n");
		return false;
	}
#endif

}
#if BNCH_SWT_COMPILER_CLANG
	#pragma clang diagnostic pop
#endif
