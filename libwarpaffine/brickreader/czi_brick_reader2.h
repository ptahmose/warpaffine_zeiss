// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <memory>

#include "brick_enumerator.h"
#include "IBrickReader.h"
#include "czi_brick_reader_base.h"
#include "../inc_libCZI.h"
#include "../mmstream/IStreamEx.h"
#include "../appcontext.h"
#include "../brick.h"

/// This is an implementation of the ICziBrickReader-interface which features asynchronous
/// decompression.
class CziBrickReader2 : public CziBrickReaderBase, public ICziBrickReader
{
private:
    std::atomic_bool is_paused_externally_{ false };
    std::shared_ptr<IStreamEx> input_stream_;
    std::vector<std::thread> reader_threads_;

    std::atomic_bool isDone_{ false };
    std::function<void(const Brick&, const BrickCoordinateInfo&)> deliver_brick_func_;

    std::atomic_uint64_t statistics_number_of_compressed_subblocks_in_flight_{ 0 };
    std::atomic_uint64_t statistics_number_of_uncompressed_planes_in_flight_{ 0 };
    std::atomic_uint64_t statistics_brick_data_delivered{ 0 };
    std::atomic_uint64_t statistics_bricks_delivered{ 0 };
    std::atomic_uint64_t statistics_slices_read{ 0 };

    std::atomic_uint32_t pending_tasks_count_{ 0 };

    int handle_high_watermark_callback_;
public:
    CziBrickReader2(AppContext& context, const std::shared_ptr<libCZI::ICZIReader>& reader, std::shared_ptr<IStreamEx> stream);

    void StartPumping(const std::function<void(const Brick&, const BrickCoordinateInfo&)>& deliver_brick_func) override;
    bool IsDone() override;
    BrickReaderStatistics GetStatus() override;
    std::shared_ptr<libCZI::ICZIReader>& GetUnderlyingReader() override;
    void WaitUntilDone() override;
    void SetPauseState(bool pause) override;
    bool GetIsThrottledState() override;
private:
    BrickEnumerator brick_enumerator_;
    void ReadBrick();
    Brick CreateBrick(const libCZI::CDimCoordinate& coordinate, const libCZI::IntRect& rectangle);
    void DoBrick(const libCZI::CDimCoordinate& coordinate, TileIdentifier tile_identifier, const libCZI::IntRect& rectangle, Brick& brick);

    struct BrickOutputInfo
    {
        int max_count;
        std::atomic<int> counter;
        Brick output_brick;
    };

    struct BrickDecodeInfo
    {
        std::shared_ptr<libCZI::ISubBlock> subBlock;
        BrickOutputInfo* brick_output_info;
    };

    /// Copies the bitmap into brick. The bitmap is allowed to be smaller than the brick (in X-Y),
    /// but it must be fully contained in the brick's X-Y-extent.
    ///
    /// \param          subblock_info   Information describing the subblock.
    /// \param          z               The z coordinate.
    /// \param [in,out] bitmap          If non-null, the bitmap.
    /// \param          decode_info     If non-null, information describing the decoding.
    /// \param          rectangle       The rectangle.
    void CopySubblockIntoBrick(const libCZI::SubBlockInfo& subblock_info, int z, libCZI::IBitmapData* bitmap, const BrickDecodeInfo* decode_info, const libCZI::IntRect& rectangle);
};
