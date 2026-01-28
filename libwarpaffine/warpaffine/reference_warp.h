// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <Eigen/Eigen>
#include <limits>
#include <algorithm>
#include "../brick.h"
#include "../geotypes.h"
#include "../cmdlineoptions_enums.h"

class ReferenceWarp
{
private:
    const Brick& source_brick_;
    const Brick& destination_brick_;
    Eigen::Matrix4d transformation_;
    Eigen::Matrix4d transformation_inverse_;
    Interpolation interpolation_{ 1 };    // 1 -> NN, 2 -> linear, other types are not supported
public:
    ReferenceWarp(const Brick& source_brick, const Brick& destination_brick) :
        source_brick_(source_brick), destination_brick_(destination_brick)
    {
    }

    void SetTransformation(const Eigen::Matrix4d& transformation);
    void SetInterpolation(Interpolation interpolation);

    void Do();
private:
    void DoNearestNeighbor();
    void DoLinearInterpolation();
    [[noreturn]] void ThrowUnsupportedPixelType();

    static IntPos3 ToNearestNeighbor(const Eigen::Vector4d& position);

    enum class PixelPosition
    {
        kInside,
        kOnePixelOutside,
        kOutside,
    };

    static inline bool IsInsideBrickForTriLinear(const BrickInfo& brick_info, const DoublePos3& position)
    {
        if (position.x_position < -1 || position.x_position >= brick_info.width /* - 1*/ ||
            position.y_position < -1 || position.y_position >= brick_info.height/* - 1 */ ||
            position.z_position < -1 || position.z_position >= brick_info.depth /*- 1*/)
        {
            return false;
        }

        return true;
    }

    static inline PixelPosition GetPixelPositionForTriLinear(const BrickInfo& brick_info, const DoublePos3& position)
    {
        if (position.x_position < 0 || position.x_position >= brick_info.width - 1 ||
            position.y_position < 0 || position.y_position >= brick_info.height - 1 ||
            position.z_position < 0 || position.z_position >= brick_info.depth - 1)
        {
            if (position.x_position >= -1 && position.x_position <= brick_info.width &&
                position.y_position >= -1 && position.y_position <= brick_info.height &&
                position.z_position >= -1 && position.z_position <= brick_info.depth)
            {
                return PixelPosition::kOnePixelOutside;
            }

            return PixelPosition::kOutside;
        }

        return PixelPosition::kInside;
    }

    static inline bool IsInsideBrick(const BrickInfo& brick_info, const IntPos3& position)
    {
        if (position.x_position <= -1 || position.x_position >= brick_info.width ||
            position.y_position <= -1 || position.y_position >= brick_info.height ||
            position.z_position <= -1 || position.z_position >= brick_info.depth)
        {
            return false;
        }

        return true;
    }

