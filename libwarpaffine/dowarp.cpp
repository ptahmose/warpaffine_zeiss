// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "dowarp.h"
#include <functional> 
#include <iostream>
#include <utility>
#include <thread>
#include <vector>
#include <limits>
#include <memory>
#include <algorithm>

#include "inc_libCZI.h"
#include "deskew_helpers.h"

using namespace std;
using namespace libCZI;

class MemoryBlockWrapper : public libCZI::IMemoryBlock
{
private:
    std::shared_ptr<void> data_;
    size_t size_;
public:
    MemoryBlockWrapper(std::shared_ptr<void> data, size_t size)
        : data_(std::move(data)), size_(size)
    {
    }

    void* GetPtr() override
    {
        return this->data_.get();
    }

    size_t GetSizeOfData() const override
    {
        return this->size_;
    }
};

DoWarp::OutputBrickInfoRepository::OutputBrickInfoRepository(const AppContext& context, const DeskewDocumentInfo& document_info, const Eigen::Matrix4d& transformation_matrix)
{
    // Here we create a tiling of the output brick - in other words, we ensure that the output-tiles are of a size
    // with the specified maximum of pixels in x and y. We create a list of "tiled output bricks", and we determine
    // the m- and the scene-index of those tiles. Care must be taken here in order to ensure that the m-index is
    // counting "per scene" and that the m-index must be unique within a scene. 
    const uint32_t max_extent = context.GetCommandLineOptions().GetMaxOutputTileExtent();

    uint32_t total_number_of_output_subblocks = 0;

    // we use this map to keep track of "counting m per scene". Key is the scene-index, and m is the next
    //  m-index we want to use for the scene. In case of "no scene-index present", we use numeric_limits<int>::min()
    //  as the key.
    map<int, int> scene_index_to_m_index;

    for (const auto& item : document_info.map_brickid_position)
    { 
        Eigen::Vector3d edge_point;
        Eigen::Vector3d extent;
        DeskewHelpers::CalculateAxisAlignedBoundingBox(item.second.width, item.second.height, document_info.depth, transformation_matrix, edge_point, extent);
        DestinationBrickInfo destination_brick_info;
        destination_brick_info.cuboid.x_position = 0;
        destination_brick_info.cuboid.y_position = 0;
        destination_brick_info.cuboid.z_position = 0;
        destination_brick_info.cuboid.width = static_cast<uint32_t>(round(extent(0)));
        destination_brick_info.cuboid.height = static_cast<uint32_t>(round(extent(1)));
        destination_brick_info.cuboid.depth = static_cast<uint32_t>(round(extent(2)));
        auto tiling = Create2dTiling(max_extent, IntRect{ 0, 0, static_cast<int>(destination_brick_info.cuboid.width), static_cast<int>(destination_brick_info.cuboid.height) });

        optional<int> s_index;
        if (item.first.IsSIndexValid())
        {
            s_index = item.first.s_index;
        }

        for (const auto& tile : tiling)
        {
            TilingRectAndMandSceneIndex tiling_rect_and_mindex{};
            tiling_rect_and_mindex.s_index = s_index.value_or(numeric_limits<int>::min());
            
            // this will nicely either read and increment an existing m-index from the map, or insert a new one with value zero
            tiling_rect_and_mindex.m_index = scene_index_to_m_index[tiling_rect_and_mindex.s_index]++;
            tiling_rect_and_mindex.rectangle = tile;
            destination_brick_info.tiling.push_back(tiling_rect_and_mindex);
        }

        this->map_brickid_destinationbrickinfo_.insert(pair<BrickInPlaneIdentifier, DestinationBrickInfo>(item.first, destination_brick_info));
        total_number_of_output_subblocks += tiling.size() * destination_brick_info.cuboid.depth;
    }

    this->number_of_subblocks_to_output_ = total_number_of_output_subblocks;
}

IntSize3 DoWarp::OutputBrickInfoRepository::GetOutputExtent(const BrickInPlaneIdentifier& brick_identifier) const
{
    const auto& item = this->map_brickid_destinationbrickinfo_.at(brick_identifier);
    IntSize3 extent;
    extent.width = item.cuboid.width;
    extent.height = item.cuboid.height;
    extent.depth = item.cuboid.depth;
    return extent;
}

