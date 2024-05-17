// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <tuple>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <Eigen/Eigen>
#include "inc_libCZI.h"
#include "geotypes.h"
#include "appcontext.h"
#include "document_info.h"
#include "calcresulthash.h"
#include "sliceswriter/ISlicesWriter.h"
#include "warpaffine/IWarpAffine.h"
#include "brickreader/IBrickReader.h"
#include "BrickAllocator.h"

struct WarpStatistics
{
    double elapsed_time_since_start_in_seconds;
    std::uint64_t bytes_read_from_source_file;
    double datarate_read_from_source_file;
    std::uint64_t source_brick_data_delivered;
    std::uint64_t source_bricks_delivered;
    std::uint64_t source_slices_read;
    double source_brick_data_delivered_per_second;
    double source_bricks_delivered_per_minute;
    double source_slices_read_per_second;
    std::uint32_t warp_tasks_in_flight;
    std::uint32_t compression_tasks_in_flight;
    std::uint32_t write_slices_queue_length;
    bool reader_throttled;
    std::uint32_t task_arena_queue_length;      ///< The number of tasks in task-arena's queue.
    std::uint32_t currently_active_tasks;       ///< The number of currently active tasks in task-arena's thread-pool.
    std::uint32_t currently_suspended_tasks;    ///< The number of currently suspended tasks in task-arena's thread-pool.
    std::uint64_t brickreader_compressed_subblocks_in_flight;
    std::uint64_t brickreader_uncompressed_planes_in_flight;
    std::uint32_t subblocks_added_to_writer;    ///< The number of slices added to the writer.
    float         total_progress_percent;       ///< An estimation of the overall progress, in percent (between 0 and 100). It is NaN in case no progress information is available.

    std::array<std::uint64_t, BrickAllocator::Count_of_MemoryTypes> memory_status;
};

/// This class is orchestrating the warp-operation.
class DoWarp
{
    /// This encapsulates the "information about the unit of work" it is operated on. The unit-of-work
    /// here is a "plane-of-bricks" or a "3D-plane".
    class OutputBrickInfoRepository
    {
    public:
        /// This struct gathers the rectangle and the M- and Scene-index (of an output-brick).
        struct TilingRectAndMandSceneIndex
        {
            int m_index;                 ///< The m-index (this will always be valid).
            int s_index;                 ///< The scene-index (numeric_limits<int>::min() is used to denote an invalid scene-index).
            libCZI::IntRect rectangle;   ///< The position and size.
        };

        /// This struct gives the "tiling of an output brick". The 'tiling' vector gives a subdivision of
        /// the output-brick (into potentially many tile-bricks) together with their respective m- and scene-
        /// index.
        struct DestinationBrickInfo
        {
            IntCuboid   cuboid;
            std::vector<TilingRectAndMandSceneIndex> tiling;
        };
    private:
        std::map<BrickInPlaneIdentifier, DestinationBrickInfo> map_brickid_destinationbrickinfo_;
        std::uint32_t number_of_subblocks_to_output_{ 0 };
    public:
        OutputBrickInfoRepository(const AppContext& context, const DeskewDocumentInfo& document_info, const Eigen::Matrix4d& transformation_matrix);

        /// Gets the extent of the output brick for the specified input-brick. If the specified brick_identifier is
        /// unknown/invalid, an out_of_range is exception is thrown.
        /// \param  brick_identifier    Identifier of the input brick.
        /// \returns    The output extent.
        [[nodiscard]] IntSize3 GetOutputExtent(const BrickInPlaneIdentifier& brick_identifier) const;

        [[nodiscard]] IntCuboid GetOutputVolume(const BrickInPlaneIdentifier& brick_identifier) const;
        [[nodiscard]] const DestinationBrickInfo& GetDestinationInfo(const BrickInPlaneIdentifier& brick_identifier) const;

        /// Gets the total number of subblocks which is to be expected to be created (when processing the "3D-plane").
        ///
        /// \returns The total number of subblocks to output.
        [[nodiscard]] std::uint32_t GetTotalNumberOfSubblocksToOutput() const;
    private:
        static std::vector<libCZI::IntRect> Create2dTiling(std::uint32_t max_extent, const libCZI::IntRect& rectangle);
    };

    AppContext& context_;
    Eigen::Matrix4d transformation_matrix_;
    DeskewDocumentInfo document_info_;
    std::shared_ptr<ICziSlicesWriter> writer_;
    std::shared_ptr< ICziBrickReader> brick_reader_;
    std::shared_ptr<IWarpAffine> warp_affine_engine_;

