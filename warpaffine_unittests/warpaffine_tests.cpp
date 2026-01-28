// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <warpafine_unittests_config.h>
#include "../libwarpaffine/warpaffine/IWarpAffine.h"

#include "utilities.h"

#include <math.h>

using namespace std;
using namespace libCZI;

static void CopyIntoBrick(Brick& dest_brick, const uint16_t* data)
{
    for (uint32_t x = 0; x < dest_brick.info.width; ++x)
    {
        for (uint32_t y = 0; y < dest_brick.info.height; ++y)
        {
            for (uint32_t z = 0; z < dest_brick.info.depth; ++z)
            {
                *static_cast<uint16_t*>(dest_brick.GetPointerToPixel(x, y, z)) = data[x + y * dest_brick.info.width + z * dest_brick.info.width * dest_brick.info.height];
            }
        }
    }
}

static void CopyIntoBrick(Brick& dest_brick, const uint8_t* data)
{
    for (uint32_t x = 0; x < dest_brick.info.width; ++x)
    {
        for (uint32_t y = 0; y < dest_brick.info.height; ++y)
        {
            for (uint32_t z = 0; z < dest_brick.info.depth; ++z)
            {
                *static_cast<uint8_t*>(dest_brick.GetPointerToPixel(x, y, z)) = data[x + y * dest_brick.info.width + z * dest_brick.info.width * dest_brick.info.height];
            }
        }
    }
}

static void CheckResult(const Brick& brick, const uint16_t* expected_result)
{
    for (uint32_t x = 0; x < brick.info.width; ++x)
    {
        for (uint32_t y = 0; y < brick.info.height; ++y)
        {
            for (uint32_t z = 0; z < brick.info.depth; ++z)
            {
                EXPECT_EQ(
                    *static_cast<const uint16_t*>(brick.GetConstPointerToPixel(x, y, z)),
                    expected_result[x + y * brick.info.width + z * brick.info.width * brick.info.height]);
            }
        }
    }
}

static void CheckResult(const Brick& brick, const uint8_t* expected_result)
{
    for (uint32_t x = 0; x < brick.info.width; ++x)
    {
        for (uint32_t y = 0; y < brick.info.height; ++y)
        {
            for (uint32_t z = 0; z < brick.info.depth; ++z)
            {
                EXPECT_EQ(
                    *static_cast<const uint8_t*>(brick.GetConstPointerToPixel(x, y, z)),
                    expected_result[x + y * brick.info.width + z * brick.info.width * brick.info.height]);
            }
        }
    }
}

// ----------------------------------------------------------------------------


