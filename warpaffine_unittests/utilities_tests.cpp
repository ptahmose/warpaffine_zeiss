// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <warpafine_unittests_config.h>
#include "../libwarpaffine/utilities.h"

#include <cstdint>

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case1_Gray8)
{
    // we test this arrangement
    // +---+
    // |   |
    // | * |
    // |   |
    // +---+
    //
    // So, we copy the source (with value 88) right into the middle of the 3x3 destination bitmap, and
    //  expect to have 0 for all the rest.
    uint8_t source[1] = { 88 };
    uint8_t destination[3 * 3];

    memset(destination, 42, 3 * 3);

    Utilities::CopyAtOffsetInfo info =
    {
        1,
        1,
        libCZI::PixelType::Gray8,
        source,
        1,
        1,
        1,
        destination,
        3,
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint8_t expected_result[3 * 3] = { 0, 0, 0,  0, 88, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case2_Gray8)
{
    // we test this arrangement
    // +---+
    // |   |
    // |  *|
    // |   |
    // +---+

    uint8_t source[1] = { 88 };
    uint8_t destination[3 * 3];

    memset(destination, 42, 3 * 3);

    Utilities::CopyAtOffsetInfo info =
    {
        2,
        1,
        libCZI::PixelType::Gray8,
        source,
        1,
        1,
        1,
        destination,
        3,
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint8_t expected_result[3 * 3] = { 0, 0, 0,  0, 0, 88,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case3_Gray8)
{
    // we test this arrangement
    // +---+
    // |   |
    // |*  |
    // |   |
    // +---+

    uint8_t source[1] = { 88 };
    uint8_t destination[3 * 3];

    memset(destination, 42, 3 * 3);

    Utilities::CopyAtOffsetInfo info =
    {
        0,
        1,
        libCZI::PixelType::Gray8,
        source,
        1,
        1,
        1,
        destination,
        3,
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint8_t expected_result[3 * 3] = { 0, 0, 0,  88, 0, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case4_Gray8)
{
    // we test this arrangement
    // +---+
    // |   |
    // |   |
    // | * |
    // +---+

    uint8_t source[1] = { 88 };
    uint8_t destination[3 * 3];

    memset(destination, 42, 3 * 3);

    Utilities::CopyAtOffsetInfo info =
    {
        1,
        2,
        libCZI::PixelType::Gray8,
        source,
        1,
        1,
        1,
        destination,
        3,
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint8_t expected_result[3 * 3] = { 0, 0, 0,  0, 0, 0,  0, 88, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case5_Gray8)
{
    // we test this arrangement
    // +---+
    // | * |
    // |   |
    // |   |
    // +---+

    uint8_t source[1] = { 88 };
    uint8_t destination[3 * 3];

    memset(destination, 42, 3 * 3);

    Utilities::CopyAtOffsetInfo info =
    {
        1,
        0,
        libCZI::PixelType::Gray8,
        source,
        1,
        1,
        1,
        destination,
        3,
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint8_t expected_result[3 * 3] = { 0, 88, 0,  0, 0, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case6_Gray8)
{
    // In this case, the intersection of source and destination is empty, so we expect the
    //  destination to be completely cleared

    uint8_t source[1] = { 88 };
    uint8_t destination[3 * 3];

    memset(destination, 42, 3 * 3);

    Utilities::CopyAtOffsetInfo info =
    {
        3,
        4,
        libCZI::PixelType::Gray8,
        source,
        1,
        1,
        1,
        destination,
        3,
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint8_t expected_result[3 * 3] = { 0, 0, 0,  0, 0, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case1_Gray16)
{
    // we test this arrangement
    // +---+
    // |   |
    // | * |
    // |   |
    // +---+
    //
    // So, we copy the source (with value 88) right into the middle of the 3x3 destination bitmap, and
    //  expect to have 0 for all the rest.
    uint16_t source[1] = { 88 };
    uint16_t destination[3 * 3];

    memset(destination, 42, 3 * 3 * sizeof(uint16_t));

    Utilities::CopyAtOffsetInfo info =
    {
        1,
        1,
        libCZI::PixelType::Gray16,
        source,
        1 * sizeof(uint16_t),
        1,
        1,
        destination,
        3 * sizeof(uint16_t),
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint16_t expected_result[3 * 3] = { 0, 0, 0,  0, 88, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3 * sizeof(uint16_t)), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case2_Gray16)
{
    // we test this arrangement
    // +---+
    // |   |
    // |  *|
    // |   |
    // +---+

    uint16_t source[1] = { 88 };
    uint16_t destination[3 * 3];

    memset(destination, 42, 3 * 3 * sizeof(uint16_t));

    Utilities::CopyAtOffsetInfo info =
    {
        2,
        1,
        libCZI::PixelType::Gray16,
        source,
        1 * sizeof(uint16_t),
        1,
        1,
        destination,
        3 * sizeof(uint16_t),
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint16_t expected_result[3 * 3] = { 0, 0, 0,  0, 0, 88,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3 * sizeof(uint16_t)), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case3_Gray16)
{
    // we test this arrangement
    // +---+
    // |   |
    // |*  |
    // |   |
    // +---+

    uint16_t source[1] = { 88 };
    uint16_t destination[3 * 3];

    memset(destination, 42, 3 * 3 * sizeof(uint16_t));

    Utilities::CopyAtOffsetInfo info =
    {
        0,
        1,
        libCZI::PixelType::Gray16,
        source,
        1 * sizeof(uint16_t),
        1,
        1,
        destination,
        3 * sizeof(uint16_t),
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint16_t expected_result[3 * 3] = { 0, 0, 0, 88, 0, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3 * sizeof(uint16_t)), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case4_Gray16)
{
    // we test this arrangement
    // +---+
    // |   |
    // |   |
    // | * |
    // +---+

    uint16_t source[1] = { 88 };
    uint16_t destination[3 * 3];

    memset(destination, 42, 3 * 3 * sizeof(uint16_t));

    Utilities::CopyAtOffsetInfo info =
    {
        1,
        2,
        libCZI::PixelType::Gray16,
        source,
        1 * sizeof(uint16_t),
        1,
        1,
        destination,
        3 * sizeof(uint16_t),
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint16_t expected_result[3 * 3] = { 0, 0, 0, 0, 0, 0, 0, 88, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3 * sizeof(uint16_t)), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case5_Gray16)
{
    // we test this arrangement
    // +---+
    // | * |
    // |   |
    // |   |
    // +---+

    uint16_t source[1] = { 88 };
    uint16_t destination[3 * 3];

    memset(destination, 42, 3 * 3 * sizeof(uint16_t));

    Utilities::CopyAtOffsetInfo info =
    {
        1,
        0,
        libCZI::PixelType::Gray16,
        source,
        1 * sizeof(uint16_t),
        1,
        1,
        destination,
        3 * sizeof(uint16_t),
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint16_t expected_result[3 * 3] = { 0, 88, 0,  0, 0, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3 * sizeof(uint16_t)), 0);
}

TEST(Utilities, CopyBitmapAtOffsetAndClearNonCoveredArea_Case6_Gray16)
{
    // In this case, the intersection of source and destination is empty, so we expect the
    //  destination to be completely cleared

    uint16_t source[1] = { 88 };
    uint16_t destination[3 * 3];

    memset(destination, 42, 3 * 3 * sizeof(uint16_t));

    Utilities::CopyAtOffsetInfo info =
    {
        3,
        4,
        libCZI::PixelType::Gray16,
        source,
        1 * sizeof(uint16_t),
        1,
        1,
        destination,
        3 * sizeof(uint16_t),
        3,
        3
    };

    Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(info);

    static const uint16_t expected_result[3 * 3] = { 0, 0, 0,  0, 0, 0,  0, 0, 0 };

    EXPECT_EQ(memcmp(expected_result, destination, 3 * 3 * sizeof(uint16_t)), 0);
}
