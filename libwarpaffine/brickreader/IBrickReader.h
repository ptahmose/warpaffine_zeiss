// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include "../inc_libCZI.h"
#include "../mmstream/IStreamEx.h"
#include "../appcontext.h"
#include "../brick.h"

/// This structure gathers the statistics information provided by the brick-reader.
struct BrickReaderStatistics
{
    std::uint64_t source_file_data_read;            ///< Number of bytes read from the source file.
    std::uint64_t brick_data_delivered;             ///< The total size (in bytes) delivered as uncompressed bricks delivered down-streams.
    std::uint64_t bricks_delivered;                 ///< The number of bricks delivered down-streams.
    std::uint64_t slices_read;                      ///< Number of slices read.
    std::uint64_t compressed_subblocks_in_flight;   ///< Number of (compressed) subblocks in flight. If this value is numeric_limits<uint64_t>::max() it means - this value is not applicable for the specific brick-reader.
    std::uint64_t uncompressed_planes_in_flight;    ///< Number of (uncompressed) planes in flight. If this value is numeric_limits<uint64_t>::max() it means - this value is not applicable for the specific brick-reader.
};

/// Information about the "coordinates" of a brick - this includes the "document-dimensions" (in coordinate) like T, C, the
/// mosaic-index and the x-y-position of the top-left point of the brick in the document's pixel-coordinate-system.
struct BrickCoordinateInfo
{
    libCZI::CDimCoordinate coordinate;
    int mIndex;
    int scene_index;
    int x_position; ///< The x-position of the top-left point of the brick in the document's pixel-coordinate-system.
    int y_position; ///< The y-position of the top-left point of the brick in the document's pixel-coordinate-system.
};

/// This interface is used to abstract "reading from the source". It is representing the source, delivering bricks
/// "as fast as can". The bricks output from this class are uncompressed, so all necessary operations like decompression
/// occur inside this component.
/// It can be set to a "paused" state, in which case it will pause operation until the "paused" state
/// is reset.
class ICziBrickReader
{
public:
    /// Starts the operation of the brick-reader. The brick-reader will start delivering bricks as fast as it can
    /// and output them to the specified functor 'deliver_brick_func'. A call to this method will return immediately and
    /// operation will be done in the background.
    /// \param  deliver_brick_func Function which will be called to deliver a brick.
    virtual void StartPumping(
        const std::function<void(const Brick&, const BrickCoordinateInfo&)>& deliver_brick_func) = 0;

    /// Query if the operation has finished.
    /// \returns True if operation is finished, false if not.
    virtual bool IsDone() = 0;

    /// Gets statistics about the operation.
    /// \returns The operation status.
    virtual BrickReaderStatistics GetStatus() = 0;

    /// Wait until done - this method will block until the operation is finished.
    virtual void WaitUntilDone() = 0;

    /// Sets the reader object to "paused state". If paused the reader will not acquire additional resources (esp. memory), and
    /// it will not deliver any additional bricks. Note that the 'paused state' will not be entered immediately, so after
    /// return from this call, it is very well possible that the reader will still deliver some additional bricks.
    virtual void SetPauseState(bool pause) = 0;

    /// Gets a boolean whether the reader is in "paused state" (or, more precisely, if entering "paused state" was instructed).
    /// \returns True if the reader is in "paused state" (or was requested to enter pause state); false otherwise.
    virtual bool GetIsThrottledState() = 0;

    /// Allows access to the underlying "CZI-reader" object.
    /// \returns The underlying reader.
    virtual std::shared_ptr<libCZI::ICZIReader>& GetUnderlyingReader() = 0;

    virtual ~ICziBrickReader() = default;

    // non-copyable and non-moveable
    ICziBrickReader() = default;
    ICziBrickReader(const ICziBrickReader&) = default;             // copy constructor
    ICziBrickReader& operator=(const ICziBrickReader&) = default;  // copy assignment
    ICziBrickReader(ICziBrickReader&&) = default;                  // move constructor
    ICziBrickReader& operator=(ICziBrickReader&&) = default;       // move assignment

    /// This key for the "brick source property bag" is used by the "linear-reading" implementation,
    /// and gives the suggested limit for "max number of subblocks-in-flight-before-a-brick-is-finished".
    /// The type is "int32".
    static const char* kPropertyBagKey_LinearReader_max_number_of_subblocks_to_wait_for;  
};

std::shared_ptr<ICziBrickReader> CreateBrickReaderPlaneReader(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream);
std::shared_ptr<ICziBrickReader> CreateBrickReaderPlaneReader2(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream);
std::shared_ptr<ICziBrickReader> CreateBrickReaderLinearReading(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream);
