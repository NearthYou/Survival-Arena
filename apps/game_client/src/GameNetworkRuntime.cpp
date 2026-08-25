#include <dxa/game_client/GameNetworkRuntime.hpp>

#include <boost/asio/executor_work_guard.hpp>

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

namespace dxa::game_client
{
struct GameNetworkRuntime::Impl
{
    boost::asio::io_context io;
    std::optional<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> work;
    std::thread thread;
    mutable std::mutex mutex;
    std::atomic<std::thread::id> threadId{std::thread::id{}};
    bool started = false;
    bool stopped = false;
};

GameNetworkRuntime::GameNetworkRuntime()
    : impl_{std::make_shared<Impl>()}
{
}

GameNetworkRuntime::~GameNetworkRuntime()
{
    Stop();
}

bool GameNetworkRuntime::Start()
{
    std::scoped_lock lock{impl_->mutex};
    if (impl_->started || impl_->stopped)
    {
        return false;
    }

    impl_->started = true;
    impl_->work.emplace(boost::asio::make_work_guard(impl_->io));
    const std::shared_ptr<Impl> state = impl_;
    impl_->thread = std::thread{[state] {
        state->threadId.store(std::this_thread::get_id());
        state->io.run();
        state->threadId.store(std::thread::id{});
    }};
    return true;
}

void GameNetworkRuntime::Stop()
{
    std::thread* thread = nullptr;
    {
        std::scoped_lock lock{impl_->mutex};
        if (!impl_->started || impl_->stopped)
        {
            return;
        }
        impl_->stopped = true;
        impl_->work.reset();
        impl_->io.stop();
        thread = &impl_->thread;
    }

    if (!thread->joinable())
    {
        return;
    }
    if (thread->get_id() == std::this_thread::get_id())
    {
        thread->detach();
        return;
    }
    thread->join();
}

boost::asio::io_context& GameNetworkRuntime::Io() noexcept
{
    return impl_->io;
}

bool GameNetworkRuntime::Started() const noexcept
{
    std::scoped_lock lock{impl_->mutex};
    return impl_->started && !impl_->stopped;
}

bool GameNetworkRuntime::RunningOnThisThread() const noexcept
{
    return impl_->threadId.load() == std::this_thread::get_id();
}
} // namespace dxa::game_client
