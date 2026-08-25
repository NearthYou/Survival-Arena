#pragma once

#include <dxa/protocol/ByteCodec.hpp>

#include <optional>

namespace dxa::protocol
{
template <typename MessageVariant>
struct MessageDecodeResult
{
    std::optional<MessageVariant> message;
    DecodeError error = DecodeError::None;
};
} // namespace dxa::protocol
