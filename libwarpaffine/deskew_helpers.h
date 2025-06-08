// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <limits>
#include <algorithm>
#include <Eigen/Eigen>
#include "document_info.h"
#include "operationtype.h"
#include "geotypes.h"

/// A bunch of utilities for the deskew-operation are found here.
class DeskewHelpers
{
public:
    static Eigen::Matrix4d GetTransformationMatrix(OperationType type, const DeskewDocumentInfo& document_info);
    static Eigen::Matrix4d GetTransformationMatrixSoThatEdgePointIsAtOrigin(OperationType type, const DeskewDocumentInfo& document_info);

    /// This gathers information defining a plane. It is used to project points onto this plane, used
    /// for transforming the X-Y coordinates of the source sub-blocks to the X-Y coordinates of the destination sub-blocks.
    struct ProjectionPlaneInfo
    {
        Eigen::Vector3d x_axis; ///< The x-axis of the projection plane.
        Eigen::Vector3d y_axis; ///< The y-axis of the projection plane.
        Eigen::Vector3d origin; ///< The origin of the projection plane.
    };

    /// Calculates an axis-aligned-bounding-box for a cuboid located a (0,0,0) and with
    /// width, height and depth as specified, which is transformed by the specified transformation.
    ///
    /// \param          width           The width of the cuboid.
    /// \param          height          The height  of the cuboid.
    /// \param          depth           The depth  of the cuboid.
    /// \param          transformation  The transformation.
    /// \param [out]    edge_point      The edge point of the axis-aligned-bounding-box.
    /// \param [out]    extent          The extent of the axis-aligned-bounding-box.
    static void CalculateAxisAlignedBoundingBox(
        double width, double height, double depth,
        const Eigen::Matrix4d& transformation,
        Eigen::Vector3d& edge_point,
        Eigen::Vector3d& extent);

    /// Gives the coordinates of the 8 edges of the specified cuboid when transformed with the specified transformation.
    ///
    /// \param  cuboid          The cuboid (with integer coordinates).
    /// \param  transformation  The transformation.
    ///
    /// \returns    An array containing the 8 transformed edge points.
    static std::array<DoublePos3, 8> TransformEdgePointOfAabb(
        const IntCuboid& cuboid,
        const Eigen::Matrix4d& transformation);

    /// Gives the coordinates of the 8 edges of the specified cuboid when transformed with the specified transformation.
    ///
    /// \param  cuboid          The cuboid (with double precision coordinates).
    /// \param  transformation  The transformation.
    ///
    /// \returns    An array containing the 8 transformed edge points.
    static std::array<DoublePos3, 8> TransformEdgePointOfAabb(
        const DoubleCuboid& cuboid,
        const Eigen::Matrix4d& transformation);

    /// Given an enumeration of 3D-points, calculate an axis-aligned-bounding-box for those points.
    ///
    /// \tparam T   The generic type parameter, which must an enumeration of points (e.g. DoublePos3 or IntPos3).
    /// \param  collection_of_points    The collection of points.
    ///
    /// \returns    The calculated axis-aligned-bounding-box containing all the points.
    template<class T>
    static inline DoubleCuboid CalculateAabbOfPoints(
        const T& collection_of_points)
    {
        double x_min = std::numeric_limits<double>::max();
        double x_max = std::numeric_limits<double>::lowest();
        double y_min = std::numeric_limits<double>::max();
        double y_max = std::numeric_limits<double>::lowest();
        double z_min = std::numeric_limits<double>::max();
        double z_max = std::numeric_limits<double>::lowest();

        for (const auto& point : collection_of_points)
        {
            x_min = std::min(point.x_position, x_min);
            y_min = std::min(point.y_position, y_min);
            z_min = std::min(point.z_position, z_min);
            x_max = std::max(point.x_position, x_max);
            y_max = std::max(point.y_position, y_max);
            z_max = std::max(point.z_position, z_max);
        }

        return DoubleCuboid{ x_min, y_min, z_min, x_max - x_min, y_max - y_min, z_max - z_min };
    }

    static IntCuboid FromFloatCuboid(const DoubleCuboid& float_cuboid);

    /// Returns the orthogonal distance of the measurement planes from the document info.
    static double OrthogonalPlaneDistance(const DeskewDocumentInfo& document_info);

    /// Calculates the projection plane - this is used to transform the X-Y coordinates of the source sub-blocks to the X-Y coordinates of the destination sub-blocks.
    ///
    /// \param  transformation_matrix   The transformation matrix.
    /// \param  source_origin_point     Coordinates of a point, which when transformed defines the origin of the projection plane (optional).
    ///
    /// \returns    The calculated projection plane.
    static ProjectionPlaneInfo CalculateProjectionPlane(const Eigen::Matrix4d& transformation_matrix, const Eigen::Vector3d& source_origin_point = Eigen::Vector3d::Zero());

    /// Calculates the projection onto the projection plane defined by the projection plane info.
    ///
    /// \param  projection_plane_info   Information describing the projection plane.
    /// \param  point                   The point.
    ///
    /// \returns    The calculated projection.
    static Eigen::Vector2d CalculateProjection(const ProjectionPlaneInfo& projection_plane_info, const Eigen::Vector3d& point);
private:
    static double DegreesToRadians(double angle_in_degrees);
    static double RadiansToDegrees(double angle_in_radians);
    static Eigen::Matrix4d GetScalingMatrix(double x_scale, double y_scale, double z_scale);
    static Eigen::Matrix4d GetTranslationMatrix(double x, double y, double z);
    static Eigen::Matrix4d GetRotationAroundXAxis(double angle_in_radians);
    static Eigen::Matrix4d GetRotationAroundZAxis(double angle_in_radians);

    static Eigen::Matrix4d GetTransformationMatrix_Deskew(const DeskewDocumentInfo& document_info);
    static Eigen::Matrix4d GetTransformationMatrix_CoverglassTransform(const DeskewDocumentInfo& document_info, bool rotate_around_z_axis_by_90_degree);
};
