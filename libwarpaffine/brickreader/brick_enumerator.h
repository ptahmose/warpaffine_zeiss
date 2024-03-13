// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once 

#include <vector>
#include <mutex>
#include <optional>
#include "../inc_libCZI.h"
#include "../czi_helpers.h"

/// This class is used to enumerate all "tiles" in a given range. With the Reset-method, the range is set.
class BrickEnumerator
{
private:
    bool is_t_valid_{ false };
    int t_{ 0 };
    int c_{ 0 };
    int tile_number_{ 0 };
    int max_t_{ 0 };
    int max_c_{ 0 };
    std::vector<TileIdentifierAndRect> tile_identifier_and_rects_;
    std::mutex mutex_;
public:
    BrickEnumerator() = default;
    void Reset(std::optional<int> max_t, int max_c, const TileIdentifierToRectangleMap& regions);
    bool GetNextBrickCoordinate(libCZI::CDimCoordinate& coordinate, TileIdentifier& tile_identifier, libCZI::IntRect& rectangle);
};
