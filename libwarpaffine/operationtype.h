// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

/// Values that represent the "operation type", i.e. the geometric transformation to be performed.
enum class OperationType
{
    Identity,                           ///< The identify transformation - not really useful, but great for debugging.

    Deskew,                             ///< This corresponds to ZEN "Processing Method: Deskew, XyTargetIsRotatedBy90=false".

    CoverGlassTransform,                ///< This corresponds to ZEN "Processing Method: Cover Glass Transform, XyTargetIsRotatedBy90=false".

    CoverGlassTransformAndXYRotated,    ///< This corresponds to ZEN "Processing Method: Cover Glass Transform, XyTargetIsRotatedBy90=true".
};
