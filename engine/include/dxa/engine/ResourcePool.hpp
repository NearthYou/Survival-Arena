#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dxa::engine
{
template <typename T>
class ResourcePool;

template <typename T>
class ResourceHandle
{
public:
    constexpr ResourceHandle() noexcept = default;

    [[nodiscard]] constexpr std::uint32_t Index() const noexcept
    {
        return index_;
    }

    [[nodiscard]] constexpr std::uint32_t Generation() const noexcept
    {
        return generation_;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return index_ != InvalidIndex && generation_ != 0;
    }

    friend constexpr bool operator==(const ResourceHandle&, const ResourceHandle&) noexcept = default;

private:
    friend class ResourcePool<T>;

    constexpr ResourceHandle(const std::uint32_t index, const std::uint32_t generation) noexcept
        : index_(index), generation_(generation)
    {
    }

    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index_ = InvalidIndex;
    std::uint32_t generation_ = 0;
};

template <typename T>
class ResourcePool
{
public:
    template <typename... Args>
    [[nodiscard]] ResourceHandle<T> Create(Args&&... args)
    {
        std::uint32_t index = InvalidIndex;

        if (freeHead_ != InvalidIndex)
        {
            index = freeHead_;
            Slot& slot = slots_[index];
            const std::uint32_t nextFree = slot.nextFree;
            slot.value.emplace(std::forward<Args>(args)...);
            freeHead_ = nextFree;
            slot.nextFree = InvalidIndex;
        }
        else
        {
            if (slots_.size() >= static_cast<std::size_t>(InvalidIndex))
            {
                throw std::length_error("resource pool exhausted its handle index range");
            }

            index = static_cast<std::uint32_t>(slots_.size());
            slots_.emplace_back();
            try
            {
                slots_.back().value.emplace(std::forward<Args>(args)...);
            }
            catch (...)
            {
                slots_.pop_back();
                throw;
            }
        }

        ++liveCount_;
        return ResourceHandle<T>{index, slots_[index].generation};
    }

    [[nodiscard]] T* Get(const ResourceHandle<T> handle) noexcept
    {
        Slot* slot = FindSlot(handle);
        return slot == nullptr ? nullptr : &*slot->value;
    }

    [[nodiscard]] const T* Get(const ResourceHandle<T> handle) const noexcept
    {
        const Slot* slot = FindSlot(handle);
        return slot == nullptr ? nullptr : &*slot->value;
    }

    bool Destroy(const ResourceHandle<T> handle) noexcept
    {
        Slot* slot = FindSlot(handle);
        if (slot == nullptr)
        {
            return false;
        }

        slot->value.reset();
        slot->generation = NextGeneration(slot->generation);
        slot->nextFree = freeHead_;
        freeHead_ = handle.Index();
        --liveCount_;
        return true;
    }

    [[nodiscard]] std::size_t LiveCount() const noexcept
    {
        return liveCount_;
    }

private:
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    struct Slot
    {
        std::optional<T> value;
        std::uint32_t generation = 1;
        std::uint32_t nextFree = InvalidIndex;
    };

    [[nodiscard]] Slot* FindSlot(const ResourceHandle<T> handle) noexcept
    {
        if (!handle || handle.Index() >= slots_.size())
        {
            return nullptr;
        }

        Slot& slot = slots_[handle.Index()];
        if (!slot.value.has_value() || slot.generation != handle.Generation())
        {
            return nullptr;
        }

        return &slot;
    }

    [[nodiscard]] const Slot* FindSlot(const ResourceHandle<T> handle) const noexcept
    {
        if (!handle || handle.Index() >= slots_.size())
        {
            return nullptr;
        }

        const Slot& slot = slots_[handle.Index()];
        if (!slot.value.has_value() || slot.generation != handle.Generation())
        {
            return nullptr;
        }

        return &slot;
    }

    [[nodiscard]] static constexpr std::uint32_t NextGeneration(const std::uint32_t generation) noexcept
    {
        return generation == std::numeric_limits<std::uint32_t>::max() ? 1U : generation + 1U;
    }

    std::deque<Slot> slots_;
    std::uint32_t freeHead_ = InvalidIndex;
    std::size_t liveCount_ = 0;
};
} // namespace dxa::engine
