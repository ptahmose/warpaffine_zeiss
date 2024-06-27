// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <LibWarpAffine_Config.h>
#include <string>
#include <cstdint>
#include <optional>
#include <memory>
#include "../inc_libCZI.h"
#include "../appcontext.h"

///Interface for writing slices (aka subblocks) to a CZI file.
class ICziSlicesWriter
{
public:
    /// The information about "a slice (or subblock) to be added" is gathered here.
    struct AddSliceInfo
    {
        /// The blob of data to be put into the subblock. The data is expected to be compressed (if applicable), no further processing
        /// is done with the data passed in here.
        std::shared_ptr<libCZI::IMemoryBlock> subblock_raw_data;

        /// The compression mode.
        libCZI::CompressionMode compression_mode{ libCZI::CompressionMode::Invalid };

        /// The pixeltype.
        libCZI::PixelType pixeltype{ libCZI::PixelType::Invalid };

        /// The width of the subblock in pixels.
        std::uint32_t width{ 0 };

        /// The height of the subblock in pixels.
        std::uint32_t height{ 0 };

        /// The plane coordinate of the subblock.
        libCZI::CDimCoordinate coordinate;

        /// The m-index of the subblock. This may or may not be valid.
        std::optional<int> m_index;

        /// The scene-index of the subblock. This may or may not be valid.
        std::optional<int> scene_index;

        /// The X-position of the subblock.
        int x_position{ 0 };

        /// The Y-position of the subblock.
        int y_position{ 0 };

        /// The ID of the slice
        int slice_id;
    };

    /// Gets number of currently pending slice write operations.
    ///
    /// \returns The number of currently pending slice write operations.
    virtual std::uint32_t GetNumberOfPendingSliceWriteOperations() = 0;

    /// Adds a slice (or subblock).
    /// \param  add_slice_info Information describing the add slice.
    virtual void AddSlice(const AddSliceInfo& add_slice_info) = 0;

    /// Closes the output CZI-files. The specified metadata object (of the source document) is used
    /// to update the metadata of the output CZI-file.
    ///
    /// \param  source_metadata     The metadata object of the source document.
    /// \param  new_scaling_info    If non-null, this scaling information is set with the output document.
    /// \param  tweak_metadata_hook If non-null, this function will be called passing in the XML-metadata which
    ///                             is about to be written to the output file, allowing for modifications of it. 
    virtual void Close(const std::shared_ptr<libCZI::ICziMetadata>& source_metadata,
                        const libCZI::ScalingInfo* new_scaling_info,
                        const std::function<void(libCZI::IXmlNodeRw*)>& tweak_metadata_hook) = 0;

    virtual ~ICziSlicesWriter() = default;

    // non-copyable and non-moveable
    ICziSlicesWriter() = default;
    ICziSlicesWriter(const ICziSlicesWriter&) = default;             // copy constructor
    ICziSlicesWriter& operator=(const ICziSlicesWriter&) = default;  // copy assignment
    ICziSlicesWriter(ICziSlicesWriter&&) = default;                  // move constructor
    ICziSlicesWriter& operator=(ICziSlicesWriter&&) = default;       // move assignment
};

std::shared_ptr<ICziSlicesWriter> CreateNullSlicesWriter();
std::shared_ptr<ICziSlicesWriter> CreateSlicesWriterTbb(AppContext& context, const std::wstring& filename);
