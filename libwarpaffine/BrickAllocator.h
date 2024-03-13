// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <memory>
#include <map>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <array>
#include <limits>

class AppContext;

class BrickAllocator
{
public:
    enum class MemoryType
    {
        SourceBrick = 0,
        DestinationBrick,
        CompressedDestinationSlice,

        Max
    }; 

    static constexpr size_t Count_of_MemoryTypes = static_cast<size_t>(MemoryType::Max);

    static const char* MemoryTypeToInformalString(MemoryType memory_type);
private:
    AppContext& context_;
    std::atomic_int32_t next_functor_handle_;
    std::mutex mutex_callbacks_;
    std::map<int, std::function<void(bool)>> high_water_mark_crossed_functors_;
    std::atomic_int64_t bytes_allocated_{ 0 };
    std::int64_t high_water_mark_{ std::numeric_limits<std::int64_t>::max() };

    /// The maximum amount of memory we allow ourselves to allocate. This limit is applied to
    /// all memory types.
    std::int64_t max_memory_{ std::numeric_limits<std::int64_t>::max() };   

    std::array<std::atomic_uint64_t, Count_of_MemoryTypes> array_allocated_size_;
    std::array<std::uint64_t, Count_of_MemoryTypes> array_max_memory_for_types_;

    std::function<void()> func_for_released;
public:
    BrickAllocator() = delete;
    explicit BrickAllocator(AppContext& context);

    void AddDestinationBrickMemoryReleasedCallback(const std::function<void()>& func)
    {
        this->func_for_released = func;
    }

    /// Sets the high watermark - if the amount of allocated memory is crossing this number, then
    /// the high-watermark-crossed-callback will be raised.
    /// \param  high_water_mark The high water mark (in bytes).
    void SetHighWatermark(std::uint64_t high_water_mark)
    {
        this->high_water_mark_ = CastToIn64ThrowIfTooLarge(high_water_mark);
    }

    void SetMaximumMemoryLimit(std::uint64_t max_memory)
    {
        this->max_memory_ = CastToIn64ThrowIfTooLarge(max_memory);
    }

    void SetMaximumMemoryLimitForMemoryType(MemoryType memory_type, std::uint64_t max_memory);

    int AddHighWatermarkCrossedCallback(const std::function<void(bool)>& high_water_mark_crossed_functor);
    bool RemoveHighWatermarkCrossedCallback(int handle);

    /// Allocates the specified amount of memory. If 'must_succeed' is specified as 'true', then
    /// this allocation either succeeds or the application is terminated. If 'false' is given here,
    /// then the call may return with a null-pointer (in case of failure).
    ///
    /// \param  type            The type of the memory requested.
    /// \param  size            The size in bytes.
    /// \param  must_succeed    (Optional) True if the allocation must succeed. In this case, the application is
    ///                         terminated if the memory cannot be allocated. If false is specified here, the
    ///                         call may return a null-pointer (in case of failure).
    ///
    /// \returns    A std::shared_ptr&lt;void&gt; object representing the newly allocated memory.
    std::shared_ptr<void> Allocate(MemoryType type, size_t size, bool must_succeed = true);

    void GetState(std::array<std::uint64_t, Count_of_MemoryTypes>& allocation_state);
private:
    void MemoryFreed(size_t size) { this->MemoryChange(-(std::int64_t)size); }
    void MemoryAllocated(size_t size) { this->MemoryChange((std::int64_t)size); }
    void MemoryChange(std::int64_t change);
    void SignalHighwaterMarkCrossed(bool over_highwater_mark);
    std::int64_t GetTotalAllocatedMemory();
    static std::int64_t CastToIn64ThrowIfTooLarge(std::uint64_t value);
    bool CanAllocateAndIfSuccessfulAddToAllocatedSize(MemoryType type, size_t size);

    void RaiseDestinationBrickMemoryReleased()
    {
        if (this->func_for_released)
        {
            this->func_for_released();
        }
    }
};
