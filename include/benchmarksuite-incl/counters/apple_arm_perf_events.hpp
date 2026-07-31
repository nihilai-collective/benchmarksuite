// Original design from:
// =============================================================================
// XNU kperf/kpc
// Available for 64-bit Intel/Apple Silicon, macOS/iOS, with root privileges
//
// References:
//
// XNU source (since xnu 2422.1.72):
// https://github.com/apple/darwin-xnu/blob/main/osfmk/kern/kpc.h
// https://github.com/apple/darwin-xnu/blob/main/bsd/kern/kern_kpc.c
//
// Lightweight PET (Profile Every Thread, since xnu 3789.1.32):
// https://github.com/apple/darwin-xnu/blob/main/osfmk/kperf/pet.c
// https://github.com/apple/darwin-xnu/blob/main/osfmk/kperf/kperf_kpc.c
//
// System Private frameworks (since macOS 10.11, iOS 8.0):
// /System/Library/PrivateFrameworks/kperf.framework
// /System/Library/PrivateFrameworks/kperfdata.framework
//
// Xcode framework (since Xcode 7.0):
// /Applications/Xcode.app/Contents/SharedFrameworks/DVTInstrumentsFoundation.framework
//
// CPU database (plist files)
// macOS (since macOS 10.11):
//     /usr/share/kpep/<name>.plist
// iOS (copied from Xcode, since iOS 10.0, Xcode 8.0):
//     /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform
//     /DeviceSupport/<version>/DeveloperDiskImage.dmg/usr/share/kpep/<name>.plist
//
//
// Created by YaoYuan <ibireme@gmail.com> on 2021.
// Released into the public domain (https://unlicense.org).
// =============================================================================

#pragma once

#include <benchmarksuite-incl/config.hpp>

#if BNCH_SWT_PLATFORM_MAC

namespace benchmarksuite::internal {

	struct performance_counters {
		double cache_misses{};
		double branch_misses{};
		double instructions{};
		double branches{};
		double cycles{};

		BNCH_SWT_HOST performance_counters(double c, double b, double m, double i, double cm)
			: cache_misses{ cm }, branch_misses{ m }, instructions{ i }, branches{ b }, cycles{ c } {
		}

		BNCH_SWT_HOST performance_counters() : cache_misses{}, branch_misses{}, instructions{}, branches{}, cycles{} {
		}
	};

	static inline performance_counters operator-(const performance_counters& a, const performance_counters& b) {
		return performance_counters(a.cycles - b.cycles, a.branches - b.branches, a.branch_misses - b.branch_misses, a.instructions - b.instructions,
			a.cache_misses - b.cache_misses);
	}

	class performance_monitor {
	  protected:
		static constexpr uint32_t kpc_class_configurable_mask{ 1U << 1U };
		static constexpr uint32_t kpc_class_rawpmu_mask{ 1U << 3U };
		static constexpr uint32_t kpc_class_fixed_mask{ 1U << 0U };
		static constexpr uint32_t kpc_class_power_mask{ 1U << 2U };
		static constexpr uint64_t kpc_max_counters{ 32 };

		struct kpep_event {
			const char* description;
			const char* fallback;
			const char* errata;
			const char* alias;
			const char* name;
			uint8_t reserved;
			uint8_t is_fixed;
			uint8_t number;
			uint32_t mask;
			uint8_t umask;
		};

		struct kpep_db {
			uint32_t config_counter_bits;
			kpep_event** fixed_event_arr;
			size_t config_counter_count;
			uint32_t fixed_counter_bits;
			uint32_t power_counter_bits;
			size_t fixed_counter_count;
			size_t power_counter_count;
			const char* marketing_name;
			kpep_event* event_arr;
			uint32_t archtecture;
			const char* cpu_id;
			size_t event_count;
			size_t alias_count;
			size_t reserved_1;
			size_t reserved_2;
			size_t reserved_3;
			const char* name;
			void* plist_data;
			void* event_map;
			void* alias_map;
		};

