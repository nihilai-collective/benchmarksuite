#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace fused_key {

	struct key_corpus {
		const char* data{};
		const uint32_t* offsets{};
		uint64_t count{};
	};

	struct alignas(16) expected_block {
		uint8_t bytes[16]{};
	};

	struct mixed_key_corpus {
		const char* data{};
		const uint32_t* offsets{};
		const uint8_t* lengths{};
		const uint64_t* expectedLo{};
		const uint64_t* expectedHi{};
		const expected_block* expectedBlocks{};
		uint64_t count{};
	};

	inline constexpr std::string_view key9{ "\"userId\":" };
	inline constexpr std::string_view key10{ "\"created\":" };
	inline constexpr std::string_view key11{ "\"username\":" };
	inline constexpr std::string_view key12{ "\"timestamp\":" };
	inline constexpr std::string_view key13{ "\"channel_id\":" };
	inline constexpr std::string_view key14{ "\"description\":" };
	inline constexpr std::string_view key15{ "\"last_message\":" };

	inline constexpr std::array<std::string_view, 7> mixedKeys{ key9, key10, key11, key12, key13, key14, key15 };

	uint64_t scanU64Key9(const key_corpus& corpus) noexcept;
	uint64_t scanU64Key11(const key_corpus& corpus) noexcept;
	uint64_t scanU64Key14(const key_corpus& corpus) noexcept;
	uint64_t scanXmmKey9(const key_corpus& corpus) noexcept;
	uint64_t scanXmmKey11(const key_corpus& corpus) noexcept;
	uint64_t scanXmmKey14(const key_corpus& corpus) noexcept;
	uint64_t scanXmmJsonifierMemcpyKey9(const key_corpus& corpus) noexcept;
	uint64_t scanXmmJsonifierMemcpyKey11(const key_corpus& corpus) noexcept;
	uint64_t scanXmmJsonifierMemcpyKey14(const key_corpus& corpus) noexcept;
	uint64_t scanXmmPow2Key9(const key_corpus& corpus) noexcept;
	uint64_t scanXmmPow2Key11(const key_corpus& corpus) noexcept;
	uint64_t scanXmmPow2Key14(const key_corpus& corpus) noexcept;
	uint64_t scanU64Mixed(const mixed_key_corpus& corpus) noexcept;
	uint64_t scanXmmMixed(const mixed_key_corpus& corpus) noexcept;
	uint64_t scanXmmJsonifierMemcpyMixed(const mixed_key_corpus& corpus) noexcept;

}
