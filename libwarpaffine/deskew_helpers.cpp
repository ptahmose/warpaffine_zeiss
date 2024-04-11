// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "deskew_helpers.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

using namespace Eigen;
using namespace std;

/*static*/void DeskewHelpers::CalculateAxisAlignedBoundingBox(
    double width, double height, double depth,
    const Eigen::Matrix4d& transformation,
    Eigen::Vector3d& edge_point,
    Eigen::Vector3d& extent)
{
    // what we do is:
    // - we transform all the 8 edges of the quad with the transformation
    // - then, we calculate the min and max of the coordinates
    // - and, voilà, there is our axis aligned bounding box
    const Vector4d p1(0, 0, 0, 1);
    const Vector4d p2(width, 0, 0 , 1);
    const Vector4d p3(width, height, 0 , 1);
    const Vector4d p4(0, height, 0, 1);
    const Vector4d p5(0, 0, depth, 1);
    const Vector4d p6(width, 0, depth, 1);
    const Vector4d p7(width, height, depth, 1);
    const Vector4d p8(0, height, depth, 1);

    Vector4d p1_transformed = transformation * p1;
    Vector4d p2_transformed = transformation * p2;
    Vector4d p3_transformed = transformation * p3;
    Vector4d p4_transformed = transformation * p4;
    Vector4d p5_transformed = transformation * p5;
    Vector4d p6_transformed = transformation * p6;
    Vector4d p7_transformed = transformation * p7;
    Vector4d p8_transformed = transformation * p8;

    const double x_min = min(min(min(min(min(min(min(p1_transformed[0], p2_transformed[0]), p3_transformed[0]), p4_transformed[0]), p5_transformed[0]), p6_transformed[0]), p7_transformed[0]), p8_transformed[0]);
    const double x_max = max(max(max(max(max(max(max(p1_transformed[0], p2_transformed[0]), p3_transformed[0]), p4_transformed[0]), p5_transformed[0]), p6_transformed[0]), p7_transformed[0]), p8_transformed[0]);
    const double y_min = min(min(min(min(min(min(min(p1_transformed[1], p2_transformed[1]), p3_transformed[1]), p4_transformed[1]), p5_transformed[1]), p6_transformed[1]), p7_transformed[1]), p8_transformed[1]);
    const double y_max = max(max(max(max(max(max(max(p1_transformed[1], p2_transformed[1]), p3_transformed[1]), p4_transformed[1]), p5_transformed[1]), p6_transformed[1]), p7_transformed[1]), p8_transformed[1]);
    const double z_min = min(min(min(min(min(min(min(p1_transformed[2], p2_transformed[2]), p3_transformed[2]), p4_transformed[2]), p5_transformed[2]), p6_transformed[2]), p7_transformed[2]), p8_transformed[2]);
    const double z_max = max(max(max(max(max(max(max(p1_transformed[2], p2_transformed[2]), p3_transformed[2]), p4_transformed[2]), p5_transformed[2]), p6_transformed[2]), p7_transformed[2]), p8_transformed[2]);

    edge_point = { x_min, y_min, z_min };
    extent = { x_max - x_min, y_max - y_min, z_max - z_min };
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetTransformationMatrixSoThatEdgePointIsAtOrigin(OperationType type, const DeskewDocumentInfo& document_info)
{
    const auto matrix = DeskewHelpers::GetTransformationMatrix(type, document_info);

    // now, calculate the AABB of the transformed cube - we take a random stack (the first one) and use its size, making the assumption that all stacks have the same size.
    // TODO(JBL): I suppose this calculation should be done "per tile" (i.e. per stack) and not once for the whole document.
    const auto& brick_rect_position_info_of_random_tile = document_info.map_brickid_position.cbegin()->second;
    Eigen::Vector3d edge_point; Eigen::Vector3d extent;
    DeskewHelpers::CalculateAxisAlignedBoundingBox(brick_rect_position_info_of_random_tile.width, brick_rect_position_info_of_random_tile.height, document_info.depth, matrix, edge_point, extent);

    // and add a translation (at the *end* of the transformation-chain!) to move the edge to the origin
    const Matrix4d translation = GetTranslationMatrix(-edge_point(0), -edge_point(1), -edge_point(2));

    return translation * matrix;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetTransformationMatrix(OperationType type, const DeskewDocumentInfo& document_info)
{
    switch (type)
    {
    case OperationType::Identity:
        return Matrix4d::Identity(4, 4);
    case OperationType::Deskew:
        return GetTransformationMatrix_Deskew(document_info);
    case OperationType::CoverGlassTransform:
        return GetTransformationMatrix_CoverglassTransform(document_info, false);
    case OperationType::CoverGlassTransformAndXYRotated:
        return GetTransformationMatrix_CoverglassTransform(document_info, true);
    default:
        throw invalid_argument("unknown operation-type");
    }
}

/*static*/double DeskewHelpers::DegreesToRadians(double angle_in_degrees)
{
    return angle_in_degrees / 180 * M_PI;
}

/*static*/double DeskewHelpers::RadiansToDegrees(double angle_in_radians)
{
    return angle_in_radians / M_PI * 180;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetScalingMatrix(double x_scale, double y_scale, double z_scale)
{
    Matrix4d m;
    m << x_scale, 0, 0, 0, 0, y_scale, 0, 0, 0, 0, z_scale, 0, 0, 0, 0, 1;
    return m;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetTranslationMatrix(double x, double y, double z)
{
    Matrix4d m;
    m << 1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1;
    return m;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetRotationAroundXAxis(double angle_in_radians)
{
    double cosine = cos(angle_in_radians);
    double sine = sin(angle_in_radians);

    // https://en.wikipedia.org/wiki/Rotation_matrix
    Matrix4d m;
    m << 1, 0, 0, 0, 0, cosine, -sine, 0, 0, sine, cosine, 0, 0, 0, 0, 1;
    return m;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetRotationAroundZAxis(double angle_in_radians)
{
    double cosine = cos(angle_in_radians);
    double sine = sin(angle_in_radians);

    // https://en.wikipedia.org/wiki/Rotation_matrix
    Matrix4d m;
    m << cosine, -sine, 0, 0, sine, cosine, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
    return m;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetTransformationMatrix_Deskew(const DeskewDocumentInfo& document_info)
{
    /*

    The images are arranged like this:


|   |--\    x    x    x    x
    |   -- x    x    x    x
    |  α  x    x    x    x
    |    x    x    x    x
    |   x    x    x    x
    |  x    x    x    x
    | x    x    x    x
    |x    x    x    x

    with an angle of α = 60° to the vertical.

    */

    // TODO(Jbl) : I am not sure where this factor of 0.5 comes from (=cos(60°)), or - the z-spacing
    //              of the source-file is telling us by how much the hardware moved, not how far the z-slices
    //              are apart from one another I suppose
    double b = tan(DegreesToRadians(60)) * (document_info.z_scaling * cos(DegreesToRadians(60)));

    double b_in_pixels = b / document_info.xy_scaling;

    double total_skew = b_in_pixels * (document_info.depth - 1);

    Matrix4d matrix_skew;
    matrix_skew << 1, 0, 0, 0, 0, 1, b_in_pixels, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    return matrix_skew;
}

/*static*/Eigen::Matrix4d DeskewHelpers::GetTransformationMatrix_CoverglassTransform(const DeskewDocumentInfo& document_info, bool rotate_around_z_axis_by_90_degree)
{
    // The shearing matrix gives us something like:
    //
    // z |
    // |     xxxxxxxxxxx          z |
    // |    xxxxxxxxxxx             |
    // |   xxxxxxxxxxx              |  /y
    // |  xxxxxxxxxxx               | /
    // | xxxxxxxxxxx                |/
    // |xxxxxxxxxxx                 -------> x
    // --------------> y

    const auto shearing_matrix = GetTransformationMatrix_Deskew(document_info);

    double factor_to_scale_z = (document_info.z_scaling / 2) / document_info.xy_scaling;
    Matrix4d scaling_matrix;
    scaling_matrix << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, factor_to_scale_z, 0, 0, 0, 0, 1;


    const double b = tan(DegreesToRadians(60)) * (document_info.z_scaling * cos(DegreesToRadians(60)));
    const double b_in_pixels = b / document_info.xy_scaling;
    const double total_skew = b_in_pixels * (document_info.depth - 1);

    const double angle = RadiansToDegrees(atan(total_skew / (document_info.depth * factor_to_scale_z)));

    const Matrix4d flip_y = GetTranslationMatrix(0, document_info.height / 2.0, 0) * GetScalingMatrix(1, -1, 1) * GetTranslationMatrix(0, -document_info.height / 2.0, 0);
    const Matrix4d flip_z = GetTranslationMatrix(0, 0, document_info.depth / 2.0) * GetScalingMatrix(1, 1, -1) * GetTranslationMatrix(0, 0, -document_info.depth / 2.0);
    const Matrix4d flip = flip_z * flip_y;

    const auto rotation_around_x_axis = GetRotationAroundXAxis(DegreesToRadians(+angle + 90));

    // so, we have the following operations
    // 1. do mirroring on the x, y and z (determined experimentally to match ZEN's result)
    // 2. then, do the shearing
    // 3. next, scale it so that scaling in x/y is the same as in z
    // 4. and, finally rotate it (around x-axis)
    Matrix4d m = rotation_around_x_axis * scaling_matrix * shearing_matrix * flip;

    if (rotate_around_z_axis_by_90_degree)
    {
        // this is corresponding to the ZEN-option "XyTargetIsRotatedBy90":
        //  we rotate around the z-axis by 90 degree (as the final step)
        m = GetRotationAroundZAxis(DegreesToRadians(90)) * m;
    }

    return m;
}

/*static*/std::array<DoublePos3, 8> DeskewHelpers::TransformEdgePointOfAabb(
    const IntCuboid& cuboid,
    const Eigen::Matrix4d& transformation)
{
    Vector4d p1;
    p1 << cuboid.x_position, cuboid.y_position, cuboid.z_position, 1;
    Vector4d p2;
    p2 << cuboid.x_position + cuboid.width, cuboid.y_position, cuboid.z_position, 1;
    Vector4d p3;
    p3 << cuboid.x_position + cuboid.width, cuboid.y_position + cuboid.height, cuboid.z_position, 1;
    Vector4d p4;
    p4 << cuboid.x_position, cuboid.y_position + cuboid.height, cuboid.z_position, 1;
    Vector4d p5;
    p5 << cuboid.x_position, cuboid.y_position, cuboid.z_position + cuboid.depth, 1;
    Vector4d p6;
    p6 << cuboid.x_position + cuboid.width, cuboid.y_position, cuboid.z_position + cuboid.depth, 1;
    Vector4d p7;
    p7 << cuboid.x_position + cuboid.width, cuboid.y_position + cuboid.height, cuboid.z_position + cuboid.depth, 1;
    Vector4d p8;
    p8 << cuboid.x_position, cuboid.y_position + cuboid.height, cuboid.z_position + cuboid.depth, 1;

    Vector4d p1_transformed = transformation * p1;
    Vector4d p2_transformed = transformation * p2;
    Vector4d p3_transformed = transformation * p3;
    Vector4d p4_transformed = transformation * p4;
    Vector4d p5_transformed = transformation * p5;
    Vector4d p6_transformed = transformation * p6;
    Vector4d p7_transformed = transformation * p7;
    Vector4d p8_transformed = transformation * p8;

    std::array<DoublePos3, 8> result;
    result[0] = DoublePos3{ p1_transformed[0], p1_transformed[1], p1_transformed[2] };
    result[1] = DoublePos3{ p2_transformed[0], p2_transformed[1], p2_transformed[2] };
    result[2] = DoublePos3{ p3_transformed[0], p3_transformed[1], p3_transformed[2] };
    result[3] = DoublePos3{ p4_transformed[0], p4_transformed[1], p4_transformed[2] };
    result[4] = DoublePos3{ p5_transformed[0], p5_transformed[1], p5_transformed[2] };
    result[5] = DoublePos3{ p6_transformed[0], p6_transformed[1], p6_transformed[2] };
    result[6] = DoublePos3{ p7_transformed[0], p7_transformed[1], p7_transformed[2] };
    result[7] = DoublePos3{ p8_transformed[0], p8_transformed[1], p8_transformed[2] };
    return result;                                                
}

/*static*/std::array<DoublePos3, 8> DeskewHelpers::TransformEdgePointOfAabb(
    const DoubleCuboid& cuboid,
    const Eigen::Matrix4d& transformation)
{
    Vector4d p1;
    p1 << cuboid.x_position, cuboid.y_position, cuboid.z_position, 1;
    Vector4d p2;
    p2 << cuboid.x_position + cuboid.width-1, cuboid.y_position, cuboid.z_position, 1;
    Vector4d p3;
    p3 << cuboid.x_position + cuboid.width-1, cuboid.y_position + cuboid.height-1, cuboid.z_position, 1;
    Vector4d p4;
    p4 << cuboid.x_position, cuboid.y_position + cuboid.height-1, cuboid.z_position, 1;
    Vector4d p5;
    p5 << cuboid.x_position, cuboid.y_position, cuboid.z_position + cuboid.depth-1, 1;
    Vector4d p6;
    p6 << cuboid.x_position + cuboid.width-1, cuboid.y_position, cuboid.z_position + cuboid.depth-1, 1;
    Vector4d p7;
    p7 << cuboid.x_position + cuboid.width-1, cuboid.y_position + cuboid.height-1, cuboid.z_position + cuboid.depth-1, 1;
    Vector4d p8;
    p8 << cuboid.x_position, cuboid.y_position + cuboid.height-1, cuboid.z_position + cuboid.depth-1, 1;

    Vector4d p1_transformed = transformation * p1;
    Vector4d p2_transformed = transformation * p2;
    Vector4d p3_transformed = transformation * p3;
    Vector4d p4_transformed = transformation * p4;
    Vector4d p5_transformed = transformation * p5;
    Vector4d p6_transformed = transformation * p6;
    Vector4d p7_transformed = transformation * p7;
    Vector4d p8_transformed = transformation * p8;

    std::array<DoublePos3, 8> result;
    result[0] = DoublePos3{ p1_transformed[0], p1_transformed[1], p1_transformed[2] };
    result[1] = DoublePos3{ p2_transformed[0], p2_transformed[1], p2_transformed[2] };
    result[2] = DoublePos3{ p3_transformed[0], p3_transformed[1], p3_transformed[2] };
    result[3] = DoublePos3{ p4_transformed[0], p4_transformed[1], p4_transformed[2] };
    result[4] = DoublePos3{ p5_transformed[0], p5_transformed[1], p5_transformed[2] };
    result[5] = DoublePos3{ p6_transformed[0], p6_transformed[1], p6_transformed[2] };
    result[6] = DoublePos3{ p7_transformed[0], p7_transformed[1], p7_transformed[2] };
    result[7] = DoublePos3{ p8_transformed[0], p8_transformed[1], p8_transformed[2] };
    return result;
}

/*static*/IntCuboid DeskewHelpers::FromFloatCuboid(const DoubleCuboid& float_cuboid)
{
    IntCuboid integer_cuboid;
    integer_cuboid.x_position = lrint(floor(float_cuboid.x_position));
    integer_cuboid.y_position = lrint(floor(float_cuboid.y_position));
    integer_cuboid.z_position = lrint(floor(float_cuboid.z_position));
    integer_cuboid.width = static_cast<std::uint32_t>(llrint(ceil((float_cuboid.x_position - integer_cuboid.x_position) + float_cuboid.width)));
    integer_cuboid.height = static_cast<std::uint32_t>(llrint(ceil((float_cuboid.y_position - integer_cuboid.y_position) + float_cuboid.height)));
    integer_cuboid.depth = static_cast<std::uint32_t>(llrint(ceil((float_cuboid.z_position - integer_cuboid.z_position) + float_cuboid.depth)));
    return integer_cuboid;
}
