#include <dxa/lobby/MatchTicketRegistry.hpp>

#if defined(_WIN32)
#include <Windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <sys/random.h>
#endif

#include <cstddef>

namespace dxa::lobby
{
bool SecureTicketSource::Fill(
    const std::span<std::byte, dxa::protocol::MatchTicketBytes> output) noexcept
{
#if defined(_WIN32)
    const NTSTATUS status = BCryptGenRandom(
        nullptr,
        reinterpret_cast<PUCHAR>(output.data()),
        static_cast<ULONG>(output.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return status >= 0;
#else
    std::size_t offset = 0;
    while (offset < output.size())
    {
        const ssize_t received = getrandom(
            output.data() + offset,
            output.size() - offset,
            0);
        if (received > 0)
        {
            offset += static_cast<std::size_t>(received);
            continue;
        }
        if (received < 0 && errno == EINTR)
        {
            continue;
        }
        return false;
    }
    return true;
#endif
}
} // namespace dxa::lobby