    template <typename t>
    static inline void NearestNeighborWarp(const Brick& source_brick, const Brick& destination_brick, const Eigen::Matrix4d& transformation_inverse)
    {
        for (uint32_t z = 0; z < destination_brick.info.depth; ++z)
        {
            for (uint32_t y = 0; y < destination_brick.info.height; ++y)
            {
                for (uint32_t x = 0; x < destination_brick.info.width; ++x)
                {
                    Eigen::Vector4d position_in_destination;
                    position_in_destination << x, y, z, 1;
                    const auto source_point = transformation_inverse * position_in_destination;

                    t* dest_pixel = static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z));

                    // now, check whether this point is within the source
                    IntPos3 source_point_nearest_neighbor = ToNearestNeighbor(source_point);

                    if (IsInsideBrick(source_brick.info, source_point_nearest_neighbor))
                    {
                        *dest_pixel = *static_cast<const t*>(source_brick.GetConstPointerToPixel(
                            source_point_nearest_neighbor.x_position,
                            source_point_nearest_neighbor.y_position,
                            source_point_nearest_neighbor.z_position));
                    }
                    else
                    {
                        *dest_pixel = 0;
                    }
                }
            }
        }
    }

    template <typename t>
    static inline t SampleWithLinearInterpolation(const Brick& brick, const DoublePos3& position)
    {
        // -> https://en.wikipedia.org/wiki/Trilinear_interpolation
        double dummy;
        const double xd = modf(position.x_position, &dummy);
        const double yd = modf(position.y_position, &dummy);
        const double zd = modf(position.z_position, &dummy);

        const IntPos3 position_rounded_down =
        {
            static_cast<int>(position.x_position),
            static_cast<int>(position.y_position),
            static_cast<int>(position.z_position)
        };

        const t c000 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position, position_rounded_down.z_position));
        const t c100 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position, position_rounded_down.z_position));
        const t c010 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position + 1, position_rounded_down.z_position));
        const t c110 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position + 1, position_rounded_down.z_position));
        const t c001 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position, position_rounded_down.z_position + 1));
        const t c101 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position, position_rounded_down.z_position + 1));
        const t c011 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position + 1, position_rounded_down.z_position + 1));
        const t c111 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position + 1, position_rounded_down.z_position + 1));

        const double c00 = c000 * (1 - xd) + c100 * xd;
        const double c01 = c001 * (1 - xd) + c101 * xd;
        const double c10 = c010 * (1 - xd) + c110 * xd;
        const double c11 = c011 * (1 - xd) + c111 * xd;

        const double c0 = c00 * (1 - yd) + c10 * yd;
        const double c1 = c01 * (1 - yd) + c11 * yd;

        const double c = c0 * (1 - zd) + c1 * zd;

        return (c < 0) ? 0 : c > static_cast<double>(std::numeric_limits<t>::max()) ? std::numeric_limits<t>::max() : static_cast<t>(lround(c));
    }

    template <typename t>
    static inline t SampleWithLinearInterpolationOutsideOfVolume(const Brick& brick, const DoublePos3& position)
    {
        // -> https://en.wikipedia.org/wiki/Trilinear_interpolation
        double dummy;
        const double xd = modf(position.x_position, &dummy);
        const double yd = modf(position.y_position, &dummy);
        const double zd = modf(position.z_position, &dummy);

        const IntPos3 position_rounded_down =
        {
            static_cast<int>(floor(position.x_position)),
            static_cast<int>(floor(position.y_position)),
            static_cast<int>(floor(position.z_position)),
        };

        int x_position_to_sample = std::max(position_rounded_down.x_position, 0);
        int x_position_plus_one_to_sample = std::min(position_rounded_down.x_position + 1, (int)(brick.info.width - 1));
        int y_position_to_sample = std::max(position_rounded_down.y_position, 0);
        int y_position_plus_one_to_sample = std::min(position_rounded_down.y_position + 1, (int)(brick.info.height - 1));
        int z_position_to_sample = std::max(position_rounded_down.z_position, 0);
        int z_position_plus_one_to_sample = std::min(position_rounded_down.z_position + 1, (int)(brick.info.depth - 1));

        const t c000 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_to_sample, y_position_to_sample, z_position_to_sample));
        const t c100 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_plus_one_to_sample, y_position_to_sample, z_position_to_sample));
        const t c010 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_to_sample, y_position_plus_one_to_sample, z_position_to_sample));
        const t c110 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_plus_one_to_sample, y_position_plus_one_to_sample, z_position_to_sample));
        const t c001 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_to_sample, y_position_to_sample, z_position_plus_one_to_sample));
        const t c101 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_plus_one_to_sample, y_position_to_sample, z_position_plus_one_to_sample));
        const t c011 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_to_sample, y_position_plus_one_to_sample, z_position_plus_one_to_sample));
        const t c111 = *static_cast<const t*>(brick.GetConstPointerToPixel(x_position_plus_one_to_sample, y_position_plus_one_to_sample, z_position_plus_one_to_sample));

        const double c00 = c000 * (1 - xd) + c100 * xd;
        const double c01 = c001 * (1 - xd) + c101 * xd;
        const double c10 = c010 * (1 - xd) + c110 * xd;
        const double c11 = c011 * (1 - xd) + c111 * xd;

        const double c0 = c00 * (1 - yd) + c10 * yd;
        const double c1 = c01 * (1 - yd) + c11 * yd;

        const double c = c0 * (1 - zd) + c1 * zd;

        return (c < 0) ? 0 : c > static_cast<double>(std::numeric_limits<t>::max()) ? std::numeric_limits<t>::max() : static_cast<t>(lround(c));
    }

    /*template <typename t>
    static inline void TriLinearWarp(const Brick& source_brick, const Brick& destination_brick, const Eigen::Matrix4d& transformation_inverse)
    {
        for (uint32_t z = 0; z < destination_brick.info.depth; ++z)
        {
            for (uint32_t y = 0; y < destination_brick.info.height; ++y)
            {
                for (uint32_t x = 0; x < destination_brick.info.width; ++x)
                {
                    Eigen::Vector4d position_in_destination;
                    position_in_destination << x, y, z, 1;
                    const auto source_point = transformation_inverse * position_in_destination;

                    t* dest_pixel = static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z));

                    // now, check whether this point is within the source
                    DoublePos3 source_point3 = { source_point[0], source_point[1], source_point[2] };
                    switch (ReferenceWarp::GetPixelPositionForTriLinear(source_brick.info, source_point3))
                    {
                        case PixelPosition::kInside:
                        {
                            const t source_pixel_linear_interpolated = SampleWithLinearInterpolation<t>(source_brick, source_point3);
                            *dest_pixel = source_pixel_linear_interpolated;
                            break;
                        }
                        case PixelPosition::kOnePixelOutside:
                        {
                            const t source_pixel_linear_interpolated = SampleWithLinearInterpolationOutsideOfVolume<t>(source_brick, source_point3);
                            *dest_pixel = source_pixel_linear_interpolated;
                            break;
                        }
                        case PixelPosition::kOutside:
                        {
                            *dest_pixel = 0;
                            break;
                        }
                    }
                }
            }
        }
    }*/

    /// Computes the range of x-values in destination space where a source coordinate falls within 
