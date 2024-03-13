// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <memory>
#include "WarpAffineNull.h"

using namespace std;
using namespace libCZI;

void WarpAffineNull::Execute(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    memset(destination_brick.data.get(), 0, static_cast<size_t>(destination_brick.info.stride_plane) * destination_brick.info.depth);
}
