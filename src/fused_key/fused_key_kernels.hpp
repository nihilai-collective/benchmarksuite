#pragma once

#include <cstdint>
#include <string_view>

namespace fused_key {

	struct key_corpus {
		const char* data{};
		const uint32_t* offsets{};
		uint64_t count{};
	};

	inline constexpr std::string_view key9{ "\"userId\":" };
	inline constexpr std::string_view key11{ "\"username\":" };
	inline constexpr std::string_view key14{ "\"description\":" };

	uint64_t scanU64Key9(const key_corpus& corpus) noexcept;
	uint64_t scanU64Key11(const key_corpus& corpus) noexcept;
	uint64_t scanU64Key14(const key_corpus& corpus) noexcept;
	uint64_t scanXmmKey9(const key_corpus& corpus) noexcept;
	uint64_t scanXmmKey11(const key_corpus& corpus) noexcept;
	uint64_t scanXmmKey14(const key_corpus& corpus) noexcept;

}