		struct kpep_config {
			uint32_t config_counter;
			uint32_t power_counter;
			uint64_t* kpc_periods;
			size_t counter_count;
			kpep_event** ev_arr;
			size_t event_count;
			uint32_t reserved;
			uint32_t classes;
			uint32_t* flags;
			size_t* ev_map;
			size_t* ev_idx;
			kpep_db* db;
		};

		enum class kpep_config_error_code {
			kpep_config_error_none					 = 0,
			kpep_config_error_invalid_argument		 = 1,
			kpep_config_error_out_of_memory			 = 2,
			kpep_config_error_io					 = 3,
			kpep_config_error_buffer_too_small		 = 4,
			kpep_config_error_cur_system_unknown	 = 5,
			kpep_config_error_db_path_invalid		 = 6,
			kpep_config_error_db_not_found			 = 7,
			kpep_config_error_db_arch_unsupported	 = 8,
			kpep_config_error_db_version_unsupported = 9,
			kpep_config_error_db_corrupt			 = 10,
			kpep_config_error_event_not_found		 = 11,
			kpep_config_error_conflicting_events	 = 12,
			kpep_config_error_counters_not_forced	 = 13,
			kpep_config_error_event_unavailable		 = 14,
			kpep_config_error_errno					 = 15,
			kpep_config_error_max
		};

		struct lib_symbol {
			const char* name;
			void** impl;
		};

		struct event_alias {
			const char* alias;
			std::array<const char*, 8> names;
		};

		struct loaded_symbols {
			int32_t (*kpc_cpu_string)(char* buf, size_t buf_size){};
			uint32_t (*kpc_pmu_version)(){};
			uint32_t (*kpc_get_counting)(){};
			int32_t (*kpc_set_counting)(uint32_t classes){};
			uint32_t (*kpc_get_thread_counting)(){};
			int32_t (*kpc_set_thread_counting)(uint32_t classes){};
			uint32_t (*kpc_get_config_count)(uint32_t classes){};
			int32_t (*kpc_get_config)(uint32_t classes, uint64_t* config){};
			int32_t (*kpc_set_config)(uint32_t classes, uint64_t* config){};
			uint32_t (*kpc_get_counter_count)(uint32_t classes){};
			int32_t (*kpc_get_cpu_counters)(bool all_cpus, uint32_t classes, int32_t* curcpu, uint64_t* buf){};
			int32_t (*kpc_get_thread_counters)(uint32_t tid, uint32_t buf_count, uint64_t* buf){};
			int32_t (*kpc_force_all_ctrs_set)(int32_t val){};
			int32_t (*kpc_force_all_ctrs_get)(int32_t* val_out){};
			int32_t (*kpep_config_create)(kpep_db* db, kpep_config** cfg_ptr){};
			void (*kpep_config_free)(kpep_config* cfg){};
			int32_t (*kpep_config_add_event)(kpep_config* cfg, kpep_event** ev_ptr, uint32_t flag, uint32_t* err){};
			int32_t (*kpep_config_force_counters)(kpep_config* cfg){};
			int32_t (*kpep_config_kpc)(kpep_config* cfg, uint64_t* buf, size_t buf_size){};
			int32_t (*kpep_config_kpc_count)(kpep_config* cfg, size_t* count_ptr){};
			int32_t (*kpep_config_kpc_classes)(kpep_config* cfg, uint32_t* classes_ptr){};
			int32_t (*kpep_config_kpc_map)(kpep_config* cfg, size_t* buf, size_t buf_size){};
			int32_t (*kpep_db_create)(const char* name, kpep_db** db_ptr){};
			void (*kpep_db_free)(kpep_db* db){};
			int32_t (*kpep_db_event)(kpep_db* db, const char* name, kpep_event** ev_ptr){};
			void* lib_handle_kperf{ nullptr };
			void* lib_handle_kperfdata{ nullptr };
			bool valid{ false };

			loaded_symbols()								 = default;
			loaded_symbols(const loaded_symbols&)			 = delete;
			loaded_symbols& operator=(const loaded_symbols&) = delete;

