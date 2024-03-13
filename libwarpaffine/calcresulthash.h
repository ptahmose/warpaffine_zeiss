// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "inc_libCZI.h"
#include <memory>
#include <array>
#include <mutex>

/// Use this class to calculate a hash for "output data". For the information provided with
/// an "AddSlice"-call, hash-codes are calculated and a "combined and cumulative" hash-code
/// is maintained. This "cumulative" hash is invariant to order, so the order in which data
/// is added (via "AddSlice") is not relevant for the resulting hash.
/// This class allows concurrent calls to "AddSlice".
class CalcResultHash
{
private:
    std::mutex mutex_;
    std::array<std::uint8_t, 16> hash_;
public:
    /// Default constructor.
    CalcResultHash();

    /// Adds a slice to the hash calculation. Both the memory-block and the coordinate are hashed, and the
    /// two hashes are aggregated to the "overall" hash (as maintained by this class).
    /// \param  memory_block     Memory block to be hashed.
    /// \param  coordinate       The coordinate.
    void AddSlice(const std::shared_ptr<libCZI::IMemoryBlock>& memory_block, const libCZI::CDimCoordinate& coordinate);

    /// Gets the cumulative hash.
    /// \returns    The hash.
    std::array<std::uint8_t, 16> GetHash();
private:
    void AddHash(const std::array<std::uint8_t, 16>& hash_to_add);
};
