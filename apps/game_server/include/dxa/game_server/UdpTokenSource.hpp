#pragma once

#include <cstddef>
#include <span>

namespace dxa::game_server
{
class IUdpTokenSource
{
public:
    virtual ~IUdpTokenSource() = default;

    [[nodiscard]] virtual bool Fill(
        std::span<std::byte, 16U> output) noexcept = 0;
};

class SecureUdpTokenSource final : public IUdpTokenSource
{
public:
    [[nodiscard]] bool Fill(
        std::span<std::byte, 16U> output) noexcept override;
};
} // namespace dxa::game_server