			~loaded_symbols() {
				if (lib_handle_kperf) {
					dlclose(lib_handle_kperf);
				}
				if (lib_handle_kperfdata) {
					dlclose(lib_handle_kperfdata);
				}
			}
		};

		BNCH_SWT_HOST static bool load_symbols(void* handle, const std::span<const lib_symbol>& symbols, std::string_view lib_name) {
			for (const auto& symbol: symbols) {
				*symbol.impl = dlsym(handle, symbol.name);
				if (!*symbol.impl) {
					std::cerr << "Failed to load " << lib_name << " function: " << symbol.name << "." << std::endl;
					return false;
				}
			}
			return true;
		}

		BNCH_SWT_HOST static const loaded_symbols& get_symbols() {
			static const loaded_symbols& symbols = *[] {
				auto* result = new loaded_symbols{};

				const char lib_path_kperf[]		= "/System/Library/PrivateFrameworks/kperf.framework/kperf";
				const char lib_path_kperfdata[] = "/System/Library/PrivateFrameworks/kperfdata.framework/kperfdata";

				result->lib_handle_kperf = dlopen(lib_path_kperf, RTLD_LAZY);
				if (!result->lib_handle_kperf) {
					std::cerr << "Failed to open kperf." << std::endl;
					return result;
				}

				result->lib_handle_kperfdata = dlopen(lib_path_kperfdata, RTLD_LAZY);
				if (!result->lib_handle_kperfdata) {
					std::cerr << "Failed to open kperfdata." << std::endl;
					return result;
				}

				const std::array<lib_symbol, 14> symbols_kperf{ lib_symbol("kpc_pmu_version", std::bit_cast<void**>(&result->kpc_pmu_version)),
					lib_symbol("kpc_cpu_string", std::bit_cast<void**>(&result->kpc_cpu_string)), lib_symbol("kpc_set_counting", std::bit_cast<void**>(&result->kpc_set_counting)),
					lib_symbol("kpc_get_counting", std::bit_cast<void**>(&result->kpc_get_counting)),
					lib_symbol("kpc_set_thread_counting", std::bit_cast<void**>(&result->kpc_set_thread_counting)),
					lib_symbol("kpc_get_thread_counting", std::bit_cast<void**>(&result->kpc_get_thread_counting)),
					lib_symbol("kpc_get_config_count", std::bit_cast<void**>(&result->kpc_get_config_count)),
					lib_symbol("kpc_get_counter_count", std::bit_cast<void**>(&result->kpc_get_counter_count)),
					lib_symbol("kpc_set_config", std::bit_cast<void**>(&result->kpc_set_config)), lib_symbol("kpc_get_config", std::bit_cast<void**>(&result->kpc_get_config)),
					lib_symbol("kpc_get_cpu_counters", std::bit_cast<void**>(&result->kpc_get_cpu_counters)),
					lib_symbol("kpc_get_thread_counters", std::bit_cast<void**>(&result->kpc_get_thread_counters)),
					lib_symbol("kpc_force_all_ctrs_set", std::bit_cast<void**>(&result->kpc_force_all_ctrs_set)),
					lib_symbol("kpc_force_all_ctrs_get", std::bit_cast<void**>(&result->kpc_force_all_ctrs_get)) };

				const std::array<lib_symbol, 11> symbols_kperfdata{ lib_symbol("kpep_config_create", std::bit_cast<void**>(&result->kpep_config_create)),
					lib_symbol("kpep_config_free", std::bit_cast<void**>(&result->kpep_config_free)),
					lib_symbol("kpep_config_add_event", std::bit_cast<void**>(&result->kpep_config_add_event)),
					lib_symbol("kpep_config_force_counters", std::bit_cast<void**>(&result->kpep_config_force_counters)),
					lib_symbol("kpep_config_kpc", std::bit_cast<void**>(&result->kpep_config_kpc)),
					lib_symbol("kpep_config_kpc_count", std::bit_cast<void**>(&result->kpep_config_kpc_count)),
					lib_symbol("kpep_config_kpc_classes", std::bit_cast<void**>(&result->kpep_config_kpc_classes)),
					lib_symbol("kpep_config_kpc_map", std::bit_cast<void**>(&result->kpep_config_kpc_map)),
					lib_symbol("kpep_db_create", std::bit_cast<void**>(&result->kpep_db_create)), lib_symbol("kpep_db_free", std::bit_cast<void**>(&result->kpep_db_free)),
					lib_symbol("kpep_db_event", std::bit_cast<void**>(&result->kpep_db_event)) };

				if (!load_symbols(result->lib_handle_kperf, symbols_kperf, "kperf")) {
					return result;
				}
				if (!load_symbols(result->lib_handle_kperfdata, symbols_kperfdata, "kperfdata")) {
					return result;
				}

				result->valid = true;
				return result;
			}();
			return symbols;
		}

