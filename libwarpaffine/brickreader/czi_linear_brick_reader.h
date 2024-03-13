// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <limits>
#include <vector>
#include <map>
#include <memory>
#include "../inc_libCZI.h"
#include "../mmstream/IStreamEx.h"
#include "../appcontext.h"
#include "../brick.h"
#include "IBrickReader.h"
#include "brick_bucket_manager.h"
#include "brick_coordinate.h"

/// This brick-reader implementation is following the idea to read the
/// file as contiguously as possible. In best case, we read the file from start
/// to end, i.e. with linearly increasing file-position and no seeks backwards/
/// forwards in between.
class CziBrickReaderLinearReading : public ICziBrickReader
{
private:
    AppContext& context_;
    std::shared_ptr<libCZI::ICZIReader> reader_;
    std::shared_ptr<IStreamEx> input_stream_;
    libCZI::SubBlockStatistics statistics_;
    std::map<int, libCZI::PixelType> map_channelno_to_pixeltype_;
    std::vector<std::thread> reader_threads_;

    std::function<void(const Brick&, const BrickCoordinateInfo&)> deliver_brick_func_;

    /// This is counting the number of "libCZI-subblocks" (containing the compressed data)
    std::atomic_uint64_t statistics_number_of_compressed_subblocks_in_flight_{ 0 };

    /// Counting the number of "uncompressed planes" around - we increment this whenever we add
    /// something to the "brick_bucket_manager", and decrement it when a "complete bricks" comes out
    /// of it.
    std::atomic_uint64_t statistics_number_of_uncompressed_planes_in_flight_{0};
    
    std::atomic_uint64_t statistics_brick_data_delivered{ 0 };
    std::atomic_uint64_t statistics_bricks_delivered{ 0 };
    std::atomic_uint64_t statistics_slices_read{ 0 };

    std::atomic_uint32_t pending_tasks_count_{ 0 };
    std::atomic_bool isDone_{ false };

    std::atomic_bool isPaused_{ false };
    std::atomic_bool isThrottledInternally_{ false };

    std::atomic_uint32_t active_bricks_count_{ 0 };

    std::atomic_uint64_t memory_used_by_subblocks_in_queue_{ 0 };

    std::uint64_t max_size_of_subblocks_queued_{ (std::numeric_limits<std::uint64_t>::max)() };

    BrickBucketManager brick_bucket_manager_;

    int handle_high_watermark_callback_;
public:
    CziBrickReaderLinearReading(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream);

public:
    void StartPumping(const std::function<void(const Brick&, const BrickCoordinateInfo&)>& deliver_brick_func) override;
    bool IsDone() override;
    BrickReaderStatistics GetStatus() override;
    void WaitUntilDone() override;
    std::shared_ptr<libCZI::ICZIReader>& GetUnderlyingReader() override;
    void SetPauseState(bool pause) override { this->isPaused_ = pause; }
    bool GetIsThrottledState() override { return this->isThrottledInternally_.load() || this->isPaused_.load(); }
private:
    std::atomic_int32_t next_subblock_index_to_read_{0};  ///< The next subblock index to be read (i.e. an index into the subblocks_ordered_-vector).
    std::vector<int> subblocks_ordered_;                  ///< This array contains the order in which the subblocks are to be read from the file.

    std::map<BrickCoordinate, std::uint32_t> GenerateReadInfo();
    void ReadSubblocksThread();

    void DecompressTask(const std::shared_ptr<libCZI::ISubBlock>& subblock);
    void BrickCompleted(const std::shared_ptr<IBrickResult>& brick_result);
    void ComposeBrickTask(const std::shared_ptr<IBrickResult>& brick_result);
    static std::uint64_t DetermineMemorySizeOfSubblock(libCZI::ISubBlock* subblock);
};
