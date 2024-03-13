// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include "../inc_libCZI.h"

/// Extend the IStream-interface by an instrumentation.
class IStreamEx : public libCZI::IStream
{
public:
    /// Gets the total number of bytes read from the file.
    ///
    /// \returns    The total bytes read.
    virtual std::uint64_t GetTotalBytesRead() = 0;
};
