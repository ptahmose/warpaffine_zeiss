// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "NullSlicesWriter.h"
#include <memory>

using namespace std;
using namespace libCZI;

std::uint32_t NullSlicesWriter::GetNumberOfPendingSliceWriteOperations()
{
    return 0;
}

void NullSlicesWriter::AddSlice(const AddSliceInfo& add_slice_info)
{
}

void NullSlicesWriter::AddAttachment(const std::shared_ptr<libCZI::IAttachment>& attachment)
{
}

void NullSlicesWriter::Close(const std::shared_ptr<libCZI::ICziMetadata>&, const libCZI::ScalingInfo*, const std::function<void(libCZI::IXmlNodeRw*)>&)
{
}
