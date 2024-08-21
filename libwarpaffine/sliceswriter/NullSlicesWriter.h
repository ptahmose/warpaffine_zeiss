// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "ISlicesWriter.h"
#include <memory>

/// Implementation of ICziSlicesWriter that does nothing - i.e. the slices are not written anywhere, and
/// the data is discarded. It is used for testing purposes.
class NullSlicesWriter : public ICziSlicesWriter
{
public:
    std::uint32_t GetNumberOfPendingSliceWriteOperations() override;
    void AddSlice(const AddSliceInfo& add_slice_info) override;
    void AddAttachment(const std::shared_ptr<libCZI::IAttachment>& attachment) override;
    void Close(const std::shared_ptr<libCZI::ICziMetadata>& source_metadata,
                const libCZI::ScalingInfo* new_scaling_info,
                const std::function<void(libCZI::IXmlNodeRw*)>& tweak_metadata_hook) override;
};