/// specified bounds. For an affine transformation, each source coordinate is a linear function 
/// of the destination x-coordinate: source_coord = base + coeff * x. This function solves for 
/// the x-range where: lo <= base + coeff * x < hi.
/// \param base   The source coordinate value when x=0 (precomputed from y, z, and translation).
/// \param coeff  The rate of change of source coordinate per unit x (from transformation matrix).
/// \param lo     Lower bound (inclusive) for the source coordinate.
/// \param hi     Upper bound (exclusive) for the source coordinate.
/// \param max_x  Maximum destination x-value (destination width).
/// \returns A pair (start, end) representing the inclusive x-range. If start > end, the range is empty.
    static inline std::pair<int, int> ComputeXRange(double base, double coeff, double lo, double hi, int max_x)
    {
        if (std::abs(coeff) < 1e-12)
        {
            return (base >= lo && base < hi) ? std::make_pair(0, max_x - 1) : std::make_pair(0, -1);
        }

        double x0 = (lo - base) / coeff;
        double x1 = (hi - base) / coeff;
        if (coeff < 0) std::swap(x0, x1);

        return { std::max(0, static_cast<int>(std::ceil(x0))),
                 std::min(max_x - 1, static_cast<int>(std::ceil(x1)) - 1) };
    }

    /// Computes the intersection of three x-ranges. Used to find x-values where all three source 
    /// coordinates (x, y, z) simultaneously satisfy their respective bounds.
    /// \param a First range (typically from x-coordinate constraint).
    /// \param b Second range (typically from y-coordinate constraint).
    /// \param c Third range (typically from z-coordinate constraint).
    /// \returns The intersection range. Empty (first > second) if no overlap exists.
    static inline std::pair<int, int> IntersectRanges(std::pair<int, int> a, std::pair<int, int> b, std::pair<int, int> c)
    {
        return { std::max({a.first, b.first, c.first}), std::min({a.second, b.second, c.second}) };
    }

    /// Performs trilinear interpolation warp with optimized scanline processing.
    /// 
    /// The naive approach checks bounds for every destination pixel, which is expensive. This 
    /// optimized version exploits the linearity of affine transformations: for a fixed (y, z) 
    /// scanline, the source position varies linearly with x. This allows precomputing the x-ranges 
    /// where the source falls into each category (inside, border, outside).
    /// 
    /// Each scanline is divided into up to 5 contiguous regions:
    ///   - outside (zeros) | border (clamped) | inside (fast) | border (clamped) | outside (zeros)
    /// 
    /// The "inside" region is the hot path where sampling requires no bounds checking.
    /// 
    /// \tparam t      Pixel type (e.g., uint8_t, uint16_t, float).
    /// \param source_brick           Source volume to sample from.
    /// \param destination_brick      Destination volume to write to.
    /// \param transformation_inverse Inverse of the affine transformation matrix (4x4 homogeneous).
    template <typename t>
    static inline void TriLinearWarp(const Brick& source_brick, const Brick& destination_brick, const Eigen::Matrix4d& transformation_inverse)
    {
        // Extract transformation matrix coefficients. The inverse transformation maps 
        // destination coords to source coords.
        const double a00 = transformation_inverse(0, 0), a01 = transformation_inverse(0, 1), a02 = transformation_inverse(0, 2), a03 = transformation_inverse(0, 3);
        const double a10 = transformation_inverse(1, 0), a11 = transformation_inverse(1, 1), a12 = transformation_inverse(1, 2), a13 = transformation_inverse(1, 3);
        const double a20 = transformation_inverse(2, 0), a21 = transformation_inverse(2, 1), a22 = transformation_inverse(2, 2), a23 = transformation_inverse(2, 3);

        const double sw = static_cast<double>(source_brick.info.width);
        const double sh = static_cast<double>(source_brick.info.height);
        const double sd = static_cast<double>(source_brick.info.depth);
        const int dw = static_cast<int>(destination_brick.info.width);

        for (uint32_t z = 0; z < destination_brick.info.depth; ++z)
        {
            for (uint32_t y = 0; y < destination_brick.info.height; ++y)
            {
                // Base source position for this scanline (at x=0): source = base + coeff * x
                const double bx = a01 * y + a02 * z + a03;
                const double by = a11 * y + a12 * z + a13;
                const double bz = a21 * y + a22 * z + a23;

                // Inside range: 0 <= source < dim-1 (all 8 neighbors are valid)
                auto inside = IntersectRanges(
                    ComputeXRange(bx, a00, 0, sw - 1, dw),
                    ComputeXRange(by, a10, 0, sh - 1, dw),
                    ComputeXRange(bz, a20, 0, sd - 1, dw));

                // Border range: -1 <= source <= dim (within reach of clamped sampling)
                auto border = IntersectRanges(
                    ComputeXRange(bx, a00, -1, sw + 1, dw),
                    ComputeXRange(by, a10, -1, sh + 1, dw),
                    ComputeXRange(bz, a20, -1, sd + 1, dw));

                int x = 0;

                // Region 1: outside (before border) - output zeros
                for (; x < border.first && x < dw; ++x)
                    *static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z)) = 0;

                // Region 2: border (before inside) - clamped sampling
                for (; x < inside.first && x <= border.second; ++x)
                {
                    DoublePos3 sp = { bx + a00 * x, by + a10 * x, bz + a20 * x };
                    *static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z)) =
                        SampleWithLinearInterpolationOutsideOfVolume<t>(source_brick, sp);
                }

                // Region 3: inside - fast path, no bounds checking
                for (; x <= inside.second; ++x)
                {
                    DoublePos3 sp = { bx + a00 * x, by + a10 * x, bz + a20 * x };
                    *static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z)) =
                        SampleWithLinearInterpolation<t>(source_brick, sp);
                }

                // Region 4: border (after inside) - clamped sampling
                for (; x <= border.second; ++x)
                {
                    DoublePos3 sp = { bx + a00 * x, by + a10 * x, bz + a20 * x };
                    *static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z)) =
                        SampleWithLinearInterpolationOutsideOfVolume<t>(source_brick, sp);
                }

                // Region 5: outside (after border) - output zeros
                for (; x < dw; ++x)
                    *static_cast<t*>(destination_brick.GetPointerToPixel(x, y, z)) = 0;
            }
        }
    }
};
