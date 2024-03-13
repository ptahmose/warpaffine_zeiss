// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <Eigen/Eigen>
#include <limits>
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
    {}

    void SetTransformation(const Eigen::Matrix4d& transformation);
    void SetInterpolation(Interpolation interpolation);

    void Do();
private:
    void DoNearestNeighbor();
    void DoLinearInterpolation();
    void ThrowUnsupportedPixelType();

    static IntPos3 ToNearestNeighbor(const Eigen::Vector4d& position);

    static inline bool IsInsideBrickForTriLinear(const BrickInfo& brick_info, const DoublePos3& position)
    {
        if (position.x_position < 0 || position.x_position >= brick_info.width - 1 ||
            position.y_position < 0 || position.y_position >= brick_info.height - 1 ||
            position.z_position < 0 || position.z_position >= brick_info.depth - 1)
        {
            return false;
        }

        return true;
    }

    static inline bool IsInsideBrick(const BrickInfo& brick_info, const IntPos3& position)
    {
        if (position.x_position < 0 || position.x_position >= brick_info.width ||
            position.y_position < 0 || position.y_position >= brick_info.height ||
            position.z_position < 0 || position.z_position >= brick_info.depth)
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

        const IntPos3 position_rounded_down = { static_cast<int>(position.x_position), static_cast<int>(position.y_position), static_cast<int>(position.z_position) };
        t c000 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position, position_rounded_down.z_position));
        t c100 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position, position_rounded_down.z_position));
        t c010 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position + 1, position_rounded_down.z_position));
        t c110 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position + 1, position_rounded_down.z_position));
        t c001 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position, position_rounded_down.z_position + 1));
        t c101 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position, position_rounded_down.z_position + 1));
        t c011 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position, position_rounded_down.y_position + 1, position_rounded_down.z_position + 1));
        t c111 = *static_cast<const t*>(brick.GetConstPointerToPixel(position_rounded_down.x_position + 1, position_rounded_down.y_position + 1, position_rounded_down.z_position + 1));

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
                    if (IsInsideBrickForTriLinear(source_brick.info, source_point3))
                    {
                        const t source_pixel_linear_interpolated = SampleWithLinearInterpolation<t>(source_brick, source_point3);
                        *dest_pixel = source_pixel_linear_interpolated;
                    }
                    else
                    {
                        *dest_pixel = 0;
                    }
                }
            }
        }
    }
};
