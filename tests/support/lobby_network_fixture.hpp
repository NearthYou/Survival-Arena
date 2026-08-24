#pragma once

#include <dxa/lobby/LobbyService.hpp>
#include <dxa/lobby/LobbyTcpServer.hpp>
#include <dxa/lobby/MatchTicketRegistry.hpp>
#include <dxa/lobby_client/LobbyClient.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace dxa::test
{
class DeterministicTicketSource final : public dxa::lobby::ITicketSource
{
public:
    [[nodiscard]] bool Fill(
        const std::span<std::byte, dxa::protocol::MatchTicketBytes> output) noexcept override
    {
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            output[index] = static_cast<std::byte>(
                static_cast<std::uint8_t>(next_ + index));
        }
        ++next_;
        return true;
    }

private:
    std::uint8_t next_ = 1U;
};

class RawLobbyServerFixture
{
public:
    explicit RawLobbyServerFixture(
        const std::optional<dxa::protocol::GameEndpoint> worker = std::nullopt)
        : work_{boost::asio::make_work_guard(io_)},
          tickets_{ticketSource_},
          service_{tickets_},
          server_{
              io_,
              service_,
              boost::asio::ip::tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}}
    {
        if (worker.has_value())
        {
            const dxa::protocol::GameEndpoint endpoint = *worker;
            server_.SetRuntimeActionHandler(
                [this, endpoint](const dxa::lobby::LobbyRuntimeAction& action) {
                    const auto* reserve =
                        std::get_if<dxa::lobby::ReserveMatchAction>(&action);
                    if (reserve == nullptr)
                    {
                        return;
                    }
                    server_.ApplyWorkerEvent(
                        dxa::lobby::ReservationReadyEvent{
                            reserve->reservation,
                            reserve->match,
                            dxa::protocol::WorkerId{1U},
                            endpoint},
                        std::chrono::steady_clock::now());
                });
        }
        server_.Start();
        thread_ = std::thread{[this] { io_.run(); }};
    }

    RawLobbyServerFixture(const RawLobbyServerFixture&) = delete;
    RawLobbyServerFixture& operator=(const RawLobbyServerFixture&) = delete;

    ~RawLobbyServerFixture()
    {
        boost::asio::post(io_, [this] {
            server_.Stop();
            work_.reset();
        });
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    [[nodiscard]] std::uint16_t Port() const
    {
        return server_.LocalPort();
    }

    [[nodiscard]] dxa::lobby::LobbyService& Service() noexcept
    {
        return service_;
    }

private:
    boost::asio::io_context io_;
    boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type> work_;
    DeterministicTicketSource ticketSource_;
    dxa::lobby::MatchTicketRegistry tickets_;
    dxa::lobby::LobbyService service_;
    dxa::lobby::LobbyTcpServer server_;
    std::thread thread_;
};

[[nodiscard]] inline dxa::protocol::GameEndpoint StaticEndpoint()
{
    return {"127.0.0.1", 7100U, 7101U};
}

struct LobbyClientProbe
{
    std::shared_ptr<dxa::lobby_client::LobbyClient> client;
    bool connected = false;
    std::vector<dxa::protocol::ServerMessage> messages;
    std::optional<boost::system::error_code> closedError;
};

class LobbyNetworkFixture : public RawLobbyServerFixture
{
public:
    explicit LobbyNetworkFixture(
        const std::optional<dxa::protocol::GameEndpoint> worker)
        : RawLobbyServerFixture{worker}
    {
    }

    ~LobbyNetworkFixture()
    {
        for (const auto& probe : clients_)
        {
            probe->client->Close();
        }
        clientIo_.restart();
        while (clientIo_.poll_one() != 0U)
        {
        }
    }

    [[nodiscard]] std::shared_ptr<LobbyClientProbe> AddClient()
    {
        auto probe = std::make_shared<LobbyClientProbe>();
        probe->client = dxa::lobby_client::LobbyClient::Create(clientIo_);
        const std::weak_ptr<LobbyClientProbe> weak = probe;
        probe->client->AsyncConnect(
            "127.0.0.1",
            Port(),
            dxa::lobby_client::LobbyClientCallbacks{
                [weak] {
                    if (const auto locked = weak.lock())
                    {
                        locked->connected = true;
                    }
                },
                [weak](dxa::protocol::ServerMessage message) {
                    if (const auto locked = weak.lock())
                    {
                        locked->messages.push_back(std::move(message));
                    }
                },
                [weak](const boost::system::error_code error) {
                    if (const auto locked = weak.lock())
                    {
                        locked->closedError = error;
                    }
                }});
        clients_.push_back(probe);
        return probe;
    }

    void ConnectAndWelcome(const std::shared_ptr<LobbyClientProbe>& probe)
    {
        RunUntil([&probe] { return probe->connected; });
        static_cast<void>(probe->client->Hello());
        RunUntil([&probe] {
            return std::any_of(
                probe->messages.begin(),
                probe->messages.end(),
                [](const auto& message) {
                    return std::holds_alternative<
                        dxa::protocol::ServerWelcome>(message);
                });
        });
    }

    [[nodiscard]] std::vector<std::shared_ptr<LobbyClientProbe>>
    AddWelcomedClients(const std::size_t count)
    {
        std::vector<std::shared_ptr<LobbyClientProbe>> clients;
        clients.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            clients.push_back(AddClient());
        }
        for (const auto& client : clients)
        {
            ConnectAndWelcome(client);
        }
        return clients;
    }

    void RunUntil(const std::function<bool()>& condition)
    {
        bool timedOut = false;
        boost::asio::steady_timer timer{clientIo_};
        timer.expires_after(std::chrono::seconds{5});
        timer.async_wait([&timedOut](const boost::system::error_code error) {
            if (!error)
            {
                timedOut = true;
            }
        });

        clientIo_.restart();
        while (!condition() && !timedOut)
        {
            if (clientIo_.run_one() == 0U)
            {
                break;
            }
        }
        timer.cancel();
        clientIo_.restart();
        while (clientIo_.poll_one() != 0U)
        {
        }
        if (!condition())
        {
            throw std::runtime_error{"lobby network test timed out"};
        }
    }

private:
    boost::asio::io_context clientIo_;
    std::vector<std::shared_ptr<LobbyClientProbe>> clients_;
};
} // namespace dxa::test