const DoWarp::OutputBrickInfoRepository::DestinationBrickInfo& DoWarp::OutputBrickInfoRepository::GetDestinationInfo(const BrickInPlaneIdentifier& brick_identifier) const
{
    return this->map_brickid_destinationbrickinfo_.at(brick_identifier);
}

IntCuboid DoWarp::OutputBrickInfoRepository::GetOutputVolume(const BrickInPlaneIdentifier& brick_identifier) const
{
    const auto& item = this->map_brickid_destinationbrickinfo_.at(brick_identifier);
    return item.cuboid;
}

/*static*/std::vector<IntRect> DoWarp::OutputBrickInfoRepository::Create2dTiling(std::uint32_t max_extent, const IntRect& rectangle)
{
    const uint32_t rows_count = (rectangle.h + max_extent - 1) / max_extent;
    const uint32_t columns_count = (rectangle.w + max_extent - 1) / max_extent;
    std::vector<IntRect> tiling_result;
    tiling_result.reserve(static_cast<size_t>(columns_count) * rows_count);
    for (uint32_t rows = 0; rows < rows_count; ++rows)
    {
        for (uint32_t columns = 0; columns < columns_count; ++columns)
        {
            tiling_result.emplace_back(
                IntRect
                {
                    rectangle.x + static_cast<int>(columns * max_extent),
                    rectangle.y + static_cast<int>(rows * max_extent),
                    static_cast<int>(min(max_extent, rectangle.w - (columns * max_extent))),
                    static_cast<int>(min(max_extent, rectangle.h - (rows * max_extent)))
                });
        }
    }

    return tiling_result;
}

std::uint32_t DoWarp::OutputBrickInfoRepository::GetTotalNumberOfSubblocksToOutput() const
{
    return this->number_of_subblocks_to_output_;
}

//-----------------------------------------------------------------------------

DoWarp::DoWarp(
    AppContext& context,
    std::uint32_t number_of_3dplanes_to_process,
    const DeskewDocumentInfo& document_info,
    const Eigen::Matrix4d& transformation_matrix,
    std::shared_ptr<ICziBrickReader> brick_reader,
    std::shared_ptr<ICziSlicesWriter> writer,
    std::shared_ptr<IWarpAffine> warp_affine_engine) :
    context_(context),
    transformation_matrix_(transformation_matrix),
    document_info_(document_info),
    writer_(std::move(writer)),
    brick_reader_(std::move(brick_reader)),
    warp_affine_engine_(std::move(warp_affine_engine)),
    output_brick_info_repository_(context, document_info, transformation_matrix)
{
    // Note: 
    // We only deal with the width/height/depth of the output-volume, not its "edge-point"; in other words,
    // we assume that the output-volume is at (0,0,0). This is currently ensure by the preparation of the transformation
    // matrix, but we should extend this in order to deal with "edge point not at the origin".
    Eigen::Vector3d edge_point;
    Eigen::Vector3d extent;
    DeskewHelpers::CalculateAxisAlignedBoundingBox(document_info.width, document_info.height, document_info.depth, transformation_matrix, edge_point, extent);
    this->output_width_ = static_cast<uint32_t>(round(extent(0)));
    this->output_height_ = static_cast<uint32_t>(round(extent(1)));
    this->output_depth_ = static_cast<uint32_t>(round(extent(2)));

    this->total_number_of_subblocks_to_output = this->output_brick_info_repository_.GetTotalNumberOfSubblocksToOutput() * number_of_3dplanes_to_process;

    if (context.GetCommandLineOptions().GetDoCalculateHashOfOutputData())
    {
        this->calculate_result_hash_ = std::make_unique<CalcResultHash>();
    }

    this->context_.GetAllocator().AddDestinationBrickMemoryReleasedCallback(
        [this]()->void
        {
            this->DestinationMemoryReleased();
        });
}

std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> DoWarp::GetOutputExtent() const
{
    return make_tuple(this->output_width_, this->output_height_, this->output_depth_);
}

IntCuboid DoWarp::GetOutputVolume(const BrickInPlaneIdentifier& brick_identifier) const
{
    return this->output_brick_info_repository_.GetOutputVolume(brick_identifier);
}

