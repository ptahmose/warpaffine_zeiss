// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include "IWarpAffine.h"

/// An implementation of the IWarpAffine-interface which does nothing - or next to nothing, it just
/// zero-fills the destination brick.
/// It is intended for performance testing.
class WarpAffineNull : public IWarpAffine
{
public:
    /// @copydoc IWarpAffine::Execute
    void Execute(
        const Eigen::Matrix4d& transformation,
        const IntPos3& destination_brick_position,
        Interpolation interpolation,
        const Brick& source_brick,
        const Brick& destination_brick) override;
};
