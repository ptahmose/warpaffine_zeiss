// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <optional>
#include <vector>
#include <memory>

#include "IBrickReader.h"
#include "brick_enumerator.h"
#include "czi_brick_reader_base.h"
#include "../inc_libCZI.h"
#include "../mmstream/IStreamEx.h"
#include "../appcontext.h"
#include "../brick.h"
#include "../czi_helpers.h"

/// This is the simplest implementation of a CziBrickReader. The features of this reader are:
/// - bricks are read in a fixed order    
/// - we have a configurable number of worker-threads (1 at minimum)
/// - each worker-thread is reading and decoding the subblocks (so, I/O and decompression are NOT decoupled)
class CziBrickReader : public CziBrickReaderBase, public ICziBrickReader
{
private:
    std::vector<std::thread> reader_threads_;

    std::vector<std::thread> plane_reader_threads;

    std::shared_ptr<IStreamEx> input_stream_;
    std::shared_ptr<libCZI::ISingleChannelTileAccessor> accessor_;

    std::atomic_bool isDone_{ false };
    std::function<void(const Brick&, const BrickCoordinateInfo&)> deliver_brick_func_;

    std::atomic_uint64_t statistics_brick_data_delivered_{ 0 };
    std::atomic_uint64_t statistics_bricks_delivered_{ 0 };
    std::atomic_uint64_t statistics_slices_read_{ 0 };
public:
    CziBrickReader(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream);

    void StartPumping(const std::function<void(const Brick&, const BrickCoordinateInfo&)>& deliver_brick_func) override;
    bool IsDone() override;
    BrickReaderStatistics GetStatus() override;
    void WaitUntilDone() override;
    std::shared_ptr<libCZI::ICZIReader>& GetUnderlyingReader() override;
    void SetPauseState(bool pause) override {}
    bool GetIsThrottledState() override { return false; }
private:
    BrickEnumerator brick_enumerator_;
    void ReadBrick();
    Brick CreateBrick(const libCZI::CDimCoordinate& coordinate, const libCZI::IntRect& rectangle);
    void FillBrick(const libCZI::CDimCoordinate& coordinate, const libCZI::IntRect& rectangle, Brick& brick);
};
