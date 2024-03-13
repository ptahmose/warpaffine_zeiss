// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <memory>
#include "WarpAffine_Reference.h"
#include "reference_warp.h"

using namespace std;
using namespace libCZI;

void WarpAffine_Reference::Execute(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    return WarpAffine_Reference::ExecuteFunction(transformation, destination_brick_position, interpolation, source_brick, destination_brick);
}

/*static*/void WarpAffine_Reference::ExecuteFunction(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    ReferenceWarp reference_warp(source_brick, destination_brick);
    reference_warp.SetInterpolation(interpolation);

    Eigen::Matrix4d translation;
    translation << 1, 0, 0, -destination_brick_position.x_position, 0, 1, 0, -destination_brick_position.y_position, 0, 0, 1, -destination_brick_position.z_position, 0, 0, 0, 1;
    const auto translation_including_offset = translation * transformation;
    reference_warp.SetTransformation(translation_including_offset);

    reference_warp.Do();
}
