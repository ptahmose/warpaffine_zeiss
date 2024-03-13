// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include "IWarpAffine.h"

/// Implementation of a home-brew "warp-affine" operation. It is by no means performance optimized.
class WarpAffine_Reference : public IWarpAffine
{
public:
    /// @copydoc IWarpAffine::Execute
    void Execute(
        const Eigen::Matrix4d& transformation,
        const IntPos3& destination_brick_position,
        Interpolation interpolation,
        const Brick& source_brick,
        const Brick& destination_brick) override;

    static void ExecuteFunction(
        const Eigen::Matrix4d& transformation,
        const IntPos3& destination_brick_position,
        Interpolation interpolation,
        const Brick& source_brick,
        const Brick& destination_brick);
};
