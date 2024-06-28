// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

/* This file contains some definitions of basic structures, mostly describing geometric properties like position, extent etc. */

#include <cstdint>
#include <algorithm>
#include <optional>

/// A point with integer coordinates in 3D.
struct IntPos3
{
    int     x_position{ 0 };    ///< The x-coordinate.
    int     y_position{ 0 };    ///< The y-coordinate.
    int     z_position{ 0 };    ///< The z-coordinate.
};

/// A point with coordinates in double precision in 3D.
struct DoublePos3
{
    double    x_position{ 0 };  ///< The x-coordinate.
    double    y_position{ 0 };  ///< The y-coordinate.
    double    z_position{ 0 };  ///< The z-coordinate.
};

/// This structure gives the extent of a cuboid (with integer values).
struct IntSize3
{
    std::uint32_t width{ 0 };   ///< The length in x-direction (width).
    std::uint32_t height{ 0 };  ///< The length in y-direction (height).
    std::uint32_t depth{ 0 };   ///< The length in z-direction (depth).
};

/// This structure describes an axis-aligned cuboid in 3D, giving its edge-point and its extent.
/// It uses integer values.
struct IntCuboid
{
    /// Default constructor initializing all properties to their default value of zero.
    IntCuboid() = default;

    /// Constructor initializing all properties.
    /// \param  x   The x-position of the edge of the cuboid.
    /// \param  y   The y-position of the edge of the cuboid.
    /// \param  z   The z-position of the edge of the cuboid.
    /// \param  w   The length in x-direction (width).
    /// \param  h   The length in y-direction (height).
    /// \param  d   The length in z-direction (depth).
    IntCuboid(int x, int y, int z, std::uint32_t w, std::uint32_t h, std::uint32_t d)
        : x_position(x), y_position(y), z_position(z), width(w), height(h), depth(d)
    {}

    int     x_position{ 0 };    ///< The x-position of the edge of the cuboid.
    int     y_position{ 0 };    ///< The y-position of the edge of the cuboid.
    int     z_position{ 0 };    ///< The z-position of the edge of the cuboid.
    std::uint32_t width{ 0 };   ///< The length in x-direction (width).
    std::uint32_t height{ 0 };  ///< The length in y-direction (height).
    std::uint32_t depth{ 0 };   ///< The length in z-direction (depth).

    /// Calculates the intersection with another cuboid. If the two cuboids do not intersect, then an
    /// empty cuboid is returned.
    /// \param  other   The other cuboid.
    /// \returns    The intersection of the two cuboids.
    inline IntCuboid GetIntersectionWith(const IntCuboid& other) const
    {
        const int x1 = (std::max)(this->x_position, other.x_position);
        const std::int64_t x2 = (std::min)(static_cast<std::int64_t>(this->x_position) + this->width, static_cast<std::int64_t>(other.x_position) + other.width);
        const int y1 = (std::max)(this->y_position, other.y_position);
        const std::int64_t y2 = (std::min)(static_cast<std::int64_t>(this->y_position) + this->height, static_cast<std::int64_t>(other.y_position) + other.height);
        const int z1 = (std::max)(this->z_position, other.z_position);
        const std::int64_t z2 = (std::min)(static_cast<std::int64_t>(this->z_position) + this->depth, static_cast<std::int64_t>(other.z_position) + other.depth);

        if (x2 >= x1 && y2 >= y1 && z2 >= z1)
        {
            return IntCuboid{ x1, y1, z1, static_cast<uint32_t>(x2 - x1), static_cast<uint32_t>(y2 - y1), static_cast<uint32_t>(z2 - z1) };
        }

        return IntCuboid{};
    }

    /// Query if this cuboid is empty (has zero volume).
    /// \returns    True if the cuboid is empty, false if not.
    inline bool IsEmpty() const
    { 
        return this->width == 0 || this->height == 0 || this->depth == 0; 
    }
};

/// This structure describes an axis-aligned cuboid in 3D, giving its edge-point and its extent.
/// It uses double precision values.
struct DoubleCuboid
{
    double x_position{ 0 }; ///< The x-position of the edge of the cuboid.
    double y_position{ 0 }; ///< The y-position of the edge of the cuboid.
    double z_position{ 0 }; ///< The z-position of the edge of the cuboid.
    double width{ 0 };      ///< The length in x-direction (width).
    double height{ 0 };     ///< The length in y-direction (height).
    double depth{ 0 };      ///< The length in z-direction (depth).
};

/// This structure describes a subblock's x- and y-position together with its M-index and scene-index
/// (where both M- and scene-index may be invalid).
struct SubblockXYM
{
    int x_position{ 0 };
    int y_position{ 0 };
    std::optional<int> m_index;         ///< The m-index of the subblock.
    std::optional<int> scene_index;     ///< The scene-index of the subblock.
};
