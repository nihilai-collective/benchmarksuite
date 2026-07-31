// MIT License @ /License.md
// Copyright (c) 2026 Nihilai Collective Corp
// https://github.com/nihilai-collective/benchmarksuite
// include/benchmarksuite-incl/file_handle.hpp

#pragma once

#include <benchmarksuite-incl/config.hpp>

namespace benchmarksuite {

	class file_handle {
	  public:
		BNCH_SWT_HOST static void save_file(const std::string& data, const std::string& path) {
			std::filesystem::path abs_path = std::filesystem::absolute(path);
			std::filesystem::create_directories(abs_path.parent_path());
			std::fstream stream{ abs_path, std::ios::out | std::ios::trunc };
			if (stream.is_open()) {
				stream << data;
				stream.flush();
				bool ok = stream.good();
				stream.close();
				std::cout << (ok ? "Saved: " : "Write error: ") << abs_path.string() << std::endl;
			} else {
				std::cout << "Failed to open for writing: " << abs_path.string() << std::endl;
			}
		}

		BNCH_SWT_HOST static std::string get(const std::string& path) {
			std::fstream stream{ std::filesystem::absolute(path), std::ios::in };
			if (stream.is_open()) {
				return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			} else {
				throw std::runtime_error{ "Sorry, but we failed to load the file at: " + path };
			}
			return {};
		}
	};

}