static void TestMoveOnePixelToTheRightGray16(IWarpAffine* warp_affine)
{
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix << 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    // we prepare a brick with 2 slices and the following content:
    // slice 0: 1  2   slice 1: 5 6
    //          3  4            7 8
    Brick source_brick = Utilities::CreateBrick(PixelType::Gray16, 2, 2, 2);
    uint16_t* pointer_voxel = static_cast<uint16_t*>(source_brick.data.get());
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        *(pointer_voxel + i) = 1 + i;
    }

    Brick destination_brick = Utilities::CreateBrick(PixelType::Gray16, 2, 2, 2);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor /*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    const uint16_t* result_data = static_cast<uint16_t*>(destination_brick.data.get());

    static const uint16_t expected_result[8] = { 0, 1, 0, 3, 0, 5, 0, 7 };
    EXPECT_TRUE(0 == memcmp(result_data, expected_result, 8 * sizeof(uint16_t))) << "Not the expected result";
}

static void TestMoveOnePixelToTheRightGray8(IWarpAffine* warp_affine)
{
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix << 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    // we prepare a brick with 2 slices and the following content:
    // slice 0: 1  2   slice 1: 5 6
    //          3  4            7 8
    Brick source_brick = Utilities::CreateBrick(PixelType::Gray8, 2, 2, 2);
    uint8_t* pointer_voxel = static_cast<uint8_t*>(source_brick.data.get());
    for (std::uint8_t i = 0; i < 8; ++i)
    {
        *(pointer_voxel + i) = 1 + i;
    }

    Brick destination_brick = Utilities::CreateBrick(PixelType::Gray8, 2, 2, 2);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    const uint8_t* result_data = static_cast<uint8_t*>(destination_brick.data.get());

    static const uint8_t expected_result[8] = { 0, 1, 0, 3, 0, 5, 0, 7 };
    EXPECT_TRUE(0 == memcmp(result_data, expected_result, 8 * sizeof(uint8_t))) << "Not the expected result";
}
TEST(WarpAffine, MoveOnePixelToTheRightGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMoveOnePixelToTheRightGray16(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheRightGray16Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMoveOnePixelToTheRightGray16(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheRightGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMoveOnePixelToTheRightGray8(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheRightGray8Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMoveOnePixelToTheRightGray8(warp_affine.get());
}


// ----------------------------------------------------------------------------

static void TestMoveOnePixelToTheLeftGray16(IWarpAffine* warp_affine)
{
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix << 1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    Brick source_brick = Utilities::CreateBrick(PixelType::Gray16, 2, 2, 2);

    uint16_t* pointer_voxel = static_cast<uint16_t*>(source_brick.data.get());
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        *(pointer_voxel + i) = 1 + i;
    }

    Brick destination_brick = Utilities::CreateBrickWithGuardPageBehind(PixelType::Gray16, 2, 2, 2);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    const uint16_t* result_data = static_cast<uint16_t*>(destination_brick.data.get());

    static const uint16_t expected_result[8] = { 2, 0, 4, 0, 6, 0, 8, 0 };
    EXPECT_TRUE(0 == memcmp(result_data, expected_result, 8 * sizeof(uint16_t))) << "Not the expected result";
}

static void TestMoveOnePixelToTheLeftGray8(IWarpAffine* warp_affine)
{
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix << 1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    Brick source_brick = Utilities::CreateBrick(PixelType::Gray8, 2, 2, 2);

    uint8_t* pointer_voxel = static_cast<uint8_t*>(source_brick.data.get());
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        *(pointer_voxel + i) = 1 + i;
    }

    Brick destination_brick = Utilities::CreateBrickWithGuardPageBehind(PixelType::Gray8, 2, 2, 2);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    const uint8_t* result_data = static_cast<uint8_t*>(destination_brick.data.get());

    static const uint8_t expected_result[8] = { 2, 0, 4, 0, 6, 0, 8, 0 };
    EXPECT_TRUE(0 == memcmp(result_data, expected_result, 8 * sizeof(uint8_t))) << "Not the expected result";
}

TEST(WarpAffine, MoveOnePixelToTheLeftGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMoveOnePixelToTheLeftGray16(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheLeftGray16Reference)
{
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMoveOnePixelToTheLeftGray16(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheLeftGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMoveOnePixelToTheLeftGray8(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheLeftGray8Reference)
{
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMoveOnePixelToTheLeftGray8(warp_affine.get());
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void MoveOnePixelToTheRightAndUseOffsetForDestination(IWarpAffine* warp_affine)
{
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix << 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    // we prepare a brick with 2 slices and the following content:
    // slice 0: 1  2   slice 1: 5 6
    //          3  4            7 8
    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 2);
    t* pointer_voxel = static_cast<t*>(source_brick.data.get());
    for (t i = 0; i < 8; ++i)
    {
        *(pointer_voxel + i) = 1 + i;
    }

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 1, 2, 2);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 1, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    const t* result_data = static_cast<t*>(destination_brick.data.get());

    // the transformation is moving the data one pixel to the right, and
    //  we instruct to place the destination-brick at (1,0,0), so we expect
    //  to get the second column of the moved-one-pixel-to-the-right result, which
    //  looks like:
    //  slice 0: 0  1   slice 1: 0 5
    //           0  3            0 7 .
    //  In result, we expect:
    //   slice 0: 1   slice 1: 5
    //            3            7 
    static const t expected_result[4] = { 1, 3, 5, 7 };
    EXPECT_TRUE(0 == memcmp(result_data, expected_result, 4 * sizeof(t))) << "Not the expected result";
}

TEST(WarpAffine, MoveOnePixelToTheRightAndUseOffsetForDestinationGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    MoveOnePixelToTheRightAndUseOffsetForDestination<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheRightAndUseOffsetForDestinationGray16Reference)
{
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    MoveOnePixelToTheRightAndUseOffsetForDestination<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheRightAndUseOffsetForDestinationGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    MoveOnePixelToTheRightAndUseOffsetForDestination<uint8_t, PixelType::Gray8>(warp_affine.get());
}

TEST(WarpAffine, MoveOnePixelToTheRightAndUseOffsetForDestinationGray8Reference)
{
    auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    MoveOnePixelToTheRightAndUseOffsetForDestination<uint8_t, PixelType::Gray8>(warp_affine.get());
}

// ----------------------------------------------------------------------------

static void TestLinearInterpolation2x2x2To1x1(IWarpAffine* warp_affine)
{
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, -0.5,
        0, 1, 0, -0.5,
        0, 0, 1, -0.5,
        0, 0, 0, 1;

    // we prepare a brick with 2 slices and the following content:
    // slice 0: 1  2   slice 1: 5 6
    //          3  4            7 8
    const Brick source_brick = Utilities::CreateBrick(PixelType::Gray16, 2, 2, 2);
    uint16_t* pointer_voxel = static_cast<uint16_t*>(source_brick.data.get());
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        *(pointer_voxel + i) = 1 + i;
    }

    // then sample one pixel in the middle, using the math from https://en.wikipedia.org/wiki/Trilinear_interpolation
    // this should give us the result 4.5

    Brick destination_brick = Utilities::CreateBrick(PixelType::Gray16, 1, 1, 1);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kBilinear/*IPPI_INTER_LINEAR*/,
        source_brick,
        destination_brick);

    const uint16_t* result_data = static_cast<uint16_t*>(destination_brick.data.get());
    const uint16_t result_pixel = *result_data;

    EXPECT_TRUE(result_pixel == 4 || result_pixel == 5) << "The exact result is 4.5";
}

