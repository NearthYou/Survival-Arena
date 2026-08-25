#include <dxa/bot_client/BotClientOptions.hpp>
#include <dxa/bot_client/BotCoordinator.hpp>

#include <boost/asio.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

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
        const auto parsed =
            dxa::bot_client::ParseBotClientOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }

        boost::asio::io_context io;
        dxa::bot_client::BotCoordinator coordinator{io, *parsed.options};
        coordinator.Start();
        io.run();
        return coordinator.Report().exitCode;
    }
    catch (const std::exception& error)
    {
        std::cerr << "bot client failed: " << error.what() << '\n';
        return 3;
    }
}
