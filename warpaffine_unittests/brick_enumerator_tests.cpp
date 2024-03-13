// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include "../libwarpaffine/brickreader/brick_enumerator.h"

using namespace std;
using namespace libCZI;

static bool CompareDimCoordinateForEquality(const libCZI::CDimCoordinate& a, const libCZI::CDimCoordinate& b)
{
    return libCZI::Utils::Compare(&a, &b) == 0;
}

TEST(BrickEnumerator, TestWithMaxC1AndOneRegion)
{
    BrickEnumerator brick_enumerator;
    brick_enumerator.Reset(nullopt, 1, TileIdentifierToRectangleMap{ { { nullopt, 0}, { 0, 0, 10, 11}} });

    libCZI::CDimCoordinate coordinate;
    TileIdentifier tile_identifier;
    libCZI::IntRect rectangle;

    bool b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_FALSE(b);
}

TEST(BrickEnumerator, TestWithMaxC2AndOneRegion)
{
    BrickEnumerator brick_enumerator;
    brick_enumerator.Reset(nullopt, 2, TileIdentifierToRectangleMap{ {{ nullopt, 0}, { 0, 0, 10, 11}} });

    libCZI::CDimCoordinate coordinate;
    TileIdentifier tile_identifier;
    libCZI::IntRect rectangle;

    bool b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_FALSE(b);
}

TEST(BrickEnumerator, TestWithMaxC1AndTwoRegions)
{
    BrickEnumerator brick_enumerator;
    brick_enumerator.Reset(nullopt, 1, TileIdentifierToRectangleMap{ {{ nullopt, 0}, { 0, 0, 10, 11}}, {{ nullopt, 1}, { 1, 1, 20, 21}} });

    libCZI::CDimCoordinate coordinate;
    TileIdentifier tile_identifier;
    libCZI::IntRect rectangle;

    bool b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_FALSE(b);
}

TEST(BrickEnumerator, TestWithMaxC2AndTwoRegions)
{
    BrickEnumerator brick_enumerator;
    brick_enumerator.Reset(nullopt, 2, TileIdentifierToRectangleMap{ {{ nullopt, 0}, { 0, 0, 10, 11}}, {{ nullopt, 1}, { 1, 1, 20, 21}} });

    libCZI::CDimCoordinate coordinate;
    TileIdentifier tile_identifier;
    libCZI::IntRect rectangle{};

    bool b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_FALSE(b);
}

TEST(BrickEnumerator, TestWithMaxT1MaxC2AndTwoRegions)
{
    BrickEnumerator brick_enumerator;
    brick_enumerator.Reset(1, 2, TileIdentifierToRectangleMap{ {{ nullopt, 0}, { 0, 0, 10, 11}}, {{ nullopt, 1}, { 1, 1, 20, 21}} });

    libCZI::CDimCoordinate coordinate;
    TileIdentifier tile_identifier;
    libCZI::IntRect rectangle{};

    bool b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_FALSE(b);
}

TEST(BrickEnumerator, TestWithMaxT2MaxC2AndTwoRegions)
{
    BrickEnumerator brick_enumerator;
    brick_enumerator.Reset(2, 2, TileIdentifierToRectangleMap{ {{ nullopt, 0}, { 0, 0, 10, 11}}, {{ nullopt, 1}, { 1, 1, 20, 21}} });

    libCZI::CDimCoordinate coordinate;
    TileIdentifier tile_identifier;
    libCZI::IntRect rectangle{};

    bool b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0T1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1T1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 0);
    EXPECT_TRUE(rectangle.x == 0 && rectangle.y == 0 && rectangle.w == 10 && rectangle.h == 11);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1T0")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C0T1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);
    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_TRUE(b);
    EXPECT_TRUE(CompareDimCoordinateForEquality(coordinate, CDimCoordinate::Parse("C1T1")));
    EXPECT_TRUE(!tile_identifier.IsSceneIndexValid() && tile_identifier.IsMIndexValid() && tile_identifier.m_index.value() == 1);
    EXPECT_TRUE(rectangle.x == 1 && rectangle.y == 1 && rectangle.w == 20 && rectangle.h == 21);

    b = brick_enumerator.GetNextBrickCoordinate(coordinate, tile_identifier, rectangle);
    EXPECT_FALSE(b);
}
