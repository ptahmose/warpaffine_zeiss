// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "brick_enumerator.h"
#include <algorithm>

using namespace std;
using namespace libCZI;

//-------------------------------------------------------------------------------------------------

void BrickEnumerator::Reset(std::optional<int> max_t, int max_c, const TileIdentifierToRectangleMap& regions)
{
    this->tile_identifier_and_rects_.clear();
    for (const auto& region : regions)
    {
        this->tile_identifier_and_rects_.emplace_back(TileIdentifierAndRect{region.first, region.second });
    }

    if (max_t.has_value())
    {
        this->max_t_ = max_t.value();
        this->t_ = 0;
        this->is_t_valid_ = true;
    }
    else
    {
        this->is_t_valid_ = false;
    }

    this->max_c_ = max_c;
    this->c_ = 0;

    this->tile_number_ = 0;
}

bool BrickEnumerator::GetNextBrickCoordinate(libCZI::CDimCoordinate& coordinate, TileIdentifier& tile_identifier, libCZI::IntRect& rectangle)
{
    std::unique_lock<std::mutex> lck(this->mutex_);

    // we first increment C, then T, then M
    //
    // reminder: we require C to be valid, T is optional, M is required

    if ((this->is_t_valid_ == true && this->t_ >= this->max_t_) ||
        this->c_ >= this->max_c_ ||
        this->tile_number_ >= this->tile_identifier_and_rects_.size())
    {
        return false;
    }

    coordinate.Clear();
    coordinate.Set(DimensionIndex::C, this->c_);
    if (this->is_t_valid_)
    {
        coordinate.Set(DimensionIndex::T, this->t_);
    }

    const TileIdentifierAndRect& tile_identifier_and_rect = this->tile_identifier_and_rects_[this->tile_number_];
    rectangle = tile_identifier_and_rect.rectangle;
    tile_identifier = tile_identifier_and_rect.tile_identifier;

    if (++this->c_ >= this->max_c_)
    {
        this->c_ = 0;

        if (this->is_t_valid_)
        {
            if (++this->t_ >= this->max_t_)
            {
                this->t_ = 0;
                ++this->tile_number_;
            }
        }
        else
        {
            ++this->tile_number_;
        }
    }

    return true;
}
