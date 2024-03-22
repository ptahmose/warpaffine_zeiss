// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "czi_linear_brick_reader.h"
#include "linearreading_orderhelper.h"
#include <optional>
#include <map>
#include <utility>
#include <memory>
#include <limits>

using namespace std;
using namespace libCZI;

CziBrickReaderLinearReading::CziBrickReaderLinearReading(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream)
    :
    context_(context),
    reader_(std::move(reader)),
    input_stream_(std::move(stream)),
    brick_bucket_manager_([this](auto&& brick_result) { CziBrickReaderLinearReading::BrickCompleted(std::forward<decltype(brick_result)>(brick_result)); })
{
    this->statistics_ = this->reader_->GetStatistics();

    // create a map "channel-no <-> Pixeltype"
    int channelCount;
    if (!this->statistics_.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &channelCount))
    {
        throw invalid_argument("The document must have a C-dimension.");
    }

    for (int c = 0; c < channelCount; ++c)
    {
        SubBlockInfo subblock_info_of_channel;
        if (!this->reader_->TryGetSubBlockInfoOfArbitrarySubBlockInChannel(c, subblock_info_of_channel))
        {
            ostringstream string_stream;
            string_stream << "Unable to determine pixeltype for C=" << c << ".";
            throw invalid_argument(string_stream.str());
        }

        this->map_channelno_to_pixeltype_[c] = subblock_info_of_channel.pixelType;
    }

    this->max_size_of_subblocks_queued_ = 2ULL * 1024 * 1024 * 1024;    // 2GB

    auto map_number_of_slices_per_brick_coordinate = this->GenerateReadInfo();

    this->handle_high_watermark_callback_ = this->context_.GetAllocator().AddHighWatermarkCrossedCallback(
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

    int t_count, c_count;
    this->statistics_.dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count);
    this->statistics_.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count);
    this->brick_bucket_manager_.Setup(
        t_count,
        c_count,
        [&](int t, int c)->int
        {
            return map_number_of_slices_per_brick_coordinate[BrickCoordinate(t, c)];
        });
}

std::map<BrickCoordinate, std::uint32_t> CziBrickReaderLinearReading::GenerateReadInfo()
{
    int z_count, t_count, c_count;
    this->statistics_.dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &z_count);
    this->statistics_.dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count);
    this->statistics_.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count);

    LinearReadingOrderHelper::ReadingConstraints reading_constraints;
    reading_constraints.max_number_of_subblocks_inflight = this->context_.GetCommandLineOptions().GetPropertyBagForBrickSource().GetInt32OrDefault(
        ICziBrickReader::kPropertyBagKey_LinearReader_max_number_of_subblocks_to_wait_for,
        2000);

    auto subblocks_ordered = LinearReadingOrderHelper::DetermineOrder(this->reader_.get(), reading_constraints);

    Utilities::ExecuteIfVerbosityAboveOrEqual(
        this->context_.GetCommandLineOptions().GetPrintOutVerbosity(),
        MessagesPrintVerbosity::kMinimal,
        [&]()->void
        {
            ostringstream stream;
            stream << "linearreading: the suggested limit for the number of subblocks-in-flight was " << reading_constraints.max_number_of_subblocks_inflight << ", " << endl;
            stream << "               the actual \"max number of subblocks-in-flight\" is " << subblocks_ordered.max_number_of_subblocks_inflight << "." << endl;
            this->context_.GetLog()->WriteLineStdOut(stream.str());
        });

    this->subblocks_ordered_ = std::move(subblocks_ordered.reading_order);

    return subblocks_ordered.number_of_slices_per_brick;
}

/*virtual*/void CziBrickReaderLinearReading::StartPumping(
    const std::function<void(const Brick&, const BrickCoordinateInfo&)>& deliver_brick_func)
{
    const int numberOfReadingThreads = this->context_.GetCommandLineOptions().GetNumberOfReaderThreads();

    this->deliver_brick_func_ = deliver_brick_func;
    this->isDone_.store(false);

    for (int i = 0; i < numberOfReadingThreads; ++i)
    {
        this->reader_threads_.emplace_back([this] {this->ReadSubblocksThread(); });
    }
}

/*virtual*/bool CziBrickReaderLinearReading::IsDone()
{
    bool b =  this->isDone_.load() && this->pending_tasks_count_.load() == 0 && this->statistics_number_of_uncompressed_planes_in_flight_.load() == 0 && this->statistics_number_of_compressed_subblocks_in_flight_.load() == 0;
    if (b)
    {
        this->context_.WriteDebugString("CziBrickReaderLinearReading::IsDone");
        //OutputDebugStringA("CziBrickReaderLinearReading::IsDone");
    }

    return b;
}

/*virtual*/BrickReaderStatistics CziBrickReaderLinearReading::GetStatus()
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

