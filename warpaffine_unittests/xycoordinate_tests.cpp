// SPDX-FileCopyrightText: 2025 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include "utilities.h"

#include "../libwarpaffine/document_info.h"
#include "../libwarpaffine/deskew_helpers.h"

using namespace std;
using namespace libCZI;

TEST(XYCoordinateTests, TestCase1)
{
    DeskewDocumentInfo document_info;
    document_info.width = 3891;
    document_info.height = 3216;
    document_info.depth = 62;
    document_info.document_origin_x = 0;
    document_info.document_origin_y = 0;
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 0, 0 }, BrickRectPositionInfo{ 0, 2988, 2048, 228 }));
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 1, 0 }, BrickRectPositionInfo{ 1843, 2988, 2048, 228 }));
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 0, 1 }, BrickRectPositionInfo{ 136, 0, 2048, 228 }));
    document_info.map_channelindex_pixeltype[0] = PixelType::Gray16;
    document_info.map_channelindex_pixeltype[1] = PixelType::Gray16;
    document_info.z_scaling = 4e-07;
    document_info.xy_scaling = 1.4499219272808386e-07;

    Eigen::Matrix4d transformation_matrix = DeskewHelpers::GetTransformationMatrixSoThatEdgePointIsAtOrigin(OperationType::CoverGlassTransformAndXYRotated, document_info);

    // Note: Here we cheat by passing the coordinate of the point we know is going to be at the origin. This allows us
    //        to compare against zero-aligned coordinates.
    const auto projection_plane = DeskewHelpers::CalculateProjectionPlane(transformation_matrix, Eigen::Vector3d(0, 2988, 0));

    static array<tuple<Eigen::Vector2d, Eigen::Vector2d>, 3>  points =
    {
        make_tuple(Eigen::Vector2d(0, 2988), Eigen::Vector2d(0, 0)),
        make_tuple(Eigen::Vector2d(136, 0), Eigen::Vector2d(2988, 136)),
        make_tuple(Eigen::Vector2d(1843, 2988), Eigen::Vector2d(0, 1843)),
    };

    for (const auto& point : points)
    {
        // transform the X-Y-coordinates (from the sub-block)
        const Eigen::Vector4d p
        {
            static_cast<double>(get<0>(point).x() - document_info.document_origin_x),
            static_cast<double>(get<0>(point).y() - document_info.document_origin_y),
            0,
            1
        };
        const auto xy_transformed = (transformation_matrix * p).hnormalized();

        // project the point onto the projection plane
        const auto& transformed_and_projected_coordinate = DeskewHelpers::CalculateProjection(
                projection_plane,
                xy_transformed);
        EXPECT_NEAR(transformed_and_projected_coordinate.x(), get<1>(point).x(), 0.6);
        EXPECT_NEAR(transformed_and_projected_coordinate.y(), get<1>(point).y(), 0.6);
    }
}

TEST(XYCoordinateTests, TestCase2)
{
    DeskewDocumentInfo document_info;
    document_info.width = 7838;
    document_info.height = 2140;
    document_info.depth = 683;
    document_info.document_origin_x = 0;
    document_info.document_origin_y = 0;
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 0, 0 }, BrickRectPositionInfo{ 1557, 0, 2048, 300 }));
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 0, 1 }, BrickRectPositionInfo{ 0, 1840, 2048, 300 }));
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 0, 2 }, BrickRectPositionInfo{ 2103, 1547, 2048, 300 }));
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 1, 2 }, BrickRectPositionInfo{ 3946, 1547, 2048, 300 }));
    document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(BrickInPlaneIdentifier{ 2, 2 }, BrickRectPositionInfo{ 5790, 1547, 2048, 300 }));
    document_info.map_channelindex_pixeltype[0] = PixelType::Gray16;
    document_info.map_channelindex_pixeltype[1] = PixelType::Gray16;
    document_info.z_scaling = 2e-07;
    document_info.xy_scaling = 1.4499219272808386e-07;

    Eigen::Matrix4d transformation_matrix = DeskewHelpers::GetTransformationMatrixSoThatEdgePointIsAtOrigin(OperationType::CoverGlassTransformAndXYRotated, document_info);

    // Note: Here we cheat by passing the coordinate of the point we know is going to be at the origin. This allows us
    //        to compare against zero-aligned coordinates.
    const auto projection_plane = DeskewHelpers::CalculateProjectionPlane(transformation_matrix, Eigen::Vector3d(0, 1840, 0));

    static array<tuple<Eigen::Vector2d, Eigen::Vector2d>, 5>  points =
    {
        make_tuple(Eigen::Vector2d(0, 1840), Eigen::Vector2d(0, 0)),
        make_tuple(Eigen::Vector2d(1557, 0), Eigen::Vector2d(1840, 1557)),
        make_tuple(Eigen::Vector2d(2103, 1547), Eigen::Vector2d(293, 2103)),
        make_tuple(Eigen::Vector2d(3946, 1547), Eigen::Vector2d(293, 3946)),
        make_tuple(Eigen::Vector2d(5790, 1547), Eigen::Vector2d(293, 5790)),
    };

    for (const auto& point : points)
    {
        // transform the X-Y-coordinates (from the sub-block)
        const Eigen::Vector4d p
        {
            static_cast<double>(get<0>(point).x() - document_info.document_origin_x),
            static_cast<double>(get<0>(point).y() - document_info.document_origin_y),
            0,
            1
        };
        const auto xy_transformed = (transformation_matrix * p).hnormalized();

        // project the point onto the projection plane
        const auto& transformed_and_projected_coordinate = DeskewHelpers::CalculateProjection(
                projection_plane,
                xy_transformed);
        EXPECT_NEAR(transformed_and_projected_coordinate.x(), get<1>(point).x(), 0.6);
        EXPECT_NEAR(transformed_and_projected_coordinate.y(), get<1>(point).y(), 0.6);
    }
}