TEST(WarpAffine, LinearInterpolation2x2x2To1x1Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestLinearInterpolation2x2x2To1x1(warp_affine.get());
}

TEST(WarpAffine, LinearInterpolation2x2x2To1x1IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestLinearInterpolation2x2x2To1x1(warp_affine.get());
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void TestRotateBy90DegreeAroundZAxis(IWarpAffine* warp_affine)
{
    static const t source_data[2 * 2 * 3] =
    {
        10, 11,
        20, 21,

        30, 51,
        31, 61,

        40, 71,
        41, 72
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    // Note: It seems that IPP is very susceptible to inaccuracies in this case. If we create the transformation matrix
    //        like so
    //          Eigen::Matrix4d transformation_matrix = (Eigen::Translation3d(0.5, 0.5, 0) * rotate_around_z_axis * Eigen::Translation3d(-0.5, -0.5, 0)).matrix();
    //       then IPP will not give the correct result (the top-left pixel will not be written), where the only difference is
    //       that the matrix will not be exact (as below) because of inaccuracies with the trigonometric calculation (with the
    //       rotation matrix). When giving the exact matrix, it seems to work fine.
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix << 0, -1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

    //cout << "Transformation-matrix" << endl << transformation_matrix << endl << endl;

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    static const t expected_result[2 * 2 * 3] =
    {
        20, 10,
        21, 11,

        31, 30,
        61, 51,

        41, 40,
        72, 71
    };

    CheckResult(destination_brick, expected_result);
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisIPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestRotateBy90DegreeAroundZAxis<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisReference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestRotateBy90DegreeAroundZAxis<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestRotateBy90DegreeAroundZAxis<uint8_t, PixelType::Gray8>(warp_affine.get());
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisGray8Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestRotateBy90DegreeAroundZAxis<uint8_t, PixelType::Gray8>(warp_affine.get());
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void TestRotateBy90DegreeAroundZAxisAndDestinationOffset(IWarpAffine* warp_affine)
{
    static const t source_data[2 * 2 * 3] =
    {
        10, 11,
        20, 21,

        30, 51,
        31, 61,

        40, 71,
        41, 72
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    // Note: It seems that IPP is very susceptible to inaccuracies in this case. If we create the transformation matrix
    //        like so
    //          Eigen::Matrix4d transformation_matrix = (Eigen::Translation3d(0.5, 0.5, 0) * rotate_around_z_axis * Eigen::Translation3d(-0.5, -0.5, 0)).matrix();
    //       then IPP will not give the correct result (the top-left pixel will not be written), where the only difference is
    //       that the matrix will not be exact (as below) because of inaccuracies with the trigonometric calculation (with the
    //       rotation matrix). When giving the exact matrix, it seems to work fine.
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        0, -1, 0, 1,
        1, 0, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 2);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 1 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    static const t expected_result[2 * 2 * 2] =
    {
        31, 30,
        61, 51,

        41, 40,
        72, 71
    };

    CheckResult(destination_brick, expected_result);
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisAndDestinationOffsetGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestRotateBy90DegreeAroundZAxisAndDestinationOffset<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisAndDestinationOffsetGray16Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestRotateBy90DegreeAroundZAxisAndDestinationOffset<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisAndDestinationOffsetGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestRotateBy90DegreeAroundZAxisAndDestinationOffset<uint8_t, PixelType::Gray8>(warp_affine.get());
}

TEST(WarpAffine, RotateBy90DegreeAroundZAxisAndDestinationOffsetGray8Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestRotateBy90DegreeAroundZAxisAndDestinationOffset<uint8_t, PixelType::Gray8>(warp_affine.get());
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void TestMirrorOnYZPlane(IWarpAffine* warp_affine)
{
    // we mirror the brick along a plane parallel to the y-z-plane through the middle of the brick (middle in x)
    static const t source_data[2 * 2 * 3] =
    {
        10, 11,
        20, 21,

        30, 51,
        31, 61,

        40, 71,
        41, 72
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    /// The transformation matrix is constructed as:
    ///  1. translate in x so that the bitmap is centered (in x)  
    ///  2. flip the x-coordinate  
    ///  3. undo the translation from step 1
    /// which gives us the following matrices
    /// 1.  ( 1 0 0 -0.5 )    2. ( -1 0 0 0 )    3.  ( 1 0 0 0.5 )
    ///     ( 0 1 0 0    )       (  0 1 0 0 )        ( 0 1 0 0   )
    ///     ( 0 0 1 0    )       (  0 0 1 0 )        ( 0 0 1 0   )
    ///     ( 0 0 0 1    )       (  0 0 0 1 )        ( 0 0 0 1   )
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        -1, 0, 0, 1,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    static const t expected_result[2 * 2 * 3] =
    {
        11, 10,
        21, 20,

        51, 30,
        61, 31,

        71, 40,
        72, 41
    };

    CheckResult(destination_brick, expected_result);
}

TEST(WarpAffine, MirrorOnZYPlaneGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMirrorOnYZPlane<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnZYPlaneGray16Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMirrorOnYZPlane<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnZYPlaneGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMirrorOnYZPlane<uint8_t, PixelType::Gray8>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnZYPlaneGray8Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMirrorOnYZPlane<uint8_t, PixelType::Gray8>(warp_affine.get());
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void TestMirrorOnXYPlane(IWarpAffine* warp_affine)
{
    // we mirror the brick along a plane parallel to the x-y-plane through the middle of the brick (middle in z)
    static const t source_data[2 * 2 * 3] =
    {
        10, 11,
        20, 21,

        30, 51,
        31, 61,

        40, 71,
        41, 72
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    /// The transformation matrix is constructed as:
    ///  1. translate in x so that the bitmap is centered (in x)  
    ///  2. flip the z-coordinate  
    ///  3. undo the translation from step 1
    /// which gives us the following matrices
    /// 1.  ( 1 0 0 0    )    2. ( 1 0  0 0 )    3.  ( 1 0 0 0    )
    ///     ( 0 1 0 0    )       ( 0 1  0 0 )        ( 0 1 0 0    )
    ///     ( 0 0 1 -1.0 )       ( 0 0 -1 0 )        ( 0 0 1 1.0  )
    ///     ( 0 0 0 1    )       ( 0 0  0 1 )        ( 0 0 0 1    )
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, -1, 2,
        0, 0, 0, 1;

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    static const t expected_result[2 * 2 * 3] =
    {
        40, 71,
        41, 72,

        30, 51,
        31, 61,

        10, 11,
        20, 21
    };

    CheckResult(destination_brick, expected_result);
}

TEST(WarpAffine, MirrorOnXYPlaneGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMirrorOnXYPlane<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnXYPlaneGray16Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMirrorOnXYPlane<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnXYPlaneGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMirrorOnXYPlane<uint8_t, PixelType::Gray8>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnXYPlaneGray8Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMirrorOnXYPlane<uint8_t, PixelType::Gray8>(warp_affine.get());
}
// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void TestMirrorOnXZPlane(IWarpAffine* warp_affine)
{
    // we mirror the brick along a plane parallel to the x-z-plane through the middle of the brick (middle in y)
    static const t source_data[2 * 2 * 3] =
    {
        10, 11,
        20, 21,

        30, 51,
        31, 61,

        40, 71,
        41, 72
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    /// The transformation matrix is constructed as:
    ///  1. translate in x so that the bitmap is centered (in x)  
    ///  2. flip the x-coordinate  
    ///  3. undo the translation from step 1
    /// which gives us the following matrices
    /// 1.  ( 1 0 0 0    )    2. ( 1  0 0 0 )    3.  ( 1 0 0 0 )
    ///     ( 0 1 0 -0.5 )       ( 0 -1 0 0 )        ( 0 1 0 0.5   )
    ///     ( 0 0 1 0    )       ( 0  0 1 0 )        ( 0 0 1 0   )
    ///     ( 0 0 0 1    )       ( 0  0 0 1 )        ( 0 0 0 1   )
    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, 0,
        0, -1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1;

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    warp_affine->Execute(
        transformation_matrix,
        IntPos3{ 0, 0, 0 },
        Interpolation::kNearestNeighbor/*IPPI_INTER_NN*/,
        source_brick,
        destination_brick);

    static const t expected_result[2 * 2 * 3] =
    {
        20, 21,
        10, 11,

        31, 61,
        30, 51,

        41, 72,
        40, 71,
    };

    CheckResult(destination_brick, expected_result);
}

TEST(WarpAffine, MirrorOnXZPlaneGray16IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMirrorOnXZPlane<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnXZPlaneGray16Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMirrorOnXZPlane<uint16_t, PixelType::Gray16>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnXZPlaneGray8IPP)
{
#if !WARPAFFINEUNITTESTS_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    GTEST_SKIP() << "Skipping because IPP is not available";
#endif
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kIPP);
    TestMirrorOnXZPlane<uint8_t, PixelType::Gray8>(warp_affine.get());
}

TEST(WarpAffine, MirrorOnXZPlaneGray8Reference)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);
    TestMirrorOnXZPlane<uint8_t, PixelType::Gray8>(warp_affine.get());
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void ExtractAllPixelsAndCheck(WarpAffineImplementation warpaffine_implementation, Interpolation interpolation)
{
    const auto warp_affine = CreateWarpAffine(warpaffine_implementation);

    static const t source_data[2 * 2 * 3] =
    {
        10, 11, /* (0,0,0)   (1,0,0) */
        20, 21, /* (0,1,0)   (1,1,0) */

        30, 51, /* (0,0,1)   (1,0,1) */
        31, 61, /* (0,1,1)   (1,1,1) */

        40, 71, /* (0,0,2)   (1,0,2) */
        41, 72, /* (0,1,2)   (1,1,2) */
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 1, 1, 1);

    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;

    for (int z = 0; z < 3; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                IntPos3 position_destination_brick{ x, y, z };
                warp_affine->Execute(
                    transformation_matrix,
                    position_destination_brick,
                    interpolation,
                    source_brick,
                    destination_brick);
                const t result_pixel = *(static_cast<t*>(destination_brick.data.get()) + 0);
                const t expected_pixel = *(static_cast<const t*>(source_brick.data.get()) + (z * 4 + y * 2 + x));
                EXPECT_EQ(result_pixel, expected_pixel) << "Not the expected result at destination position (" <<
                    position_destination_brick.x_position << "," <<
                    position_destination_brick.y_position << "," <<
                    position_destination_brick.z_position << ")";
            }
        }
    }
}

TEST(WarpAffine, ExtractAllPixelsAndCheckGray8ReferenceNearestNeighbor)
{
    // we put the destination volume at all pixels of the source volume one after another, and check that the extracted pixel is the expected one
    ExtractAllPixelsAndCheck<uint8_t, PixelType::Gray8>(WarpAffineImplementation::kReference, Interpolation::kNearestNeighbor);
}

TEST(WarpAffine, ExtractAllPixelsAndCheckGray16ReferenceNearestNeighbor)
{
    // we put the destination volume at all pixels of the source volume one after another, and check that the extracted pixel is the expected one
    ExtractAllPixelsAndCheck<uint16_t, PixelType::Gray16>(WarpAffineImplementation::kReference, Interpolation::kNearestNeighbor);
}

TEST(WarpAffine, ExtractAllPixelsAndCheckGray8ReferenceTriLinear)
{
    // we put the destination volume at all pixels of the source volume one after another, and check that the extracted pixel is the expected one
    ExtractAllPixelsAndCheck<uint8_t, PixelType::Gray8>(WarpAffineImplementation::kReference, Interpolation::kBilinear);
}

TEST(WarpAffine, ExtractAllPixelsAndCheckGray16ReferenceTriLinear)
{
    // we put the destination volume at all pixels of the source volume one after another, and check that the extracted pixel is the expected one
    ExtractAllPixelsAndCheck<uint16_t, PixelType::Gray16>(WarpAffineImplementation::kReference, Interpolation::kBilinear);
}

// ----------------------------------------------------------------------------

template<typename t, libCZI::PixelType t_pixeltype>
static void SampleQuarterOfAPixelOutsideOfVolume(WarpAffineImplementation warpaffine_implementation, Interpolation interpolation)
{
    const auto warp_affine = CreateWarpAffine(warpaffine_implementation);

    static const t source_data[2 * 2 * 3] =
    {
        10, 11, /* (0,0,0)   (1,0,0) */
        20, 21, /* (0,1,0)   (1,1,0) */

        30, 51, /* (0,0,1)   (1,0,1) */
        31, 61, /* (0,1,1)   (1,1,1) */

        40, 71, /* (0,0,2)   (1,0,2) */
        41, 72, /* (0,1,2)   (1,1,2) */
    };

    Brick source_brick = Utilities::CreateBrick(t_pixeltype, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    Brick destination_brick = Utilities::CreateBrick(t_pixeltype, 1, 1, 1);

    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, -0.25,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;

    for (int z = 0; z < 3; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                IntPos3 position_destination_brick{ x, y, z };
                warp_affine->Execute(
                    transformation_matrix,
                    position_destination_brick,
                    interpolation,
                    source_brick,
                    destination_brick);
                const t result_pixel = *(static_cast<t*>(destination_brick.data.get()) + 0);
                const t expected_pixel = *(static_cast<const t*>(source_brick.data.get()) + (z * 4 + y * 2 + x));
                EXPECT_EQ(result_pixel, expected_pixel) << "Not the expected result at destination position (" <<
                    position_destination_brick.x_position << "," <<
                    position_destination_brick.y_position << "," <<
                    position_destination_brick.z_position << ")";
            }
        }
    }
}

TEST(WarpAffine, SampleVolumeQuarterOfAPixelOffGray8ReferenceNearestNeighbor)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);

    static const uint8_t source_data[2 * 2 * 3] =
    {
        10, 11, /* (0,0,0)   (1,0,0) */
        20, 21, /* (0,1,0)   (1,1,0) */

        30, 51, /* (0,0,1)   (1,0,1) */
        31, 61, /* (0,1,1)   (1,1,1) */

        40, 71, /* (0,0,2)   (1,0,2) */
        41, 72, /* (0,1,2)   (1,1,2) */
    };

    Brick source_brick = Utilities::CreateBrick(PixelType::Gray8, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    Brick destination_brick = Utilities::CreateBrick(PixelType::Gray8, 1, 1, 1);

    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, -0.25, // we are off by a quarter pixel in x, y and z - should still get the same pixel as before,
        0, 1, 0, -0.25, // because it remains to be the "nearest" pixel
        0, 0, 1, -0.25,
        0, 0, 0, 1;

    for (int z = 0; z < 3; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                IntPos3 position_destination_brick{ x, y, z };
                warp_affine->Execute(
                    transformation_matrix,
                    position_destination_brick,
                    Interpolation::kNearestNeighbor,
                    source_brick,
                    destination_brick);
                const uint8_t result_pixel = *(static_cast<uint8_t*>(destination_brick.data.get()) + 0);
                const uint8_t expected_pixel = *(static_cast<const uint8_t*>(source_brick.data.get()) + (z * 4 + y * 2 + x));
                EXPECT_EQ(result_pixel, expected_pixel) << "Not the expected result at destination position (" <<
                    position_destination_brick.x_position << "," <<
                    position_destination_brick.y_position << "," <<
                    position_destination_brick.z_position << ")";
            }
        }
    }
}

