// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "WarpAffine_IPP.h"

#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
#include <iostream>
#include <memory>
#include <limits>
#include <ipp.h>
#include "WarpAffine_Reference.h"
#include "../deskew_helpers.h"

using namespace libCZI;
using namespace std;

void WarpAffineIPP::Execute(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    this->ExecuteMinimalSource(transformation, destination_brick_position, interpolation, source_brick, destination_brick);
}

/*static*/Eigen::Matrix4d WarpAffineIPP::IncludeDestinationBrickPosition(const Eigen::Matrix4d& transformation, const IntPos3& destination_brick_position)
{
    if (destination_brick_position.x_position == 0 && destination_brick_position.y_position == 0 && destination_brick_position.z_position == 0)
    {
        return transformation;
    }

    Eigen::Matrix4d translation;
    translation <<
        1, 0, 0, -destination_brick_position.x_position,
        0, 1, 0, -destination_brick_position.y_position,
        0, 0, 1, -destination_brick_position.z_position,
        0, 0, 0, 1;

    // we add a translation to the transformation, so that the specified x,y,z lines up with the origin

    return translation * transformation;
}

void WarpAffineIPP::ExecuteMinimalSource(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    // What we do here is to limit the "source-brick" to the very minimal region (or VOI) which is
    //  required to produce the destination. We do this because it turned out that the kIPP-WarpAffine-function
    //  has flaws when it comes to "large source/destination", where large here means beyond 2 (or 4) GB. We get
    //  access violations, presumably because the calculation of pointers overflows (seems to be done in 32-bit
    //  arithmetic). By limiting the size to the very minimum required, we can work around this (at least to some
    //  degree).
    auto matrix_including_destination_offset = IncludeDestinationBrickPosition(transformation, destination_brick_position);
    auto inverse_matrix_including_destination_offset = matrix_including_destination_offset.inverse();

    const DoubleCuboid destination_cuboid{ 0, 0, 0 , static_cast<double>(destination_brick.info.width), static_cast<double>(destination_brick.info.height), static_cast<double>(destination_brick.info.depth) };
    const auto edge_points = DeskewHelpers::TransformEdgePointOfAabb(destination_cuboid, inverse_matrix_including_destination_offset);
    //for (auto& p : edge_points) { p.x_position += 1; p.y_position += 1; p.z_position += 1; };
    const auto source_voi = DeskewHelpers::CalculateAabbOfPoints(edge_points);

    // ok, this means - we need (in order to produce this output) the rectangle as described by "source_voi"

    // now, enlarge this rectangle to integer coordinates
    auto integer_source_voi = DeskewHelpers::FromFloatCuboid(source_voi);
    integer_source_voi.width++;
    integer_source_voi.height++;
    integer_source_voi.depth++;

    // ok, this is now the cuboid in the source which we need to use - we next intersect it with the actual source cuboid,
    //  and this gives us the part of the source which we need
    const auto integer_source_voi_clipped = integer_source_voi.GetIntersectionWith(IntCuboid{ 0, 0, 0, source_brick.info.width, source_brick.info.height, source_brick.info.depth });

    // As of the time of writing, the IPP has trouble if the source or the destination is
    // beyond 2GB it seems. What we do here - if the source or the destination buffer is 
    // larger than 2GB, we fall back to the reference implemenation.
    constexpr uint64_t kSafeSizeForIppOperation = numeric_limits<int32_t>::max();
    if (static_cast<uint64_t>(source_brick.info.stride_plane) * integer_source_voi_clipped.depth > kSafeSizeForIppOperation ||
        static_cast<uint64_t>(destination_brick.info.stride_plane) * destination_brick.info.depth > kSafeSizeForIppOperation)
    {
        // The reference-implemenation currently only supports NN and linear interpolation. What we do here now is
        //  that we "silently" map all supported interpolation modes to this.
        // TODO(JBL): this is of course lame
        Interpolation interpolation_mode_adjusted;
        switch (interpolation)
        {
        case Interpolation::kNearestNeighbor:
        case Interpolation::kBilinear:
            interpolation_mode_adjusted = interpolation;
            break;
        default:
            interpolation_mode_adjusted = Interpolation::kBilinear;
            break;
        }

        WarpAffine_Reference::ExecuteFunction(
                                transformation,
                                destination_brick_position,
                                interpolation_mode_adjusted,
                                source_brick,
                                destination_brick);
        return;
    }

    // now clear the destination brick (since the kIPP-function only is writing pixels which are "present", everything else
    //  is left untouched)
    // TODO(JBL): we could be more smart here I guess, and check whether all of "destination" is covered, and in this case we
    //        could skip this memset
    memset(destination_brick.data.get(), 0, static_cast<size_t>(destination_brick.info.stride_plane) * destination_brick.info.depth);
    if (integer_source_voi_clipped.IsEmpty())
    {
        // in this case, there is nothing to do
        return;
    }

    // now, we adjust the pointer to the source-brick so that it points to the pixel we identified
    //  as the starting-point (of the region-of-interest we calculated before)
    const void* src_data_adjusted = static_cast<uint8_t*>(source_brick.data.get()) +
        static_cast<size_t>(integer_source_voi_clipped.z_position) * source_brick.info.stride_plane +
        static_cast<size_t>(integer_source_voi_clipped.y_position) * source_brick.info.stride_line +
        static_cast<size_t>(integer_source_voi_clipped.x_position) * Utils::GetBytesPerPixel(source_brick.info.pixelType);

    // and, finally, we adjust the transformation matrix to compensate for this translation
    Eigen::Matrix4d matrix_compensating_for_cropping;
    matrix_compensating_for_cropping <<
        1, 0, 0, +integer_source_voi_clipped.x_position,
        0, 1, 0, +integer_source_voi_clipped.y_position,
        0, 0, 1, +integer_source_voi_clipped.z_position,
        0, 0, 0, 1;

    // adjust the transformation matrix - this translation has to occur as the first operation, so we need to multiply it from
    //  the right
    Eigen::Matrix4d matrix_adjusted = matrix_including_destination_offset * matrix_compensating_for_cropping;

    double coefficients[3][4];
    coefficients[0][0] = matrix_adjusted(0, 0);
    coefficients[0][1] = matrix_adjusted(0, 1);
    coefficients[0][2] = matrix_adjusted(0, 2);
    coefficients[0][3] = matrix_adjusted(0, 3);
    coefficients[1][0] = matrix_adjusted(1, 0);
    coefficients[1][1] = matrix_adjusted(1, 1);
    coefficients[1][2] = matrix_adjusted(1, 2);
    coefficients[1][3] = matrix_adjusted(1, 3);
    coefficients[2][0] = matrix_adjusted(2, 0);
    coefficients[2][1] = matrix_adjusted(2, 1);
    coefficients[2][2] = matrix_adjusted(2, 2);
    coefficients[2][3] = matrix_adjusted(2, 3);

    // ok, so now adapt the source
    const IpprVolume sourceVolume
    {
        static_cast<int>(integer_source_voi_clipped.width),
        static_cast<int>(integer_source_voi_clipped.height),
        static_cast<int>(integer_source_voi_clipped.depth)
    };

    const IpprCuboid sourceCuboid
    {
        0, 0, 0,
        static_cast<int>(integer_source_voi_clipped.width),
        static_cast<int>(integer_source_voi_clipped.height),
        static_cast<int>(integer_source_voi_clipped.depth)
    };

    const IpprCuboid ippr_destination_cuboid
    {
        0,
        0,
        0,
        static_cast<int>(destination_brick.info.width),
        static_cast<int>(destination_brick.info.height),
        static_cast<int>(destination_brick.info.depth)
    };

    int size_of_tempbuffer;
    IppStatus status = ipprWarpAffineGetBufSize(
        sourceVolume,                   // srcVolume 
        sourceCuboid,                   // srcVoi
        ippr_destination_cuboid,        // dstVoi
        coefficients,                   // coeffs[3][4]
        1,                              // nChannel
        static_cast<int>(interpolation),// interpolation, 
        &size_of_tempbuffer);           // pSize

    int dummy;
    const std::unique_ptr<Ipp8u, void(*)(Ipp8u*)> up_temporary_buffer(
        size_of_tempbuffer > 0 ? ippiMalloc_8u_C1(size_of_tempbuffer, 1, &dummy) : nullptr,
        [](Ipp8u* p)->void { if (p != nullptr) { ippiFree(p); } });

    switch (source_brick.info.pixelType)
    {
    case PixelType::Gray16:
        // https://www.intel.com/content/www/us/en/develop/documentation/ipp-dev-reference/top/volume-2-image-processing/3d-data-processing-functions/warpaffine.html
        status = ipprWarpAffine_16u_C1V(
            static_cast<const Ipp16u*>(src_data_adjusted),      // pSrc
            sourceVolume,                                       // srcVolume
            source_brick.info.stride_line,                      // srcStep
            source_brick.info.stride_plane,                     // srcPlaneStep
            sourceCuboid,                                       // srcVoi
            static_cast<Ipp16u*>(destination_brick.data.get()), // pDst
            destination_brick.info.stride_line,                 // dstStep
            destination_brick.info.stride_plane,                // dstPlaneStep
            ippr_destination_cuboid,                            // dstVoi
            coefficients,                                       // coeffs[3][4]
            static_cast<int>(interpolation),                    // interpolation
            up_temporary_buffer.get());                         // pBuffer
        break;
    case PixelType::Gray8:
        // https://www.intel.com/content/www/us/en/develop/documentation/ipp-dev-reference/top/volume-2-image-processing/3d-data-processing-functions/warpaffine.html
        status = ipprWarpAffine_8u_C1V(
            static_cast<const Ipp8u*>(src_data_adjusted),       // pSrc
            sourceVolume,                                       // srcVolume
            source_brick.info.stride_line,                      // srcStep
            source_brick.info.stride_plane,                     // srcPlaneStep
            sourceCuboid,                                       // srcVoi
            static_cast<Ipp8u*>(destination_brick.data.get()),  // pDst
            destination_brick.info.stride_line,                 // dstStep
            destination_brick.info.stride_plane,                // dstPlaneStep
            ippr_destination_cuboid,                            // dstVoi
            coefficients,                                       // coeffs[3][4]
            static_cast<int>(interpolation),                    // interpolation
            up_temporary_buffer.get());                         // pBuffer
        break;
    case PixelType::Gray32Float:
        // https://www.intel.com/content/www/us/en/develop/documentation/ipp-dev-reference/top/volume-2-image-processing/3d-data-processing-functions/warpaffine.html
        status = ipprWarpAffine_32f_C1V(
            static_cast<const Ipp32f*>(src_data_adjusted),      // pSrc
            sourceVolume,                                       // srcVolume
            source_brick.info.stride_line,                      // srcStep
            source_brick.info.stride_plane,                     // srcPlaneStep
            sourceCuboid,                                       // srcVoi
            static_cast<Ipp32f*>(destination_brick.data.get()), // pDst
            destination_brick.info.stride_line,                 // dstStep
            destination_brick.info.stride_plane,                // dstPlaneStep
            ippr_destination_cuboid,                            // dstVoi
            coefficients,                                       // coeffs[3][4]
            static_cast<int>(interpolation),                    // interpolation
            up_temporary_buffer.get());                         // pBuffer
        break;
    default:
        throw invalid_argument("Only pixeltype Gray8, Gray16 or Gray32Float is supported.");
    }
}
#endif
