// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "BrickAllocator.h"

#include <stdexcept>
#include <sstream>
#include <utility>
#include <limits>

#include "appcontext.h"

using namespace std;

BrickAllocator::BrickAllocator(AppContext& context)
    : context_(context),
    next_functor_handle_(1)
{
    for (size_t i = 0; i < Count_of_MemoryTypes; ++i)
    {
        this->array_allocated_size_[i].store(0);
        this->array_max_memory_for_types_[i] = numeric_limits<uint64_t>::max();
    }
}

int BrickAllocator::AddHighWatermarkCrossedCallback(const std::function<void(bool)>& high_water_mark_crossed_functor)
{
    int handle = this->next_functor_handle_++;
    std::lock_guard<std::mutex> lck(this->mutex_callbacks_);
    if (this->high_water_mark_crossed_functors_.insert(pair(handle, high_water_mark_crossed_functor)).second != true)
    {
        ostringstream error_text;
        error_text << "handle " << handle << " was already existing, which is unexpected.";
        throw logic_error(error_text.str());
    }

    return handle;
}

bool BrickAllocator::RemoveHighWatermarkCrossedCallback(int handle)
{
    std::lock_guard<std::mutex> lck(this->mutex_callbacks_);
    if (this->high_water_mark_crossed_functors_.erase(handle) != 1)
    {
        return false;
    }

    return true;
}

void BrickAllocator::GetState(std::array<std::uint64_t, Count_of_MemoryTypes>& allocation_state)
{
    for (size_t i = 0; i < Count_of_MemoryTypes; ++i)
    {
        allocation_state[i] = this->array_allocated_size_[i].load();
    }
}

std::shared_ptr<void> BrickAllocator::Allocate(MemoryType type, size_t size, bool must_succeed)
{
    shared_ptr<void> memory;
    if (this->CanAllocateAndIfSuccessfulAddToAllocatedSize(type, size))
    {
        memory = shared_ptr<void>(
            malloc(size),
            [this, size, type](void* vp)
            {
                if (vp != nullptr)
                {
                    this->MemoryFreed(size);
                    this->array_allocated_size_[static_cast<size_t>(type)].fetch_sub(size);
                    free(vp);
                    if (type == MemoryType::DestinationBrick)
                    {
                        this->RaiseDestinationBrickMemoryReleased();
                    }
                }
            });

        if (!memory)
        {
            // undo the registration of the allocation (which did not succeed, so remove it from our bookkeeping)
            this->array_allocated_size_[static_cast<size_t>(type)].fetch_sub(size);
        }
    }

    if (!memory)
    {
        if (must_succeed == true)
        {
            ostringstream error_text;
            error_text.imbue(this->context_.GetFormattingLocale());
            error_text << "Failure to allocate " << size << " bytes of memory (type=" << BrickAllocator::MemoryTypeToInformalString(type) << ").\n";
            this->context_.FatalError(error_text.str());
        }

        return memory;
    }

    this->MemoryAllocated(size);
    return memory;
}

bool BrickAllocator::CanAllocateAndIfSuccessfulAddToAllocatedSize(MemoryType type, size_t size)
{
    if (CastToIn64ThrowIfTooLarge(this->GetTotalAllocatedMemory() + size) < this->max_memory_)
    {
        const uint64_t max_memory_for_type = this->array_max_memory_for_types_[static_cast<size_t>(type)];

        for (;;)
        {
            uint64_t current_memory_for_type = this->array_allocated_size_[static_cast<size_t>(type)].load();
            if (current_memory_for_type + size >= max_memory_for_type)
            {
                return false;
            }

            // Try to (atomically) make the adjustment - with the values we determined above. If this fails, it means that
            //  between those lines either memory was released or an increment was done - on a different thread. In this case
            //  (which is expected to occur rarely) we simply repeat the operation.
            if (this->array_allocated_size_[static_cast<size_t>(type)].compare_exchange_strong(current_memory_for_type, current_memory_for_type + size))
            {
                return true;
            }
        }
    }

    return false;
}

void BrickAllocator::MemoryChange(std::int64_t change)
{
    const int64_t before = this->bytes_allocated_.fetch_add(change);
    const int64_t after = before + change;
    if (before < this->high_water_mark_ && after >= this->high_water_mark_)
    {
        this->SignalHighwaterMarkCrossed(true);
    }
    else if (before >= this->high_water_mark_ && after < this->high_water_mark_)
    {
        this->SignalHighwaterMarkCrossed(false);
    }
}

void BrickAllocator::SignalHighwaterMarkCrossed(bool over_highwater_mark)
{
    std::lock_guard<std::mutex> lck(this->mutex_callbacks_);
    for (const auto& items : this->high_water_mark_crossed_functors_)
    {
        items.second(over_highwater_mark);
    }
}

std::int64_t BrickAllocator::GetTotalAllocatedMemory()
{
    return this->bytes_allocated_.load();
}

/*static*/const char* BrickAllocator::MemoryTypeToInformalString(MemoryType memory_type)
{
    switch (memory_type)
    {
    case MemoryType::SourceBrick:
        return "SourceBrick";
    case MemoryType::DestinationBrick:
        return "DestinationBrick";
    case MemoryType::CompressedDestinationSlice:
        return "CompressedDestinationSlice";
    }

    return "Invalid";
}

void BrickAllocator::SetMaximumMemoryLimitForMemoryType(MemoryType memory_type, std::uint64_t max_memory)
{
    this->array_max_memory_for_types_[static_cast<size_t>(memory_type)] = max_memory;
}

/*static*/std::int64_t BrickAllocator::CastToIn64ThrowIfTooLarge(std::uint64_t value)
{
    if (value > numeric_limits<int64_t>::max())
    {
        throw invalid_argument("Value cannot be converted to an int64_t.");
    }

    return static_cast<int64_t>(value);
}
