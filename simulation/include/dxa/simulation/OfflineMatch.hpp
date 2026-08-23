#pragma once

#include <dxa/simulation/MatchConfig.hpp>
#include <dxa/simulation/MatchTypes.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <memory>
#include <vector>

namespace dxa::simulation
{
class OfflineMatch
{
public:
    [[nodiscard]] static OfflineMatch Create(
        const NavMesh& navMesh,
        MatchConfig config = DefaultMatchConfig());

    ~OfflineMatch();
    OfflineMatch(OfflineMatch&&) noexcept;
    OfflineMatch& operator=(OfflineMatch&&) noexcept;
    OfflineMatch(const OfflineMatch&) = delete;
    OfflineMatch& operator=(const OfflineMatch&) = delete;

    void Start();
    void Submit(MatchCommand command);
    void Step();
    [[nodiscard]] MatchSnapshot Snapshot() const;
    [[nodiscard]] std::vector<MatchEvent> DrainEvents();

private:
    struct Impl;

    explicit OfflineMatch(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};
} // namespace dxa::simulation
