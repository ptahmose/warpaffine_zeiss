// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include <LibWarpAffine_Config.h>
#include "IWarpAffine.h"

#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE

/// Implementation of the IWarpAffine-operation based on Intel-Performance-Primitives library.
class WarpAffineIPP : public IWarpAffine
{
public:
    /// @copydoc IWarpAffine::Execute
    void Execute(
        const Eigen::Matrix4d& transformation,
        const IntPos3& destination_brick_position,
        Interpolation interpolation,
        const Brick& source_brick,
        const Brick& destination_brick) override;
private:
    static Eigen::Matrix4d IncludeDestinationBrickPosition(const Eigen::Matrix4d& transformation, const IntPos3& destination_brick_position);
    void ExecuteMinimalSource(const Eigen::Matrix4d& transformation, const IntPos3& destination_brick_position, Interpolation interpolation, const Brick& source_brick, const Brick& destination_brick);
};

#endif
