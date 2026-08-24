#pragma once

#include <dxa/lobby/GameWorkerAllocator.hpp>
#include <dxa/lobby/LobbyService.hpp>
#include <dxa/lobby/LobbyTcpServer.hpp>
#include <dxa/lobby/MatchTicketRegistry.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>

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
    RawLobbyServerFixture()
        : work_{boost::asio::make_work_guard(io_)},
          tickets_{ticketSource_},
          service_{allocator_, tickets_},
          server_{
              io_,
              service_,
              boost::asio::ip::tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}}
    {
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
    dxa::lobby::UnavailableGameWorkerAllocator allocator_;
    dxa::lobby::LobbyService service_;
    dxa::lobby::LobbyTcpServer server_;
    std::thread thread_;
};
} // namespace dxa::test
