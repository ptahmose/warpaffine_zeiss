// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "inc_libCZI.h"
#include <memory>

/// This structure is defining a "brick" - a 3D-bitmap, composed of voxels. The memory layout is
/// that of a contiguous chunk of memory, where the slices are laid out one after the other.
/// It is described by the width,height depth and a stride per line (describing a slice), and
/// consecutive slices are placed at a distance given by stride_plane.
struct BrickInfo
{
    libCZI::PixelType   pixelType;      ///< The pixeltype of the brick.
    std::uint32_t       width;          ///< The width of the brick in pixels.
    std::uint32_t       height;         ///< The height of the brick in pixels.
    std::uint32_t       depth;          ///< The depth of the brick in pixels.
    std::uint32_t       stride_line;    ///< The stride of a line in bytes.
    std::uint32_t       stride_plane;   ///< The stride of a slice (aka plane) in bytes.

    /// Gets size of the brick in bytes. This is only counting the "payload" data.
    /// \returns The (payload) size of the brick in bytes.
    [[nodiscard]] inline std::uint64_t GetBrickDataSize() const
    {
        return static_cast<std::uint64_t>(libCZI::Utils::GetBytesPerPixel(this->pixelType)) * this->width * this->height * this->depth;
    }
};

/// This structure combines the "information about the brick" and the actual voxel-data.
struct Brick
{
    BrickInfo info;             ///< The information describing the brick.
    std::shared_ptr<void> data; ///< The data.

    [[nodiscard]] inline void* GetPointerToPixel(std::uint32_t x, std::uint32_t y, std::uint32_t z) const
    {
        return static_cast<char*>(this->data.get()) +
            static_cast<size_t>(z) * this->info.stride_plane +
            static_cast<size_t>(y) * this->info.stride_line +
            static_cast<size_t>(x) * libCZI::Utils::GetBytesPerPixel(this->info.pixelType);
    }

    [[nodiscard]] inline const void* GetConstPointerToPixel(std::uint32_t x, std::uint32_t y, std::uint32_t z) const
    {
        return static_cast<const char*>(this->data.get()) +
            static_cast<size_t>(z) * this->info.stride_plane +
            static_cast<size_t>(y) * this->info.stride_line +
            static_cast<size_t>(x) * libCZI::Utils::GetBytesPerPixel(this->info.pixelType);
    }
};