/*virtual*/void CziBrickReaderLinearReading::WaitUntilDone()
{
    for (auto& worker_thread : this->reader_threads_)
    {
        worker_thread.join();
    }

    for (;;)
    {
        if (this->pending_tasks_count_.load() == 0 && this->statistics_number_of_uncompressed_planes_in_flight_.load() == 0 && this->statistics_number_of_compressed_subblocks_in_flight_.load() == 0)
        {
            break;
        }

        this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

/*virtual*/std::shared_ptr<libCZI::ICZIReader>& CziBrickReaderLinearReading::GetUnderlyingReader()
{
    return this->reader_;
}

void CziBrickReaderLinearReading::ReadSubblocksThread()
{
    for (;;)
    {
        const int index = this->next_subblock_index_to_read_++;
        if (index >= this->subblocks_ordered_.size())
        {
            break;
        }

        int subblockIndex = this->subblocks_ordered_[index];

        auto subblock = this->reader_->ReadSubBlock(subblockIndex);
        ++this->statistics_slices_read;

        ostringstream ss;
        ss << "ReadSubblocksThread: subblock read: " << Utils::DimCoordinateToString(&subblock->GetSubBlockInfo().coordinate);
        this->context_.WriteDebugString(ss.str().c_str());
        //OutputDebugStringA(ss.str().c_str());

        if (this->context_.GetCommandLineOptions().GetTestStopPipelineAfter() != TestStopPipelineAfter::kReadFromSource)
        {
            this->memory_used_by_subblocks_in_queue_.fetch_add(DetermineMemorySizeOfSubblock(subblock.get()));
            ++this->pending_tasks_count_;
            ++this->statistics_number_of_compressed_subblocks_in_flight_;
            this->context_.GetTaskArena()->AddTask(
                TaskType::DecompressSlice,
                [this, subblock]()->void
                {
                    this->DecompressTask(subblock);
                    --this->pending_tasks_count_;
                    --this->statistics_number_of_compressed_subblocks_in_flight_;
                });
        }

        for (;;)
        {
            if (this->memory_used_by_subblocks_in_queue_.load() <= this->max_size_of_subblocks_queued_)
            {
                this->isThrottledInternally_.store(false);
            }
            else
            {
                this->isThrottledInternally_.store(true);
            }

            if (!this->isPaused_.load() && !this->isThrottledInternally_.load())
            {
                break;
            }

            this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    this->isDone_.store(true);
}

void CziBrickReaderLinearReading::DecompressTask(const std::shared_ptr<libCZI::ISubBlock>& subblock)
{
    // ok, so now... decompress the subblock and forward it
    auto bitmap = subblock->CreateBitmap();

    int z_coordinate, t_coordinate, c_coordinate;
    subblock->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::T, &t_coordinate);
    subblock->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
    subblock->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::Z, &z_coordinate);

    if (this->context_.GetCommandLineOptions().GetTestStopPipelineAfter() != TestStopPipelineAfter::kDecompress)
    {
        // ...and one uncompressed subblock more around (and we only increment our counter
        //    if we actually "keep" this subblock)
        ++this->statistics_number_of_uncompressed_planes_in_flight_;     
        BrickBucketManager::SliceInfo slice_info;
        slice_info.bitmap = std::move(bitmap);
        slice_info.x_position = subblock->GetSubBlockInfo().logicalRect.x;
        slice_info.y_position = subblock->GetSubBlockInfo().logicalRect.y;
        slice_info.t_coordinate = t_coordinate;
        slice_info.z_coordinate = z_coordinate;
        slice_info.c_coordinate = c_coordinate;
        this->brick_bucket_manager_.AddSlice(slice_info);
    }

    auto size_of_subblock = DetermineMemorySizeOfSubblock(subblock.get());
    this->memory_used_by_subblocks_in_queue_.fetch_sub(size_of_subblock);
}

void CziBrickReaderLinearReading::BrickCompleted(const std::shared_ptr<IBrickResult>& brick_result)
{
    // now, start a task which will copy all planes into a brick
    ++this->pending_tasks_count_;

    //ostringstream ss;
    //ss << "BrickCompleted: T=" << brick_result->GetCoordinate(DimensionIndex::T) << " C=" << brick_result->GetCoordinate(DimensionIndex::C);
    //OutputDebugStringA(ss.str().c_str());

    this->context_.GetTaskArena()->AddTask(
        TaskType::BrickComposition,
        [=]()->void
        {
            this->ComposeBrickTask(brick_result);
            --this->pending_tasks_count_;
        });
}

void CziBrickReaderLinearReading::ComposeBrickTask(const std::shared_ptr<IBrickResult>& brick_result)
{
    const int c_coordinate = brick_result->GetCoordinate(DimensionIndex::C);
    const CDimCoordinate dim_coordinate
    {
        { DimensionIndex::C, c_coordinate },
        { DimensionIndex::T, brick_result->GetCoordinate(DimensionIndex::T) },
    };

    int number_of_slices;
    this->statistics_.dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &number_of_slices);

    const auto brick = brick_result->ComposeBrick(
        this->map_channelno_to_pixeltype_[c_coordinate],
        this->statistics_.boundingBoxLayer0Only.x,
        this->statistics_.boundingBoxLayer0Only.y,
        this->statistics_.boundingBoxLayer0Only.w,
        this->statistics_.boundingBoxLayer0Only.h,
        number_of_slices,
        this->context_.GetAllocator(),
        true);

    // and, finally, deliver the brick
    BrickCoordinateInfo brick_coordinate_info;
    brick_coordinate_info.coordinate = dim_coordinate;
    brick_coordinate_info.mIndex = numeric_limits<int>::max();      
    brick_coordinate_info.x_position = 0;
    brick_coordinate_info.y_position = 0;
    this->deliver_brick_func_(brick, brick_coordinate_info);
    ++this->statistics_bricks_delivered;
    this->statistics_brick_data_delivered.fetch_add(brick.info.GetBrickDataSize());

    // the uncompressed planes (which are held inside the brick_result object) are not needed any more,
    //  and will be released when this method returns
    this->statistics_number_of_uncompressed_planes_in_flight_.fetch_sub(brick_result->GetNumberOfSlices());
}

/*static*/std::uint64_t CziBrickReaderLinearReading::DetermineMemorySizeOfSubblock(libCZI::ISubBlock* subblock)
{
    size_t sizeData, sizeAttachment;
    const void* dummy;
    subblock->DangerousGetRawData(ISubBlock::Data, dummy, sizeData);
    subblock->DangerousGetRawData(ISubBlock::Attachment, dummy, sizeAttachment);
    return sizeData + sizeAttachment;
}