IntSize3 DoWarp::GetLargestOutputExtentIncludingTiling(const BrickInPlaneIdentifier& brick_identifier) const
{
    auto output_volume = this->output_brick_info_repository_.GetOutputVolume(brick_identifier);
    const uint32_t max_extent = this->context_.GetCommandLineOptions().GetMaxOutputTileExtent();

    // the output-volume gets tiled with "max_extent" pixels at most for width/height
    return IntSize3{ min(output_volume.width, max_extent), min(output_volume.height, max_extent), output_volume.depth };
}

const Eigen::Matrix4d& DoWarp::GetTransformationMatrix() const
{
    return this->transformation_matrix_;
}

WarpStatistics DoWarp::GetStatistics()
{
    WarpStatistics statistics;
    const auto reader_statistics = this->brick_reader_->GetStatus();

    statistics.source_brick_data_delivered = reader_statistics.brick_data_delivered;
    statistics.source_bricks_delivered = reader_statistics.bricks_delivered;
    statistics.source_slices_read = reader_statistics.slices_read;

    const auto now = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> elapsed_seconds = now - this->time_point_operation_started_;

    statistics.elapsed_time_since_start_in_seconds = elapsed_seconds.count();

    statistics.source_brick_data_delivered_per_second = reader_statistics.brick_data_delivered / elapsed_seconds.count();
    statistics.source_bricks_delivered_per_minute = reader_statistics.bricks_delivered * 60 / elapsed_seconds.count();
    statistics.source_slices_read_per_second = reader_statistics.slices_read / elapsed_seconds.count();
    statistics.bytes_read_from_source_file = reader_statistics.source_file_data_read;
    statistics.datarate_read_from_source_file = reader_statistics.source_file_data_read / elapsed_seconds.count();
    statistics.brickreader_compressed_subblocks_in_flight = reader_statistics.compressed_subblocks_in_flight;
    statistics.brickreader_uncompressed_planes_in_flight = reader_statistics.uncompressed_planes_in_flight;

    statistics.warp_tasks_in_flight = this->warp_tasks_in_flight_.load();
    statistics.compression_tasks_in_flight = this->compression_tasks_in_flight_.load();
    statistics.write_slices_queue_length = this->writer_->GetNumberOfPendingSliceWriteOperations();
    statistics.reader_throttled = this->brick_reader_->GetIsThrottledState();

    const auto task_arena_statistics_ = this->context_.GetTaskArena()->GetStatistics();
    statistics.task_arena_queue_length = task_arena_statistics_.queue_length;
    statistics.currently_active_tasks = task_arena_statistics_.active_tasks;
    statistics.currently_suspended_tasks = task_arena_statistics_.suspended_tasks;
    this->context_.GetAllocator().GetState(statistics.memory_status);
    statistics.subblocks_added_to_writer = this->number_of_subblocks_added_to_writer_.load();
    statistics.total_progress_percent = this->CalculateTotalProgress();

    return statistics;
}

void DoWarp::DoOperation()
{
    this->time_point_operation_started_ = std::chrono::high_resolution_clock::now();

    this->brick_reader_->StartPumping(
        [this](const Brick& brick, const BrickCoordinateInfo& coordinate)->void
        {
            this->InputBrick(brick, coordinate);
        });
}

bool DoWarp::IsDone()
{
    return
        this->brick_reader_->IsDone() &&
        this->GetTotalNumberOfTasksInFlight() == 0;
}

