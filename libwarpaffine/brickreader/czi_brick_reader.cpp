// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "czi_brick_reader.h"
#include <utility>
#include <limits>
#include <memory>

using namespace std;
using namespace libCZI;

class CMemBitmapFacade : public libCZI::IBitmapData
{
private:
    void* ptrData{ nullptr };
    libCZI::PixelType pixeltype;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    int lockCount{ 0 };
public:
    CMemBitmapFacade() = delete;

    CMemBitmapFacade(libCZI::PixelType pixeltype, std::uint32_t width, std::uint32_t height, uint32_t stride)
        : pixeltype(pixeltype), width(width), height(height), stride(stride)
    {
    }

    ~CMemBitmapFacade() override = default;

    libCZI::PixelType GetPixelType() const override
    {
        return this->pixeltype;
    }

    libCZI::IntSize	GetSize() const override
    {
        return libCZI::IntSize{ this->width, this->height };
    }

    libCZI::BitmapLockInfo	Lock() override
    {
        ++lockCount;
        libCZI::BitmapLockInfo bitmapLockInfo;
        bitmapLockInfo.ptrData = this->ptrData;
        bitmapLockInfo.ptrDataRoi = this->ptrData;
        bitmapLockInfo.stride = this->stride;
        bitmapLockInfo.size = static_cast<uint64_t>(this->stride) * this->height;
        return bitmapLockInfo;
    }

    void Unlock() override
    {
        if (lockCount > 0)
        {
            --lockCount;
        }
    }

    int GetLockCount() const override
    {
        return lockCount;
    }

    void SetPointer(void* ptr)
    {
        this->ptrData = ptr;
    }
};

// --------------------------------------------------------------------------------------

CziBrickReader::CziBrickReader(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream) : CziBrickReaderBase(context, reader)
{
    this->accessor_ = reader->CreateSingleChannelTileAccessor();
    this->input_stream_ = std::move(stream);
}

shared_ptr<libCZI::ICZIReader>& CziBrickReader::GetUnderlyingReader()
{
    return this->GetUnderlyingReaderBase();
}

BrickReaderStatistics CziBrickReader::GetStatus()
{
    BrickReaderStatistics statistics;
    statistics.brick_data_delivered = this->statistics_brick_data_delivered_.load();
    statistics.bricks_delivered = this->statistics_bricks_delivered_.load();
    statistics.slices_read = this->statistics_slices_read_.load();
    statistics.source_file_data_read = (this->input_stream_ ? this->input_stream_->GetTotalBytesRead() : 0);
    statistics.compressed_subblocks_in_flight = numeric_limits<uint64_t>::max();
    statistics.uncompressed_planes_in_flight = numeric_limits<uint64_t>::max();
    return statistics;
}

void CziBrickReader::StartPumping(
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

    for (int i = 0; i < kNumberOfReadingThreads; ++i)
    {
        this->reader_threads_.emplace_back([this] {this->ReadBrick(); });
    }
}

bool CziBrickReader::IsDone()
{
    return this->isDone_.load();
}

void CziBrickReader::WaitUntilDone()
{
    for (auto& reader_thread : this->reader_threads_)
    {
        reader_thread.join();
    }

    this->reader_threads_.clear();
}

/// This method is running on a worker-thread, and is responsible for reading a source-brick and
///  sending it downstream. Note that this method may run concurrently, so it may execute on
///  more than one thread.
void CziBrickReader::ReadBrick()
{
    try
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

            /*ostringstream ss;
            ss << "ReadBrick: " << Utils::DimCoordinateToString(&coordinate_of_brick);
            OutputDebugStringA(ss.str().c_str());*/

            Brick brick = this->CreateBrick(coordinate_of_brick, rectangle_of_brick);

            this->FillBrick(coordinate_of_brick, rectangle_of_brick, brick);

            if (this->deliver_brick_func_)
            {
                BrickCoordinateInfo brick_coordinate_info;
                brick_coordinate_info.coordinate = coordinate_of_brick;
                brick_coordinate_info.mIndex = tile_identifier.m_index.value_or(std::numeric_limits<int>::min());
                brick_coordinate_info.scene_index = tile_identifier.scene_index.value_or(std::numeric_limits<int>::min());
                brick_coordinate_info.x_position = rectangle_of_brick.x;
                brick_coordinate_info.y_position = rectangle_of_brick.y;
                this->deliver_brick_func_(
                    brick,
                    brick_coordinate_info);
            }

            this->statistics_bricks_delivered_.fetch_add(1);
            this->statistics_brick_data_delivered_.fetch_add(brick.info.GetBrickDataSize());
        }

        this->isDone_.store(true);
    }
    catch (exception& exception)
    {
        this->GetContextBase().FatalError(exception.what());
    }
}

/// Fill the specified brick with the content from the CZI-source-file.
///
/// \param          coordinate  The brick-coordinate. We require an valid C-coordinate, and a t-coordinate is optional. 
/// \param          rectangle   The region of the brick in the pixel-coordinate system.
/// \param [in,out] brick       The brick to be filled.
void CziBrickReader::FillBrick(const libCZI::CDimCoordinate& coordinate, const libCZI::IntRect& rectangle, Brick& brick)
{
    int c, t;
    if (!coordinate.TryGetPosition(DimensionIndex::C, &c))
    {
        ostringstream string_stream;
        string_stream << "CziBrickReader::FillBrick : invalid brick-coordinate encountered (" << Utils::DimCoordinateToString(&coordinate) << ")";
        throw invalid_argument(string_stream.str());
    }

    const bool t_valid = coordinate.TryGetPosition(DimensionIndex::T, &t);

    CDimCoordinate plane_coordinate;
    if (t_valid)
    {
        plane_coordinate.Set(DimensionIndex::T, t);
    }

    plane_coordinate.Set(DimensionIndex::C, c);

    CMemBitmapFacade memBitmap(brick.info.pixelType, brick.info.width, brick.info.height, brick.info.stride_line);
    for (uint32_t z = 0; z < brick.info.depth; ++z)
    {
        plane_coordinate.Set(DimensionIndex::Z, z);
        memBitmap.SetPointer(static_cast<uint8_t*>(brick.data.get()) + static_cast<size_t>(z) * brick.info.stride_plane);

        // TODO(JBL): we are using here a single-channel-tile-accessor, which means that we are **not** making
        //        sure that in case of overlapping tiles, we only choose the correct tiles.
        this->accessor_->Get(&memBitmap, rectangle.x, rectangle.y, &plane_coordinate, nullptr);

        this->statistics_slices_read_.fetch_add(1);
    }
}

Brick CziBrickReader::CreateBrick(const libCZI::CDimCoordinate& coordinate, const libCZI::IntRect& rectangle)
{
    int zCount;
    this->GetStatistics().dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &zCount);

    int c;
    coordinate.TryGetPosition(DimensionIndex::C, &c);

    Brick brick;
    brick.info.pixelType = GetPixelTypeForChannelNo(c);
    brick.info.width = rectangle.w;
    brick.info.height = rectangle.h;
    brick.info.depth = zCount;
    brick.info.stride_line = Utils::GetBytesPerPixel(brick.info.pixelType) * brick.info.width;
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;
    brick.data = shared_ptr<void>(malloc(static_cast<size_t>(brick.info.stride_plane) * brick.info.depth), free);

    return brick;
}
