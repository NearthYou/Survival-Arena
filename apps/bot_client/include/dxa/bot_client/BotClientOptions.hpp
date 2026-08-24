#pragma once

#include <dxa/protocol/Ids.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dxa::bot_client
{
struct BotClientOptions
{
    std::string host = "127.0.0.1";
    std::uint16_t port = 7000U;
    dxa::protocol::RoomId room;
    std::uint32_t count = 1U;
};

struct BotClientOptionsParseResult
{
    std::optional<BotClientOptions> options;
    std::string error;
};

[[nodiscard]] BotClientOptionsParseResult ParseBotClientOptions(
    std::span<const std::string_view> arguments);
} // namespace dxa::bot_client
