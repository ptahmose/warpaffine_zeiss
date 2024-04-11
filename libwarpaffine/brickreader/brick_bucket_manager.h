// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <atomic>
#include <functional>
#include <vector>
#include <memory>
#include "../inc_libCZI.h"
#include "../brick.h"
#include "brick_coordinate.h"

class BrickAllocator;

/// This is representing the resulting brick, i.e. when a brick is complete, then this object is
/// constructed and reported.
class IBrickResult
{
public:
    IBrickResult() = default;
    IBrickResult(const IBrickResult&) = delete;
    IBrickResult& operator=(const IBrickResult&) = delete;
    IBrickResult(IBrickResult&&) = delete;
    IBrickResult& operator=(IBrickResult&& other) = delete;

    /// Gets the coordinate of the brick: Currently, only T and C are considered valid here.
    /// \param  dimension   The dimension.
    /// \returns    The coordinate.
    [[nodiscard]] virtual int GetCoordinate(libCZI::DimensionIndex dimension) const = 0;

    /// Gets number of slices (with actual data) contained in this brick result.
    /// \returns    The number of slices.
    [[nodiscard]] virtual std::uint32_t GetNumberOfSlices() const = 0;

    /// This method composes the data into a brick-object.This means that the brick is allocated,
    /// and then the bitmaps are copied into it, so this is a somewhat expensive operation. The
    /// boolean "immediately_release_source_memory" instructs to release the plane-bitmaps as
    /// soon as possible, i.e. immediately after it was copied into the destination brick.
    /// Releasing the memory this way may result in a lower memory-footprint. Otherwise, it will
    /// be released when this object is destroyed. immediately_release_source_memory= true also
    /// means that the operation only works once. The specified parameters describing the output
    /// brick are more or less for debugging purposes, meaning that one specifies the expected
    /// output, and if this deviates from the internal data, then this method is throwing an
    /// exception. Not conversion or something in this direction is actually implemented.
    ///
    /// \param      pixel_type                          Type of the pixel.
    /// \param      x                                   The x coordinate (of the brick in the x-y-plane).
    /// \param      y                                   The y coordinate (of the brick in the x-y-plane).
    /// \param      width                               The width of the output brick in pixels.
    /// \param      height                              The height of the output brick in pixels.
    /// \param      depth                               The depth of the output brick in pixels.
    /// \param [in] allocator                           The allocator object to used for allocating the output-brick.
    /// \param      immediately_release_source_memory   True to immediately release source memory.
    ///
    /// \returns    The brick.
    [[nodiscard]] virtual Brick ComposeBrick(libCZI::PixelType pixel_type, int x, int y, std::uint32_t width, std::uint32_t height, std::uint32_t depth, BrickAllocator& allocator, bool immediately_release_source_memory) = 0;

    virtual ~IBrickResult() = default;
};


/// This "brick-bucket-manager"-class has the following purpose: conceptually, we have a number of buckets,
/// then we add items to this object, and it reports whenever a bucket is complete. The buckets in this
/// case are "bricks", so we wait until all slices making up a brick are present, and then report the
/// completion a such a brick.
class BrickBucketManager
{
private:
    /// Representation of a bitmap-on-a-plane. The bitmap is located at the specified x and y position,
    /// and a z-index.
    struct PlaneAndIndexZ
    {
        std::shared_ptr<libCZI::IBitmapData> plane;
        int x_position;
        int y_position;
        int z_coordinate;
    };

    /// This structure is used to gather a bucket - i.e. all slices making up a particular brick.
    struct BucketData
    {
        BucketData() = delete;

        /// Constructor which reserves the specified number of planes.
        ///
        /// \param  no_of_items The number of planes to prepare.
        explicit BucketData(size_t no_of_items)
            : items(std::make_shared<std::vector<PlaneAndIndexZ>>(no_of_items))
        {}

        /// This counter tracks the number of planes which are
        /// readily available. This counter must be incremented *after*
        /// the plane has been put into the vector.
        std::atomic_uint32_t number_of_planes_ready{ 0 };

        /// This counter is intended for getting an empty slot. So, this counter
        /// should be incremented *before* putting the plane into the vector.
        std::atomic_uint32_t next_index_for_plane{ 0 };

        /// This vector contains the bitmaps making up the brick.
        std::shared_ptr<std::vector<PlaneAndIndexZ>> items;
    };

