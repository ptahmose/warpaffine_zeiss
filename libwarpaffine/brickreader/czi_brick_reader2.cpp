// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "czi_brick_reader2.h"
#include <utility>
#include <memory>
#include <limits>

using namespace std;
using namespace libCZI;

CziBrickReader2::CziBrickReader2(AppContext& context, const std::shared_ptr<libCZI::ICZIReader>& reader, std::shared_ptr<IStreamEx> stream)
    : CziBrickReaderBase(context, reader)
{
    this->input_stream_ = std::move(stream);

    // calculate size of a brick
    int z_count;
    this->GetStatistics().dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &z_count);
    size_t size_of_brick = static_cast<size_t>(z_count) * this->GetStatistics().boundingBox.w * this->GetStatistics().boundingBox.h * 2;

    this->handle_high_watermark_callback_ = this->GetContextBase().GetAllocator().AddHighWatermarkCrossedCallback(
        [this](bool above_high_watermark)->void
        {
            if (above_high_watermark)
            {
                this->SetPauseState(true);
            }
            else
            {
                this->SetPauseState(false);
            }
        });
}

void CziBrickReader2::SetPauseState(bool pause)
{
    this->is_paused_externally_.store(pause);
}

bool CziBrickReader2::GetIsThrottledState()
{
    return this->is_paused_externally_.load();
}

std::shared_ptr<libCZI::ICZIReader>& CziBrickReader2::GetUnderlyingReader()
{
    return this->GetUnderlyingReaderBase();
}

void CziBrickReader2::StartPumping(
    const std::function<void(const Brick&, const BrickCoordinateInfo&)>& deliver_brick_func)
{
    const int kNumberOfReadingThreads = this->GetContextBase().GetCommandLineOptions().GetNumberOfReaderThreads();

    int c_count;
    optional<int> t_count_optional;
    int t_count;
    if (this->GetStatistics().dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count))
    {
        t_count_optional = t_count;
    }

    if (!this->GetStatistics().dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count))
    {
        throw invalid_argument("The document must have a C-dimension.");
    }

    const auto tile_identifier_and_rect = CziHelpers::DetermineTileIdentifierToRectangleMap(this->GetUnderlyingReader().get());
    this->brick_enumerator_.Reset(t_count_optional, c_count, tile_identifier_and_rect);

    this->isDone_.store(false);

    this->deliver_brick_func_ = deliver_brick_func;

    this->pending_tasks_count_.store(0);

    for (int i = 0; i < kNumberOfReadingThreads; ++i)
    {
        this->reader_threads_.emplace_back([this] { this->ReadBrick(); });
    }
}

