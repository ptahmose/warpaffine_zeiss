// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include "../libwarpaffine/warpaffine/IWarpAffine.h"

#include "utilities.h"

#include <math.h>

#include "../libwarpaffine/document_info.h"
#include "../libwarpaffine/deskew_helpers.h"

using namespace std;
using namespace libCZI;

TEST(XYCoordinateTests, TestCase1)
{
#if false
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
    document_info.z_scaling = 3.9999999999999998e-07;
    document_info.xy_scaling = 1.4499219272808386e-07;

    Eigen::Matrix4d transformation_matrix = DeskewHelpers::GetTransformationMatrixSoThatEdgePointIsAtOrigin(OperationType::CoverGlassTransformAndXYRotated, document_info);
    // Eigen::Matrix4d transformation_matrix = DeskewHelpers::GetTransformationMatrix_CoverglassTransform_wo_skew(document_info, true);
    ostringstream ss;
    ss << transformation_matrix << endl;
    ADD_FAILURE() << ss.str();

    Eigen::Vector4d point_origin(0, 2988, 0, 1);

    Eigen::Vector4d transformed_origin = transformation_matrix * point_origin;
    Eigen::Vector3d transformed_origin_3d = transformed_origin.head<3>() / transformed_origin.w();

    Eigen::Vector4d point(136, 0, 0, 1);
    Eigen::Vector4d transformed = transformation_matrix * point;
    Eigen::Vector3d transformed_3d = transformed.head<3>() / transformed.w();

    Eigen::Vector3d shifted = transformed_3d - transformed_origin_3d;

    const auto projected_origin = DeskewHelpers::projectPointOntoTransformedXYPlane(transformation_matrix, point_origin);
    const auto projected = DeskewHelpers::projectPointOntoTransformedXYPlane(transformation_matrix, point);
    Eigen::Vector3d shifted_projected = projected - projected_origin;

    vector<Eigen::Vector3d > points = {
        Eigen::Vector3d(136, 0, 0),
        Eigen::Vector3d(0, 2988, 0),
        Eigen::Vector3d(1843 , 2988, 0)
    };

    Eigen::Vector4d origin_2 = transformation_matrix * point_origin;
    for (const auto& point : points)
    {
        Eigen::Vector4d point_4d(point.x(), point.y(), point.z(), 1);
        auto transformed = transformation_matrix * point_4d - origin_2;

        Eigen::Vector3d transformed_point = DeskewHelpers::projectPointOntoTransformedXYPlane(transformation_matrix, point_4d);
        //Eigen::Vector3d transformed_point_3d = transformed_point.head<3>() / transformed_point.w();
        Eigen::Vector3d shifted_point = transformed_point - projected_origin;
        ostringstream ss;
        ss << "source point: " << point.x() << ", " << point.y() << ", " << point.z()
            << " * transformed " << shifted_point.x() << ", " << shifted_point.y() << ", " << shifted_point.z()
            << " * orig " << transformed.x() << ", " << transformed.y() << ", " << transformed.z() << endl;
        ADD_FAILURE() << ss.str();
    }

    auto c = transformation_matrix * Eigen::Vector4d{ 0,0,0,1 };

    auto o_p = transformation_matrix.inverse() * Eigen::Vector4d{ 0,0,0,1 };
    ADD_FAILURE() << "origin: " << c.x() << "," << c.y() << "," << c.z() << endl;

    auto e1 = transformation_matrix.inverse() * Eigen::Vector4d{ 1,0,0,0 };
    ADD_FAILURE() << "e1: " << e1.x() << "," << e1.y() << "," << e1.z() << endl;
    auto e2 = transformation_matrix.inverse() * Eigen::Vector4d{ 0,1,0,0 };
    ADD_FAILURE() << "e2: " << e2.x() << "," << e2.y() << "," << e2.z() << endl;

    auto p1 = transformation_matrix * Eigen::Vector4d{ 136,2988,0,1 };
    auto p2 = transformation_matrix * Eigen::Vector4d{ 136,0,0,1 };
    auto p3 = transformation_matrix * Eigen::Vector4d{ 0,2988,0,1 };
    auto dist_x = (p1 - p2).head<3>().norm();
    auto dist_y = (p1 - p3).head<3>().norm();
    ADD_FAILURE() << "dist: " << dist_x << "," << dist_y << endl;

    auto pp1 = transformation_matrix * Eigen::Vector4d{ 0, 2988, 0,1 };
    ADD_FAILURE() << "p: " << pp1.x() << "," << pp1.y() << "," << pp1.z() << endl;
#endif
}
