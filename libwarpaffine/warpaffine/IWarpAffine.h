// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <memory>
#include <Eigen/Eigen>
#include "../geotypes.h"
#include "../brick.h"
#include "../cmdlineoptions_enums.h"

/// This interfaces defines the "warp-operation". Note that the method "Execute" is intended to
/// be called concurrently.
class IWarpAffine
{
public:
    /// Executes the "affine-warp-transformation" on the specified brick, and puts the result into the specified destination brick.
    /// Conceptually, the input-brick is in a coordinates system with one edge at (0,0,0) and extending to (w,h,d) given by its
    /// pixel-extent. Then, we apply the transformation matrix to this cube, and sample the result again from 'destination_brick_position' to
    /// destination_brick_position+(destination_brick.info.width, destination_brick.info.height, destination_brick.info.depth).
    /// Note: this means that the transformation matrix should be constructed in a way that the "desired edge" is at (0,0,0)
    /// which can easily be achieved by adding a translation.
    /// \param  transformation              The transformation matrix.
    /// \param  destination_brick_position  The position of the destination (in the coordinate system with the edge of the source brick at the origin).
    /// \param  interpolation               The interpolation mode.
    /// \param  source_brick                The source brick.
    /// \param  destination_brick           The destination brick.
    virtual void Execute(
        const Eigen::Matrix4d& transformation,
        const IntPos3& destination_brick_position,
        Interpolation interpolation,
        const Brick& source_brick,
        const Brick& destination_brick) = 0;

    virtual ~IWarpAffine() = default;
};

/// Creates an instance of an IWarpAffine implementation.
///
/// \param  implementation  The implementation to be created.
///
/// \returns    The newly created warp-affine object.
std::shared_ptr<IWarpAffine> CreateWarpAffine(WarpAffineImplementation implementation);