    std::unique_ptr<CalcResultHash> calculate_result_hash_; ///< This object is used to calculate a hash of the output; may be null in case this is not wanted.

    std::chrono::time_point<std::chrono::high_resolution_clock> time_point_operation_started_;

    std::uint32_t output_width_;
    std::uint32_t output_height_;
    std::uint32_t output_depth_;
    OutputBrickInfoRepository output_brick_info_repository_;
    std::uint32_t total_number_of_subblocks_to_output;

    std::atomic_uint32_t compression_tasks_in_flight_{ 0 };
    std::atomic_uint32_t warp_tasks_in_flight_{ 0 };
    std::atomic_uint32_t total_tasks_in_flight_{ 0 };
    std::atomic_uint32_t number_of_subblocks_added_to_writer_{ 0 };

    std::atomic_bool above_high_water_mask2_{ false };
public:
    /// Gets extent of the output brick for the specified input-brick. Note that
    /// this result does **not** including a tiling of the output-brick. If the specified brick_identifier is
    /// unknown/invalid, an out_of_range is exception is thrown.
    /// \param  brick_identifier    Identifier for the input-brick.
    /// \returns    The extent of the output brick.
    IntCuboid GetOutputVolume(const BrickInPlaneIdentifier& brick_identifier) const;

    IntSize3 GetLargestOutputExtentIncludingTiling(const BrickInPlaneIdentifier& brick_identifier) const;

    /// Gets output extent - i.e. the pixel size of the output bricks.
    /// \returns The output extent.
    std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> GetOutputExtent() const;

    const Eigen::Matrix4d& GetTransformationMatrix() const;

    /// Constructor - where all relevant objects are passed in.
    ///
    /// \param [in,out] context                       The app-context.
    /// \param          number_of_3dplanes_to_process Number of 3dplanes to process (this information is used for deriving total progress only).
    /// \param          document_info                 Information describing the document.
    /// \param          transformation_matrix         The transformation matrix.
    /// \param          brick_reader                  The brick reader.
    /// \param          writer                        The writer.
    /// \param          warp_affine_engine            The warp affine engine.
    DoWarp(
        AppContext& context,
        std::uint32_t number_of_3dplanes_to_process,
        const DeskewDocumentInfo& document_info,
        const Eigen::Matrix4d& transformation_matrix,
        std::shared_ptr< ICziBrickReader> brick_reader,
        std::shared_ptr<ICziSlicesWriter> writer,
        std::shared_ptr<IWarpAffine> warp_affine_engine);

    void DoOperation();

    bool IsDone();
    WarpStatistics GetStatistics();

    void WaitUntilDone();

    bool TryGetHash(std::array<uint8_t, 16>* hash_code) const;
private:
    void InputBrick(const Brick& brick, const BrickCoordinateInfo& coordinate_info);
private:
    std::vector<ITaskArena::SuspendHandle> resume_handles_;
    std::mutex mutex_resume_handles_;

    struct OutputSliceToCompressTaskInfo
    {
        Brick brick;
        int z_slice;
    };

    void ProcessOutputSlice(OutputSliceToCompressTaskInfo* output_slice_task_info, const libCZI::CDimCoordinate& coordinate, const SubblockXYM& xym);

    void IncWarpTasksInFlight();
    void DecWarpTasksInFlight();
    void IncCompressionTasksInFlight();
    void DecCompressionTasksInFlight();

    std::uint32_t GetTotalNumberOfTasksInFlight();

    bool IsSubTilingRequired(const IntSize3& size);
    std::vector<libCZI::IntRect> Create2dTiling(const libCZI::IntRect& rectangle);
    Brick CreateBrick(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth);
    Brick CreateBrickAndWaitUntilAvailable(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth);

    void ProcessBrickCommon2(const Brick& brick, const Brick& destination_brick, const BrickCoordinateInfo& coordinate_info, std::uint32_t source_depth, const OutputBrickInfoRepository::TilingRectAndMandSceneIndex& rect_and_tile_identifier /*const libCZI::IntRect& roi,  int m_index*/);

    std::tuple<libCZI::CompressionMode, std::shared_ptr<libCZI::IMemoryBlock>> Compress(const OutputSliceToCompressTaskInfo& output_slice_task_info);

    void DestinationMemoryReleased();
    void AddResumeHandle(ITaskArena::SuspendHandle handle);
    void DoResume();
    float CalculateTotalProgress();
};
