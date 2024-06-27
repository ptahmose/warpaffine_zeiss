// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <map>
#include <LibWarpAffine_Config.h>
#include "operationtype.h"
#include "cmdlineoptions_enums.h"
#include "utilities.h"
#include "inc_libCZI.h"

class CCmdLineOptions
{
private:
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    static constexpr WarpAffineImplementation kDefaultWarpAffineEngineImplementation = WarpAffineImplementation::kIPP;
#else
    static constexpr WarpAffineImplementation kDefaultWarpAffineEngineImplementation = WarpAffineImplementation::kReference;
#endif

    static const char* kDefaultCompressionOptions;
    std::string cziSourceFilename;
    std::string cziDestinationFilename;
    Interpolation interpolation_{ Interpolation::kNearestNeighbor };
    OperationType type_of_operation_{ OperationType::Identity };
    LibCziReaderImplementation libCziReaderImplementation_{ LibCziReaderImplementation::kStock };
    int number_of_reader_threads_{ 1 };
    BrickReaderImplementation brick_reader_implementation_{ BrickReaderImplementation::kPlaneReader };
    WarpAffineImplementation warp_affine_engine_implementation_{ kDefaultWarpAffineEngineImplementation };
    TestStopPipelineAfter test_stop_pipeline_after_{ TestStopPipelineAfter::kReadFromSource };
    TaskArenaImplementation task_arena_implementation_{ TaskArenaImplementation::kTBB };
    libCZI::Utils::CompressionOption compression_option_;
    PropertyBag property_bag_brick_source_;
    MessagesPrintVerbosity verbosity_{ MessagesPrintVerbosity::kNormal };
    bool hash_result_{ false };
    std::uint32_t max_tile_extent_{ 2048 };
    std::uint64_t override_main_memory_size_{ 0 };
    bool override_check_for_skewed_source_{ false };
    bool use_acquisition_tiles_{ false };
    std::string source_stream_class_;
    std::map<int, libCZI::StreamsFactory::Property> property_bag_for_stream_class;
public:
    /// Values that represent the result of the "Parse"-operation.
    enum class ParseResult
    {
        OK,     ///< An enum constant representing the result "arguments successfully parsed, operation can start".
        Exit,   ///< An enum constant representing the result "operation complete, the program should now be terminated, e.g. the synopsis was printed".
        Error   ///< An enum constant representing the result "there was an error parsing the command line arguments, program should terminate".
    };

    /// Parse the command line arguments. The arguments are expected to have
    /// UTF8-encoding.
    ///
    /// \param          argc    The number of arguments.
    /// \param [in]     argv    Pointer to an array with the null-terminated, UTF8-encoded arguments.
    ///
    /// \returns    True if it succeeds, false if it fails.
    [[nodiscard]] ParseResult Parse(int argc, char** argv);

    [[nodiscard]] std::wstring GetSourceCZIFilenameW() const;
    [[nodiscard]] std::wstring GetDestinationCZIFilenameW() const;

    /// Query if the "null writer" is configured (i.e. data is not written to disk, it is just discarded)
    /// \returns    True if "null writer" is to be used, false if not.
    [[nodiscard]] bool GetUseNullWriter() const;

    [[nodiscard]] const std::string& GetSourceCZIFilename() const { return this->cziSourceFilename; }
    [[nodiscard]] const std::string& GetSourceStreamClass() const { return this->source_stream_class_; }
    [[nodiscard]] const std::map<int, libCZI::StreamsFactory::Property>& GetPropertyBagForStreamClass() const { return this->property_bag_for_stream_class; }
    [[nodiscard]] const std::string& GetDestinationCZIFilename() const { return this->cziDestinationFilename; }
    [[nodiscard]] Interpolation GetInterpolationMode() const { return this->interpolation_; }
    [[nodiscard]] OperationType GetTypeOfOperation()const { return this->type_of_operation_; }
    [[nodiscard]] LibCziReaderImplementation GetLibCziReaderImplementation() const { return this->libCziReaderImplementation_; }
    [[nodiscard]] int GetNumberOfReaderThreads() const { return this->number_of_reader_threads_; }
    [[nodiscard]] BrickReaderImplementation GetBrickReaderImplementation() const { return this->brick_reader_implementation_; }
    [[nodiscard]] WarpAffineImplementation GetWarpAffineEngineImplementation() const { return this->warp_affine_engine_implementation_; }
    [[nodiscard]] TestStopPipelineAfter GetTestStopPipelineAfter() const { return this->test_stop_pipeline_after_; }
    [[nodiscard]] TaskArenaImplementation GetTaskArenaImplementation() const { return this->task_arena_implementation_; }
    [[nodiscard]] const libCZI::Utils::CompressionOption& GetCompressionOptions() const { return this->compression_option_; }
    [[nodiscard]] const IPropBag& GetPropertyBagForBrickSource() const { return this->property_bag_brick_source_; }
    [[nodiscard]] MessagesPrintVerbosity GetPrintOutVerbosity() const { return this->verbosity_; }
    [[nodiscard]] bool GetDoCalculateHashOfOutputData() const { return this->hash_result_; }
    [[nodiscard]] std::uint32_t GetMaxOutputTileExtent() const { return this->max_tile_extent_; }
    [[nodiscard]] bool GetIsMainMemorySizeOverrideValid() const { return this->override_main_memory_size_ != 0; }
    [[nodiscard]] std::uint64_t GetMainMemorySizeOverride() const { return this->override_main_memory_size_; }
    [[nodiscard]] bool GetOverrideCheckForSkewedSourceDocument() const { return this->override_check_for_skewed_source_; }
    [[nodiscard]] bool GetUseAcquisitionTiles() const { return this->use_acquisition_tiles_; }
private:
    bool TryParseInputStreamCreationPropertyBag(const std::string& s, std::map<int, libCZI::StreamsFactory::Property>* property_bag);
};