		static constexpr std::array<event_alias, 5> profile_events{ {
			{ "cycles", { "FIXED_CYCLES", "CPU_CLK_UNHALTED.THREAD", "CPU_CLK_UNHALTED.CORE" } },
			{ "instructions", { "FIXED_INSTRUCTIONS", "INST_RETIRED.ANY" } },
			{ "branches", { "INST_BRANCH", "BR_INST_RETIRED.ALL_BRANCHES", "INST_RETIRED.ANY" } },
			{ "branch-misses", { "BRANCH_MISPRED_NONSPEC", "BRANCH_MISPREDICT", "BR_MISP_RETIRED.ALL_BRANCHES", "BR_INST_RETIRED.MISPRED" } },
			{ "cache-misses",
				{ "L1D_CACHE_MISS_LD", "L1D_CACHE_MISS_LD_NONSPEC", "L2_CACHE_MISS_DATA", "L2_CACHE_MISS", "LLC_MISSES", "LONGEST_LAT_CACHE.MISS", "MEM_LOAD_RETIRED.L3_MISS" } },
		} };

		BNCH_SWT_HOST static kpep_event* get_event(kpep_db* db, const event_alias* alias) {
			const auto& symbols = get_symbols();
			for (const char* name: alias->names) {
				if (!name) {
					break;
				}
				kpep_event* ev = nullptr;
				if (symbols.kpep_db_event(db, name, &ev) == 0) {
					return ev;
				}
			}
			return nullptr;
		}

	  public:
		struct counter_setup {
			std::array<uint64_t, kpc_max_counters> regs{ 0 };
			std::array<size_t, kpc_max_counters> counter_map{ 0 };
			size_t counter_map_count{ 0 };
			bool has_events_val{ false };
		};

