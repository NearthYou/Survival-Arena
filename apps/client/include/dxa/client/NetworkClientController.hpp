#pragma once

#include <dxa/client/ClientOptions.hpp>
#include <dxa/engine/RuntimeScene.hpp>
#include <dxa/protocol/GameTcpMessages.hpp>
#include <dxa/protocol/Ids.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace dxa::client
{
class NetworkClientController final
    : public dxa::engine::IRuntimeSceneController
{
public:
    explicit NetworkClientController(NetworkClientOptions options);
    ~NetworkClientController() override;
    NetworkClientController(const NetworkClientController&) = delete;
    NetworkClientController& operator=(const NetworkClientController&) = delete;

    void Start();
    void FixedUpdate(const dxa::engine::RuntimeInputFrame& input) override;
    [[nodiscard]] dxa::engine::RuntimeSceneFrame SampleScene() override;
    [[nodiscard]] std::optional<dxa::protocol::RoomId> Room() const;
    [[nodiscard]] std::optional<dxa::protocol::GameMatchResult> Result() const;
    [[nodiscard]] std::uint64_t SnapshotCount() const noexcept;
    void Stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace dxa::client
