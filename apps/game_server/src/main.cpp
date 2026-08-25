#include <dxa/game_server/GameServer.hpp>
#include <dxa/game_server/GameServerOptions.hpp>

#include <boost/asio.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
class ServerMetricsExporter
{
public:
    explicit ServerMetricsExporter(const std::filesystem::path& outputRoot)
        : tickPath_{outputRoot / "server-ticks.csv"},
          replicationPath_{outputRoot / "replication.csv"}
    {
        if (!std::filesystem::is_directory(outputRoot))
        {
            throw std::invalid_argument{
                "metrics output root must be an existing directory"};
        }
        if (std::filesystem::exists(tickPath_)
            || std::filesystem::exists(replicationPath_))
        {
            throw std::invalid_argument{
                "metrics output files already exist"};
        }
        WriteNewFile(
            tickPath_,
            "match_id,sample_index,duration_ns,tick_p95_ns,tcp_bytes,"
            "udp_bytes,payload_bytes,scheduler_overruns\n");
        WriteNewFile(
            replicationPath_,
            "match_id,sample_index,encode_duration_ns,replication_p95_ns,"
            "payload_bytes,fragment_count,visible_actors,visible_loot,"
            "keyframe,fallback_keyframe\n");
    }

    void Export(const dxa::game_server::GameServer& server)
    {
        if (server.CompletedMetricCount() <= exportedMatches_.size())
        {
            return;
        }
        const auto completed = server.CompletedMetrics();
        const bool hasPending = std::any_of(
            completed.begin(),
            completed.end(),
            [this](const auto& match) {
                return !exportedMatches_.contains(match.match.value);
            });
        if (!hasPending)
        {
            return;
        }
        std::ofstream ticks{tickPath_, std::ios::binary | std::ios::app};
        std::ofstream replication{
            replicationPath_,
            std::ios::binary | std::ios::app};
        if (!ticks || !replication)
        {
            throw std::runtime_error{"server metrics output open failed"};
        }

        for (const auto& match : completed)
        {
            if (exportedMatches_.contains(match.match.value))
            {
                continue;
            }
            for (std::size_t index = 0U;
                 index < match.tickSamples.size();
                 ++index)
            {
                ticks << match.match.value << ','
                      << index << ','
                      << match.tickSamples[index].count() << ','
                      << match.tickP95.count() << ','
                      << match.tcpBytes << ','
                      << match.udpBytes << ','
                      << match.payloadBytes << ','
                      << match.schedulerOverruns << '\n';
            }
            for (std::size_t index = 0U;
                 index < match.replicationSamples.size();
                 ++index)
            {
                const auto& sample = match.replicationSamples[index];
                replication << match.match.value << ','
                            << index << ','
                            << sample.encodeDuration.count() << ','
                            << match.replicationP95.count() << ','
                            << sample.payloadBytes << ','
                            << sample.fragmentCount << ','
                            << sample.visibleActors << ','
                            << sample.visibleLoot << ','
                            << (sample.keyframe ? 1 : 0) << ','
                            << (sample.fallbackKeyframe ? 1 : 0) << '\n';
            }
            ticks.flush();
            replication.flush();
            if (!ticks || !replication)
            {
                throw std::runtime_error{"server metrics output write failed"};
            }
            exportedMatches_.insert(match.match.value);
            spdlog::info(
                "game_metrics_exported match={} ticks={} replication={}",
                match.match.value,
                match.tickSamples.size(),
                match.replicationSamples.size());
        }
    }

private:
    static void WriteNewFile(
        const std::filesystem::path& path,
        const std::string_view contents)
    {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (!output)
        {
            throw std::runtime_error{"server metrics output create failed"};
        }
        output << contents;
        if (!output)
        {
            throw std::runtime_error{"server metrics header write failed"};
        }
    }

    std::filesystem::path tickPath_;
    std::filesystem::path replicationPath_;
    std::set<std::uint64_t> exportedMatches_;
};
} // namespace

int main(const int argc, const char* const* const argv)
{
    try
    {
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
        for (int index = 1; index < argc; ++index)
        {
            arguments.emplace_back(argv[index]);
        }
        const auto parsed = dxa::game_server::ParseGameServerOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }

        boost::asio::io_context io;
        dxa::game_server::GameServerConfig config;
        config.options = *parsed.options;
        std::optional<ServerMetricsExporter> metricsExporter;
        if (!parsed.options->metricsOutputRoot.empty())
        {
            metricsExporter.emplace(parsed.options->metricsOutputRoot);
        }
        dxa::game_server::GameServer server{io, config};
        boost::asio::steady_timer metricsTimer{io};
        std::function<void()> scheduleMetricsExport;
        if (metricsExporter.has_value())
        {
            scheduleMetricsExport = [&] {
                metricsTimer.expires_after(std::chrono::milliseconds{100});
                metricsTimer.async_wait(
                    [&](const boost::system::error_code error) {
                        if (!error)
                        {
                            metricsExporter->Export(server);
                            scheduleMetricsExport();
                        }
                    });
            };
            scheduleMetricsExport();
        }
        boost::asio::signal_set signals{io, SIGINT, SIGTERM};
        signals.async_wait(
            [&](const boost::system::error_code error, const int) {
                if (!error)
                {
                    metricsTimer.cancel();
                    if (metricsExporter.has_value())
                    {
                        metricsExporter->Export(server);
                    }
                    server.Stop();
                }
            });

        server.Start();
        spdlog::info(
            "game_server_listening worker={} tcp_port={} udp_port={} "
            "replication_mode={} metrics_output={}",
            parsed.options->worker.value,
            server.GameTcpPort(),
            server.GameUdpPort(),
            parsed.options->replicationMode
                == dxa::protocol::ReplicationMode::FullState
                ? "full-state"
                : "unsupported",
            parsed.options->metricsOutputRoot.empty()
                ? "disabled"
                : "enabled");
        io.run();
        if (metricsExporter.has_value())
        {
            metricsExporter->Export(server);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        spdlog::error("game server failed: {}", error.what());
        return 1;
    }
}