		BNCH_SWT_HOST static const counter_setup& get_setup() {
			static const counter_setup& result = *[] {
				counter_setup setup{};
				const auto& symbols = get_symbols();
				if (!symbols.valid) {
					return new counter_setup(setup);
				}

				int32_t force_ctrs = 0;
				if (symbols.kpc_force_all_ctrs_get(&force_ctrs)) {
					return new counter_setup(setup);
				}

				struct kpep_cleanup {
					const loaded_symbols* symbols{ nullptr };
					kpep_db* db{ nullptr };
					kpep_config* cfg{ nullptr };
					~kpep_cleanup() {
						if (cfg) {
							symbols->kpep_config_free(cfg);
						}
						if (db) {
							symbols->kpep_db_free(db);
						}
					}
				} cleanup{ &symbols };

				kpep_db* db = nullptr;
				if (symbols.kpep_db_create(nullptr, &db)) {
					return new counter_setup(setup);
				}
				cleanup.db		 = db;
				kpep_config* cfg = nullptr;
				if (symbols.kpep_config_create(db, &cfg)) {
					return new counter_setup(setup);
				}
				cleanup.cfg = cfg;
				if (symbols.kpep_config_force_counters(cfg)) {
					return new counter_setup(setup);
				}
				for (const auto& alias: profile_events) {
					kpep_event* ev = get_event(db, &alias);
					if (!ev || symbols.kpep_config_add_event(cfg, &ev, 0, nullptr))
						return new counter_setup(setup);
				}

				uint32_t classes = 0;
				size_t reg_count = 0;
				if (symbols.kpep_config_kpc_classes(cfg, &classes)) {
					return new counter_setup(setup);
				}
				if (symbols.kpep_config_kpc_count(cfg, &reg_count)) {
					return new counter_setup(setup);
				}
				if (symbols.kpep_config_kpc_map(cfg, setup.counter_map.data(), sizeof(setup.counter_map))) {
					return new counter_setup(setup);
				}
				setup.counter_map_count = std::min(profile_events.size(), setup.counter_map.size());
				if (symbols.kpep_config_kpc(cfg, setup.regs.data(), sizeof(setup.regs))) {
					return new counter_setup(setup);
				}
				if (symbols.kpc_force_all_ctrs_set(1)) {
					return new counter_setup(setup);
				}
				if ((classes & kpc_class_configurable_mask) && reg_count) {
					if (symbols.kpc_set_config(classes, setup.regs.data()))
						return new counter_setup(setup);
				}
				if (symbols.kpc_set_counting(classes)) {
					return new counter_setup(setup);
				}
				if (symbols.kpc_set_thread_counting(classes)) {
					return new counter_setup(setup);
				}

				setup.has_events_val = true;
				return new counter_setup(setup);
			}();
			return result;
		}

	  public:
		BNCH_SWT_HOST static bool setup_performance_counters() {
			return get_setup().has_events_val;
		}

		BNCH_SWT_HOST static performance_counters get_counters() {
			static bool warned	= false;
			const auto& symbols = get_symbols();
			const auto& setup	= get_setup();
			if (!symbols.valid || !setup.has_events_val) {
				return {};
			}
			std::array<uint64_t, kpc_max_counters> counters{ 0 };
			if (symbols.kpc_get_thread_counters(0, kpc_max_counters, counters.data())) {
				if (!warned) {
					std::cerr << "Failed get thread counters." << std::endl;
					warned = true;
				}
				return {};
			}
			const auto value_at = [&](size_t index) -> double {
				if (index >= setup.counter_map_count || setup.counter_map[index] >= kpc_max_counters) {
					return 0.0;
				}
				return static_cast<double>(counters[setup.counter_map[index]]);
			};
			return performance_counters{ value_at(0), value_at(2), value_at(3), value_at(1), value_at(4) };
		}

		BNCH_SWT_HOST static bool has_events() {
			return get_setup().has_events_val;
		}
	};

	template<benchmark_types benchmark_types, typename function_type> struct iteration_metric_collector {
		template<typename metric_type, typename... arg_types> BNCH_SWT_NOINLINE static void impl(metric_type& iteration_data, arg_types&&... args) {
			performance_monitor::setup_performance_counters();
			const bool has_perf_events = performance_monitor::has_events();
			performance_counters diff{};
			if (has_perf_events) {
				diff = performance_monitor::get_counters();
			}
			const auto start_clock		   = clock_type::now();
			iteration_data.bytes_processed = static_cast<uint64_t>(function_type::impl(std::forward<arg_types>(args)...));
			const auto end_clock		   = clock_type::now();
			if (has_perf_events) {
				performance_counters end = performance_monitor::get_counters();
				diff					 = end - diff;
				iteration_data.cycles.emplace(static_cast<uint64_t>(diff.cycles));
				iteration_data.instructions_val.emplace(static_cast<uint64_t>(diff.instructions));
				iteration_data.branches_val.emplace(static_cast<uint64_t>(diff.branches));
				iteration_data.branch_misses_val.emplace(static_cast<uint64_t>(diff.branch_misses));
				iteration_data.cache_misses_val.emplace(static_cast<uint64_t>(diff.cache_misses));
			}
			iteration_data.time_in_ns = (end_clock - start_clock).count();
		}
	};
}

#endif
