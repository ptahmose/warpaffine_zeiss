// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "brick_bucket_manager.h"
#include <utility>
#include <limits>
#include <memory>
#include "../BrickAllocator.h"
#include "../utilities.h"

using namespace std;

// ---------------------------------------------------------------------------------------------

BrickBucketManager::BrickBucketManager(std::function<void(const std::shared_ptr<IBrickResult>&)> functor_brick_done)
    : functor_brick_done_(std::move(functor_brick_done))
{
}

void BrickBucketManager::Setup(std::uint32_t t_count, std::uint32_t c_count, const std::function<int(int t, int c)>& functor_get_number_of_slices)
{
    for (int t = 0; t < t_count; ++t)
    {
        for (int c = 0; c < c_count; ++c)
        {
            BucketData* bucket_data = new BucketData(functor_get_number_of_slices(t, c));
            this->brickcoordinate_buckets_map_.insert({ BrickCoordinate{ t, c }, bucket_data });
        }
    }
}

void BrickBucketManager::AddSlice(const SliceInfo& slice_info)
{
    BrickCoordinate brick_coordinate(slice_info.t_coordinate, slice_info.c_coordinate);

    auto& value = this->brickcoordinate_buckets_map_.at(brick_coordinate);

    if (this->AddToBrick(value, slice_info.x_position, slice_info.y_position, slice_info.z_coordinate, slice_info.bitmap))
    {
        const auto brick_result_on_slice_info = make_shared<BrickResultOnSliceInfo>(brick_coordinate.t, brick_coordinate.c, value);

        // Now we set the "value" in the map to null - the object is now owned by the "brick_result_on_slice_info"-object.
        // Important is that we do not do any write-access to the map itself, so we leave the key itself in the map (instead
        // of removing it here). Removing the element would require locking, and I'd think that the benefit we'd get from
        // removing the item (i.e. faster look-up) would not outweigh the benefit of having no necessity for locking.
        value = nullptr;

        this->functor_brick_done_(brick_result_on_slice_info);
    }
}

bool BrickBucketManager::AddToBrick(BucketData* bucket_data, int x, int y, int z_coordinate, std::shared_ptr<libCZI::IBitmapData> bitmap)
{
    // we atomically increment the "next_index"-field - so that (even when executing concurrently)
    //  the plane is inserted into an empty slot
    const auto index_to_use = bucket_data->next_index_for_plane++;

    // put the plane into its own slot - note that there is a guarantee of a particular
    //  order (of the planes)
    bucket_data->items->at(index_to_use) = PlaneAndIndexZ{ std::move(bitmap), x, y, z_coordinate };

    // This is now important - *after* we placed the data, we increment the "number_of_planes_ready"-counter (atomically),
    //  and only if we did the last increment (ie. if the number here is the expected number), then we report that we are
    //  done.
    return ++bucket_data->number_of_planes_ready == static_cast<uint32_t>(bucket_data->items->size());
}

// ---------------------------------------------------------------------------------------------
BrickBucketManager::BrickResultOnSliceInfo::BrickResultOnSliceInfo(int t_coordinate, int c_coordinate, BrickBucketManager::BucketData* bucket_data) :
    bucket_data_(bucket_data),
    t_coordinate_(t_coordinate),
    c_coordinate_(c_coordinate)
{
}

/*virtual*/int BrickBucketManager::BrickResultOnSliceInfo::GetCoordinate(libCZI::DimensionIndex dimension) const
{
    switch (dimension)
    {
    case libCZI::DimensionIndex::T:
        return this->t_coordinate_;
    case libCZI::DimensionIndex::C:
        return this->c_coordinate_;
    }

    return (numeric_limits<int>::min)();
}

/*virtual*/std::uint32_t BrickBucketManager::BrickResultOnSliceInfo::GetNumberOfSlices() const
{
    return this->bucket_data_->items->size();
}

/*virtual*/Brick BrickBucketManager::BrickResultOnSliceInfo::ComposeBrick(libCZI::PixelType pixel_type, int x, int y, uint32_t width, uint32_t height, uint32_t depth, BrickAllocator& allocator, bool immediately_release_source_memory)
{
    Brick brick;
    brick.info.pixelType = pixel_type;
    brick.info.width = width;
    brick.info.height = height;
    brick.info.depth = depth;// this->bucket_data_->items->size();
    brick.info.stride_line = libCZI::Utils::GetBytesPerPixel(brick.info.pixelType) * brick.info.width;
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;
    brick.data = allocator.Allocate(BrickAllocator::MemoryType::SourceBrick, static_cast<size_t>(brick.info.stride_plane) * brick.info.depth);

    memset(brick.data.get(), 0, static_cast<size_t>(brick.info.stride_plane) * brick.info.depth);
    // TODO(JBL): maybe only clear slices which are missing

    for (auto iterator = this->bucket_data_->items->begin(); iterator != this->bucket_data_->items->end(); ++iterator)
    {
        const int z_coordinate = iterator->z_coordinate;
        libCZI::IBitmapData* bitmap = iterator->plane.get();

        {
            // note: the locking-object for the bitmap must be destroyed *before* we delete the bitmap itself,
            //        so this local scope must not be removed
            const libCZI::ScopedBitmapLocker bitmap_locker(bitmap);
            Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(
            {
               iterator->x_position - x,
               iterator->y_position - y,
               bitmap->GetPixelType(),
               bitmap_locker.ptrDataRoi,
               bitmap_locker.stride,
               static_cast<int>(bitmap->GetWidth()),
               static_cast<int>(bitmap->GetHeight()),
               static_cast<uint8_t*>(brick.data.get()) + static_cast<size_t>(z_coordinate) * brick.info.stride_plane,
               brick.info.stride_line,
               static_cast<int>(brick.info.width),
               static_cast<int>(brick.info.height)
             });
        }

        if (immediately_release_source_memory)
        {
            iterator->plane.reset();
        }
    }

    return brick;
}

/*virtual*/BrickBucketManager::BrickResultOnSliceInfo::~BrickResultOnSliceInfo()
{
    delete this->bucket_data_;
}