TEST(WarpAffine, SampleVolumeQuarterOfAPixelOffGray8ReferenceTriLinear)
{
    const auto warp_affine = CreateWarpAffine(WarpAffineImplementation::kReference);

    static const uint8_t source_data[2 * 2 * 3] =
    {
        10, 11, /* (0,0,0)   (1,0,0) */
        20, 21, /* (0,1,0)   (1,1,0) */

        30, 51, /* (0,0,1)   (1,0,1) */
        31, 61, /* (0,1,1)   (1,1,1) */

        40, 71, /* (0,0,2)   (1,0,2) */
        41, 72, /* (0,1,2)   (1,1,2) */
    };

    Brick source_brick = Utilities::CreateBrick(PixelType::Gray8, 2, 2, 3);
    CopyIntoBrick(source_brick, source_data);

    Brick destination_brick = Utilities::CreateBrick(PixelType::Gray8, 1, 1, 1);

    Eigen::Matrix4d transformation_matrix;
    transformation_matrix <<
        1, 0, 0, 0.25, // we are off by a quarter pixel in x, y and z - expectation is
        0, 1, 0, 0.25, // that when sampling "one pixel without the border", we replicate
        0, 0, 1, 0.25, // the pixel at the border
        0, 0, 0, 1;

    static const uint8_t expected_result_data[2 * 2 * 3] =
    {
        10, 11, /* (0,0,0)   (1,0,0) */
        18, 18, /* (0,1,0)   (1,1,0) */

        25, 37, /* (0,0,1)   (1,0,1) */
        27, 43, /* (0,1,1)   (1,1,1) */

        38, 59, /* (0,0,2)   (1,0,2) */
        38, 61, /* (0,1,2)   (1,1,2) */
    };

    for (int z = 0; z < 3; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                IntPos3 position_destination_brick{ x, y, z };
                warp_affine->Execute(
                    transformation_matrix,
                    position_destination_brick,
                    Interpolation::kBilinear,
                    source_brick,
                    destination_brick);
                const uint8_t result_pixel = *(static_cast<uint8_t*>(destination_brick.data.get()) + 0);
                const uint8_t expected_pixel = expected_result_data[z * 4 + y * 2 + x];
                EXPECT_EQ(result_pixel, expected_pixel) << "Not the expected result at destination position (" <<
                    position_destination_brick.x_position << "," <<
                    position_destination_brick.y_position << "," <<
                    position_destination_brick.z_position << ")";
            }
        }
    }
}