void DoWarp::WaitUntilDone()
{
    this->brick_reader_->WaitUntilDone();

    for (;;)
    {
        if (this->GetTotalNumberOfTasksInFlight() == 0)
        {
            break;
        }

        this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool DoWarp::TryGetHash(std::array<uint8_t, 16>* hash_code) const
{
    if (this->calculate_result_hash_)
    {
        if (hash_code != nullptr)
        {
            *hash_code = this->calculate_result_hash_->GetHash();
        }

        return true;
    }

    return false;
}

void DoWarp::InputBrick(const Brick& brick, const BrickCoordinateInfo& coordinate_info)
{
    BrickInPlaneIdentifier brick_in_plane_identifier;
    brick_in_plane_identifier.m_index = coordinate_info.mIndex;
    brick_in_plane_identifier.s_index = coordinate_info.scene_index;

    // TODO(JBL): in the unfortunate case where our bookkeeping is not correct (e.g. we have a brick which is not in our map), we currently 
    //             would throw an exception and crash. We should handle this more gracefully, at least by logging an error message.
    const auto& destination_brick_info = this->output_brick_info_repository_.GetDestinationInfo(brick_in_plane_identifier);

    for (size_t n = 0; n < destination_brick_info.tiling.size(); n++)
    {
        auto destination_brick = this->CreateBrickAndWaitUntilAvailable(
            brick.info.pixelType,
            destination_brick_info.tiling[n].rectangle.w,
            destination_brick_info.tiling[n].rectangle.h,
            destination_brick_info.cuboid.depth);

        this->IncWarpTasksInFlight();
        this->context_.GetTaskArena()->AddTask(
            TaskType::WarpAffineBrick,
            [
                this,
                    brick_captured = brick,
                    coordinate_info_captured = coordinate_info,
                    tile_captured = destination_brick_info.tiling[n],
                    source_cuboid_depth = destination_brick_info.cuboid.depth,
                    destination_brick_captured = destination_brick
            ]()->void
            {
                this->ProcessBrickCommon2(brick_captured, destination_brick_captured, coordinate_info_captured, source_cuboid_depth, tile_captured/*tile_captured.rectangle, tile_captured.m_index*/);
                this->DecWarpTasksInFlight();
            });
    }
}

void DoWarp::ProcessBrickCommon2(const Brick& brick, const Brick& destination_brick, const BrickCoordinateInfo& coordinate_info, uint32_t source_depth, const OutputBrickInfoRepository::TilingRectAndMandSceneIndex& rect_and_tile_identifier/*const IntRect& roi, int m_index*/)
{
    this->warp_affine_engine_->Execute(
        this->transformation_matrix_,
        IntPos3{rect_and_tile_identifier.rectangle.x, rect_and_tile_identifier.rectangle.y, 0},
        this->context_.GetCommandLineOptions().GetInterpolationMode(),
        brick,
        destination_brick);

    // ok, what we now do is - add a task for every slice, in order to 
    //  parallelize the compression
    for (uint32_t z = 0; z < destination_brick.info.depth; ++z)
    {
        auto slice_to_compress_task_info = new OutputSliceToCompressTaskInfo{ destination_brick, static_cast<int>(z) };
        this->IncCompressionTasksInFlight();
        this->context_.GetTaskArena()->AddTask(
            TaskType::CompressSlice,
            [=]()->void
            {
                Eigen::Vector4d p;
                p <<
                    coordinate_info.x_position - this->document_info_.document_origin_x,
                    coordinate_info.y_position - this->document_info_.document_origin_y,
                    0,
                    1;
                const auto xy_transformed = this->GetTransformationMatrix() * p;

                /*ostringstream ss;
                ss << "roi=" << roi.x << ", " << roi.y << "; coordinate_info=" << coordinate_info.x_position << ", " << coordinate_info.y_position << "; transformed: " << lround(xy_transformed[0]) << ", " << lround(xy_transformed[1]) << " => " << roi.x + lround(xy_transformed[0]) << ", " << roi.y + lround(xy_transformed[1]) << ".\n";
                this->context_.WriteDebugString(ss.str().c_str());*/

                libCZI::CDimCoordinate coord = coordinate_info.coordinate;
                coord.Set(DimensionIndex::Z, z);
                SubblockXYM xym;
                xym.x_position = rect_and_tile_identifier.rectangle.x + lround(xy_transformed[0]);
                xym.y_position = rect_and_tile_identifier.rectangle.y + lround(xy_transformed[1]);

                // TODO(JBL): we better should use optional for this, not magic values
                if (Utils::IsValidMindex(rect_and_tile_identifier.m_index))
                {
                    xym.m_index = rect_and_tile_identifier.m_index;
                }

                if (Utils::IsValidMindex(rect_and_tile_identifier.s_index))
                {
                    xym.scene_index = coordinate_info.scene_index;
                }

                this->ProcessOutputSlice(slice_to_compress_task_info, coord, xym);
                this->DecCompressionTasksInFlight();
            });
    }
}

void DoWarp::ProcessOutputSlice(OutputSliceToCompressTaskInfo* output_slice_task_info, const libCZI::CDimCoordinate& coordinate, const SubblockXYM& xym)
{
    auto compression_mode_and_memblk = this->Compress(*output_slice_task_info);

    if (this->calculate_result_hash_)
    {
        this->calculate_result_hash_->AddSlice(get<1>(compression_mode_and_memblk), coordinate);
    }

    ICziSlicesWriter::AddSliceInfo add_slice_info;
    add_slice_info.subblock_raw_data = get<1>(compression_mode_and_memblk);
    add_slice_info.compression_mode = get<0>(compression_mode_and_memblk);
    add_slice_info.pixeltype = output_slice_task_info->brick.info.pixelType;
    add_slice_info.width = output_slice_task_info->brick.info.width;
    add_slice_info.height = output_slice_task_info->brick.info.height;
    add_slice_info.coordinate = coordinate;
    add_slice_info.m_index = xym.m_index;
    add_slice_info.scene_index = xym.scene_index;
    add_slice_info.x_position = xym.x_position;
    add_slice_info.y_position = xym.y_position;
    this->writer_->AddSlice(add_slice_info);
    ++this->number_of_subblocks_added_to_writer_;

    delete output_slice_task_info;
}

void DoWarp::IncWarpTasksInFlight()
{
    ++this->warp_tasks_in_flight_;
    ++this->total_tasks_in_flight_;
}

void DoWarp::DecWarpTasksInFlight()
{
    --this->warp_tasks_in_flight_;
    --this->total_tasks_in_flight_;
}

void DoWarp::IncCompressionTasksInFlight()
{
    ++this->compression_tasks_in_flight_;
    ++this->total_tasks_in_flight_;
}

void DoWarp::DecCompressionTasksInFlight()
{
    --this->compression_tasks_in_flight_;
    --this->total_tasks_in_flight_;
}

std::uint32_t DoWarp::GetTotalNumberOfTasksInFlight()
{
    return total_tasks_in_flight_.load();
}

bool DoWarp::IsSubTilingRequired(const IntSize3& size)
{
    const auto max_extent = this->context_.GetCommandLineOptions().GetMaxOutputTileExtent();
    if (size.width > max_extent || size.height > max_extent)
    {
        return true;
    }

    return false;
}

std::vector<IntRect> DoWarp::Create2dTiling(const IntRect& rectangle)
{
    const auto max_extent = this->context_.GetCommandLineOptions().GetMaxOutputTileExtent();
    const uint32_t rows_count = (rectangle.h + max_extent - 1) / max_extent;
    const uint32_t columns_count = (rectangle.w + max_extent - 1) / max_extent;
    std::vector<IntRect> tiling_result;
    tiling_result.reserve(static_cast<size_t>(columns_count) * rows_count);
    for (uint32_t rows = 0; rows < rows_count; ++rows)
    {
        for (uint32_t columns = 0; columns < columns_count; ++columns)
        {
            tiling_result.emplace_back(
                IntRect
                {
                    rectangle.x + static_cast<int>(columns * max_extent),
                    rectangle.y + static_cast<int>(rows * max_extent),
                    static_cast<int>(min(max_extent, rectangle.w - (columns * max_extent))),
                    static_cast<int>(min(max_extent, rectangle.h - (rows * max_extent)))
                });
        }
    }

    return tiling_result;
}

Brick DoWarp::CreateBrick(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth)
{
    Brick brick;
    brick.info.pixelType = pixel_type;
    brick.info.width = width;
    brick.info.height = height;
    brick.info.depth = depth;
    brick.info.stride_line = width * Utils::GetBytesPerPixel(pixel_type);
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;
    const uint64_t size_of_brick = brick.info.stride_plane * static_cast<uint64_t>(brick.info.depth);
    brick.data = this->context_.GetAllocator().Allocate(BrickAllocator::MemoryType::DestinationBrick, size_of_brick);
    return brick;
}

Brick DoWarp::CreateBrickAndWaitUntilAvailable(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth)
{
    Brick brick;
    brick.info.pixelType = pixel_type;
    brick.info.width = width;
    brick.info.height = height;
    brick.info.depth = depth;
    brick.info.stride_line = width * Utils::GetBytesPerPixel(pixel_type);
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;
    const uint64_t size_of_brick = brick.info.stride_plane * static_cast<uint64_t>(brick.info.depth);
    for (;;)
    {
        if (this->above_high_water_mask2_.load() == false)
        {
            brick.data = this->context_.GetAllocator().Allocate(BrickAllocator::MemoryType::DestinationBrick, size_of_brick, false);
            if (brick.data)
            {
                break;
            }
        }

        this->context_.WriteDebugString("Waiting for Destination-brick allocation\n");
        this->context_.GetTaskArena()->SuspendCurrentTask([this](void* handle)->void {this->AddResumeHandle(handle); });
        this->context_.WriteDebugString("*** Was resumed ***\n");
    }

    return brick;
}

std::tuple<libCZI::CompressionMode, std::shared_ptr<libCZI::IMemoryBlock>> DoWarp::Compress(const OutputSliceToCompressTaskInfo& output_slice_task_info)
{
    size_t(*pfn_calc_compression_size)(uint32_t, uint32_t, PixelType);
    bool (*pfn_compress)(uint32_t, uint32_t, uint32_t, PixelType, const void*, void*, size_t&, const ICompressParameters*);

    const auto& compression_options = this->context_.GetCommandLineOptions().GetCompressionOptions();
    switch (compression_options.first)
    {
    case CompressionMode::Zstd0:
        pfn_calc_compression_size = ZstdCompress::CalculateMaxCompressedSizeZStd0;
        pfn_compress = ZstdCompress::CompressZStd0;
        break;
    case CompressionMode::Zstd1:
        pfn_calc_compression_size = ZstdCompress::CalculateMaxCompressedSizeZStd1;
        pfn_compress = ZstdCompress::CompressZStd1;
        break;
    default:
        throw logic_error("An unsupported compression-mode was specified.");
    }

    const size_t max_size_necessary = pfn_calc_compression_size(
        output_slice_task_info.brick.info.width,
        output_slice_task_info.brick.info.height,
        output_slice_task_info.brick.info.pixelType);

    // As an effort to save some memory, we try the compression with a destination buffer size of half the max possible size, assuming
    //  that in "almost all cases" this is sufficiently large (or, we assume a compression rate of ~50%). If this turns out to be not 
    //  sufficient, we do one more compression where we allocate the "max possible amount of memory". So, the second attempt should in
    //  all cases succeed. Of course, we could think of a more elaborate scheme here, e.g. keeping track of the compression ratio and
    //  start with a buffer size determined by the compression ratio so far.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        size_t size = (attempt == 0) ? (max_size_necessary / 2) : max_size_necessary;
        auto allocated_memory = this->context_.GetAllocator().Allocate(BrickAllocator::MemoryType::CompressedDestinationSlice, size);

        size_t actual_size = size;
        bool b = pfn_compress(
            output_slice_task_info.brick.info.width,
            output_slice_task_info.brick.info.height,
            output_slice_task_info.brick.info.stride_line,
            output_slice_task_info.brick.info.pixelType,
            static_cast<uint8_t*>(output_slice_task_info.brick.data.get()) + static_cast<size_t>(output_slice_task_info.z_slice) * output_slice_task_info.brick.info.stride_plane,
            allocated_memory.get(),
            actual_size,
            compression_options.second.get());

        if (b == true)
        {
            return make_tuple(compression_options.first, make_shared<MemoryBlockWrapper>(allocated_memory, actual_size));
        }
    }

    ostringstream error_text;
    error_text << "We should not be able to get here, compression failed on second attempt (size=" << max_size_necessary << ").";
    throw logic_error(error_text.str());
}

void DoWarp::DestinationMemoryReleased()
{
    this->DoResume();
}

void DoWarp::AddResumeHandle(ITaskArena::SuspendHandle handle)
{
    std::lock_guard<std::mutex> lck(this->mutex_resume_handles_);
    this->resume_handles_.push_back(handle);
}

void DoWarp::DoResume()
{
    std::lock_guard<std::mutex> lck(this->mutex_resume_handles_);
    for (const auto handle : this->resume_handles_)
    {
        this->context_.GetTaskArena()->ResumeTask(handle);
    }

    this->resume_handles_.clear();
}

float DoWarp::CalculateTotalProgress()
{
    auto expected_total_number = this->total_number_of_subblocks_to_output;
    if (expected_total_number > 0)
    {
        float progress = 100.f * static_cast<float>(this->number_of_subblocks_added_to_writer_.load()) / this->total_number_of_subblocks_to_output;
        return progress;
    }

    return numeric_limits<float>::quiet_NaN();
}
