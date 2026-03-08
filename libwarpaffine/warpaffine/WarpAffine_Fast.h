// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include "IWarpAffine.h"

/// Implementation of a home-brew "warp-affine" operation. This is a faster version of the reference implementation.
class WarpAffine_Fast : public IWarpAffine
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