void CziBrickReader2::ReadBrick()
{
    for (;;)
    {
        CDimCoordinate coordinate_of_brick;
        TileIdentifier tile_identifier;
        libCZI::IntRect rectangle_of_brick;
        if (!this->brick_enumerator_.GetNextBrickCoordinate(coordinate_of_brick, tile_identifier, rectangle_of_brick))
        {
            break;
        }

        {
            Brick brick = this->CreateBrick(coordinate_of_brick, rectangle_of_brick);
            this->DoBrick(coordinate_of_brick, tile_identifier, rectangle_of_brick, brick);
        }

        while (this->GetIsThrottledState())
        {
            this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    this->isDone_.store(true);
}

Brick CziBrickReader2::CreateBrick(const libCZI::CDimCoordinate& coordinate, const libCZI::IntRect& rectangle)
{
    int zCount;
    this->GetStatistics().dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &zCount);

    int c;
    coordinate.TryGetPosition(DimensionIndex::C, &c);

    Brick brick;
    brick.info.pixelType = this->GetPixelTypeForChannelNo(c);
    brick.info.width = rectangle.w;
    brick.info.height = rectangle.h;
    brick.info.depth = zCount;
    brick.info.stride_line = Utils::GetBytesPerPixel(brick.info.pixelType) * brick.info.width;
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;
    for (;;)
    {
        brick.data = this->GetContextBase().GetAllocator().Allocate(
            BrickAllocator::MemoryType::SourceBrick,
            static_cast<size_t>(brick.info.stride_plane) * brick.info.depth,
            false);
        if (!brick.data)
        {
            this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else
        {
            break;
        }
    }

    return brick;
}

void CziBrickReader2::DoBrick(const libCZI::CDimCoordinate& coordinate, /*int m_index,*/TileIdentifier tile_identifier, const libCZI::IntRect& rectangle, Brick& brick)
{
    map<int, int> map_z_subblockindex = CziHelpers::GetSubblocksForBrick(
        this->GetUnderlyingReaderBase().get(),
        coordinate,
        tile_identifier);

/*    
    ostringstream ss;
    ss << "DoBrick: " << Utils::DimCoordinateToString(&coordinate) << " " << tile_identifier.ToInformalString() << " -> size=" << map_z_subblockindex.size() << endl;
    this->GetContextBase().WriteDebugString(ss.str().c_str());
*/

    BrickOutputInfo* brick_output_data = new BrickOutputInfo();
    brick_output_data->max_count = static_cast<int>(map_z_subblockindex.size());
    brick_output_data->counter.store(0);
    brick_output_data->output_brick = brick;

    // now, read those subblocks
    map<int, std::shared_ptr<ISubBlock>> map_z_subblock;
    for (const auto& item : map_z_subblockindex)
    {
        BrickDecodeInfo* decode_info = new BrickDecodeInfo();
        decode_info->subBlock = this->GetUnderlyingReaderBase()->ReadSubBlock(item.second);
        ++this->statistics_number_of_compressed_subblocks_in_flight_;

        decode_info->brick_output_info = brick_output_data;

        this->statistics_slices_read.fetch_add(1);

        ++this->pending_tasks_count_;
        this->GetContextBase().GetTaskArena()->AddTask(
            TaskType::BrickComposition,
            [this, decode_info, coordinate, tile_identifier/*m_index*/, rectangle, brick]()->void
            {
                const auto bitmap = decode_info->subBlock->CreateBitmap();
                ++this->statistics_number_of_uncompressed_planes_in_flight_;

                int z;
                const auto& subblock_info = decode_info->subBlock->GetSubBlockInfo();
                subblock_info.coordinate.TryGetPosition(DimensionIndex::Z, &z);

                {
                    if (bitmap->GetPixelType() != decode_info->brick_output_info->output_brick.info.pixelType)
                    {
                        this->GetContextBase().FatalError("CziBrickReader2::DoBrick - pixeltype of subblock different than the expectation.");
                    }

                    this->CopySubblockIntoBrick(subblock_info, z, bitmap.get(), decode_info, rectangle);
                }

                if (decode_info->brick_output_info->counter.fetch_add(1) + 1 == decode_info->brick_output_info->max_count)
                {
                    // ok, this means the brick is done, we can deliver it
                    if (this->deliver_brick_func_)
                    {
                        BrickCoordinateInfo brick_coordinate_info;
                        brick_coordinate_info.coordinate = coordinate;
                        brick_coordinate_info.mIndex = tile_identifier.m_index.value_or(std::numeric_limits<int>::min());
                        brick_coordinate_info.scene_index = tile_identifier.scene_index.value_or(std::numeric_limits<int>::min());
                        brick_coordinate_info.x_position = rectangle.x;
                        brick_coordinate_info.y_position = rectangle.y;

                        this->deliver_brick_func_(brick, brick_coordinate_info);
                    }

                    this->statistics_bricks_delivered.fetch_add(1);
                    this->statistics_brick_data_delivered.fetch_add(brick.info.GetBrickDataSize());

                    delete decode_info->brick_output_info;
                }

                delete decode_info;

                --this->statistics_number_of_compressed_subblocks_in_flight_;
                --this->statistics_number_of_uncompressed_planes_in_flight_;
                --this->pending_tasks_count_;
            });
    }
}

void CziBrickReader2::CopySubblockIntoBrick(const libCZI::SubBlockInfo& subblock_info, int z, libCZI::IBitmapData* bitmap, const BrickDecodeInfo* decode_info, const libCZI::IntRect& rectangle)
{
    const libCZI::ScopedBitmapLocker bitmap_locker(bitmap);

    // We copy the source bitmap into the destination brick. The source bitmap is allowed to be smaller than the destination brick.
    // We allow for the subblock being smaller (or at a different location), but still have the restriction that there will be only 
    // one subblock per slice. We copy the bitmap to the correct location (and fill the not covered parts with zero).
    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(
        {
            subblock_info.logicalRect.x - rectangle.x,
            subblock_info.logicalRect.y - rectangle.y,
            bitmap->GetPixelType(),
            bitmap_locker.ptrDataRoi,
            bitmap_locker.stride,
            static_cast<int>(bitmap->GetWidth()),
            static_cast<int>(bitmap->GetHeight()),
            static_cast<uint8_t*>(decode_info->brick_output_info->output_brick.data.get()) + static_cast<size_t>(z) * decode_info->brick_output_info->output_brick.info.stride_plane,
            decode_info->brick_output_info->output_brick.info.stride_line,
            static_cast<int>(decode_info->brick_output_info->output_brick.info.width),
            static_cast<int>(decode_info->brick_output_info->output_brick.info.height)
        });
}

bool CziBrickReader2::IsDone()
{
    return this->isDone_.load() && this->pending_tasks_count_.load() == 0;
}

void CziBrickReader2::WaitUntilDone()
{
    for (auto& worker_thread : this->reader_threads_)
    {
        worker_thread.join();
    }

    for (;;)
    {
        if (this->pending_tasks_count_.load() == 0)
        {
            break;
        }

        this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

BrickReaderStatistics CziBrickReader2::GetStatus()
{
    BrickReaderStatistics statistics;
    statistics.brick_data_delivered = this->statistics_brick_data_delivered.load();
    statistics.bricks_delivered = this->statistics_bricks_delivered.load();
    statistics.slices_read = this->statistics_slices_read.load();
    statistics.source_file_data_read = (this->input_stream_ ? this->input_stream_->GetTotalBytesRead() : 0);
    statistics.compressed_subblocks_in_flight = this->statistics_number_of_compressed_subblocks_in_flight_.load();
    statistics.uncompressed_planes_in_flight = this->statistics_number_of_uncompressed_planes_in_flight_.load();
    return statistics;
}