    std::map<BrickCoordinate, BucketData*> brickcoordinate_buckets_map_;
    std::function<void(const std::shared_ptr<IBrickResult>&)> functor_brick_done_;
public:
    /// This structure gathers all relevant information for adding a slice.
    struct SliceInfo
    {
        /// The bitmap containing the plane. Note that we (for the time being) assume that one plane
        /// is exactly one bitmap and this bitmap is covering the whole area (i.e. no concept of
        /// a mosaic and this bitmap is positioned at (0,0)).
        std::shared_ptr<libCZI::IBitmapData> bitmap;

        int x_position;       ///< The x offset - the x coordinate where to put this bitmap in the brick.
        int y_position;       ///< The y offset - the y coordinate where to put this bitmap in the brick.

        int t_coordinate;   ///< The t coordinate.
        int z_coordinate;   ///< The t coordinate.
        int c_coordinate;   ///< the c coordinate.
    };

    BrickBucketManager() = delete;

    /// Constructor taking a functor which is to be called whenever a brick is finished.
    /// Note that this functor will be called from an arbitrary thread-context.
    /// \param  functor_brick_done  A functor which will be called whenever a brick is finished.
    explicit BrickBucketManager(std::function<void(const std::shared_ptr<IBrickResult>&)> functor_brick_done);

    /// Prepare the instance for operation. The caller needs to specify how many t's and c's are
    /// expected. In addition, a function is to be provided which is called to query for the number
    /// of z's making up a brick. This allows to give a different depth for each brick, but this not
    /// intended to be made us at this point. So far, many parts of the application assume that
    /// the depth of all bricks is the same.
    ///
    /// \param  t_count                         Number of t's.
    /// \param  c_count                         Number of c's.
    /// \param  functor_get_number_of_slices    The functor to get the number of slices (where t and c are passed in).
    void Setup(std::uint32_t t_count, std::uint32_t c_count, const std::function<int(int t, int c)>& functor_get_number_of_slices);

    /// Adds a slice. This method may be called from an arbitrary thread-context and concurrently without restrictions.
    /// \param  slice_info  Information describing the slice.
    void AddSlice(const SliceInfo& slice_info);
private:
    /// Adds the specified bitmap (containing a plane) to the specified bucket. If the brick
    /// (which is represented by the bucket) is complete (i.e. that all slices are now present),
    /// then this method return true; otherwise false. Note that this method is intended to be
    /// called concurrently (from arbitrary threads), and it never blocks.
    ///
    /// \param [in] bucket_data     The bucket into which to add the plane.
    /// \param      x               The x coordinate.
    /// \param      y               The y coordinate.
    /// \param      z_coordinate    The z-index of the plane.
    /// \param      bitmap          The bitmap (containing the plane).
    ///
    /// \returns    True if the brick is complete; otherwise false.
    bool AddToBrick(BucketData* bucket_data, int x, int y, int z_coordinate, std::shared_ptr<libCZI::IBitmapData> bitmap);

    /// This class is implementing the "IBrickResult" based on BucketData.
    /// It is non-copyable and non-movable.
    class BrickResultOnSliceInfo : public IBrickResult
    {
    private:
        BrickBucketManager::BucketData* bucket_data_;
        int t_coordinate_;
        int c_coordinate_;
    public:
        BrickResultOnSliceInfo() = delete;
        BrickResultOnSliceInfo(const BrickResultOnSliceInfo&) = delete;
        BrickResultOnSliceInfo& operator=(const BrickResultOnSliceInfo&) = delete;
        BrickResultOnSliceInfo(BrickResultOnSliceInfo&&) = delete;
        BrickResultOnSliceInfo& operator=(BrickResultOnSliceInfo&& other) = delete;

        /// Constructor which takes a pointer to an BucketData-object. This instance is taking ownership of the
        /// BucketData-object.
        ///
        /// \param          t_coordinate    The T-coordinate.
        /// \param          c_coordinate    The C-coordinate.
        /// \param [in,out] bucket_data     The BucketData-object to operate on. This instance is taking ownership of the object passed in here.
        BrickResultOnSliceInfo(int t_coordinate, int c_coordinate, BrickBucketManager::BucketData* bucket_data);

        /// @copydoc IBrickResult::GetCoordinate
        int GetCoordinate(libCZI::DimensionIndex dimension) const override;

        /// @copydoc IBrickResult::GetNumberOfSlices
        std::uint32_t GetNumberOfSlices() const override;

        /// @copydoc IBrickResult::ComposeBrick
        Brick ComposeBrick(libCZI::PixelType pixel_type, int x, int y, std::uint32_t width, std::uint32_t height, std::uint32_t depth, BrickAllocator& allocator, bool immediately_release_source_memory) override;

        ~BrickResultOnSliceInfo() override;
    };
};
