// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <iterator>
#include <warpafine_unittests_config.h>
#include "../libwarpaffine/cmdlineoptions.h"
#include "../libwarpaffine/document_info.h"
#include "../libwarpaffine/utilities.h"

TEST(CmdLineOptions, IlluminationAngleNotSpecified_ReturnsNullopt)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi" };

    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));

    ASSERT_EQ(result, CCmdLineOptions::ParseResult::OK);
    EXPECT_FALSE(options.GetIlluminationAngleOverride().has_value());
}

TEST(CmdLineOptions, IlluminationAngleSpecified_ReturnsValue)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi", "--illumination-angle", "45" };

    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));

    ASSERT_EQ(result, CCmdLineOptions::ParseResult::OK);
    ASSERT_TRUE(options.GetIlluminationAngleOverride().has_value());
    EXPECT_DOUBLE_EQ(options.GetIlluminationAngleOverride().value(), 45.0);
}

TEST(CmdLineOptions, IlluminationAngleAt0Degrees_IsValid)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi", "--illumination-angle", "0" };

    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));

    ASSERT_EQ(result, CCmdLineOptions::ParseResult::OK);
    ASSERT_TRUE(options.GetIlluminationAngleOverride().has_value());
    EXPECT_DOUBLE_EQ(options.GetIlluminationAngleOverride().value(), 0.0);
}

TEST(CmdLineOptions, IlluminationAngleAt90Degrees_IsValid)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi", "--illumination-angle", "90" };

    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));

    ASSERT_EQ(result, CCmdLineOptions::ParseResult::OK);
    ASSERT_TRUE(options.GetIlluminationAngleOverride().has_value());
    EXPECT_DOUBLE_EQ(options.GetIlluminationAngleOverride().value(), 90.0);
}

TEST(CmdLineOptions, IlluminationAngleNegative_IsInvalid)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi", "--illumination-angle", "-1" };

    testing::internal::CaptureStderr();
    testing::internal::CaptureStdout();
    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));
    const auto ignored_err = testing::internal::GetCapturedStderr();
    const auto ignored_out = testing::internal::GetCapturedStdout();
    (void)ignored_err;
    (void)ignored_out;

    EXPECT_EQ(result, CCmdLineOptions::ParseResult::Error);
}

TEST(CmdLineOptions, IlluminationAngleAbove90_IsInvalid)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi", "--illumination-angle", "91" };

    testing::internal::CaptureStderr();
    testing::internal::CaptureStdout();
    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));
    const auto ignored_err = testing::internal::GetCapturedStderr();
    const auto ignored_out = testing::internal::GetCapturedStdout();
    (void)ignored_err;
    (void)ignored_out;

    EXPECT_EQ(result, CCmdLineOptions::ParseResult::Error);
}

TEST(CmdLineOptions, IlluminationAngleFractional_IsValid)
{
    CCmdLineOptions options;
    static const char* argv[] = { "warpaffine", "-s", "input.czi", "-d", "output.czi", "--illumination-angle", "60.5" };

    const auto result = options.Parse(std::size(argv), const_cast<char**>(argv));

    ASSERT_EQ(result, CCmdLineOptions::ParseResult::OK);
    ASSERT_TRUE(options.GetIlluminationAngleOverride().has_value());
    EXPECT_DOUBLE_EQ(options.GetIlluminationAngleOverride().value(), 60.5);
}

// Test the DeskewDocumentInfo::SetIlluminationAngleInDegrees function
TEST(DeskewDocumentInfo, SetIlluminationAngleInDegrees_ConvertsCorrectly)
{
    DeskewDocumentInfo doc_info;

    doc_info.SetIlluminationAngleInDegrees(60.0);
    EXPECT_NEAR(doc_info.illumination_angle_in_radians, Utilities::DegreesToRadians(60.0), 1e-10);

    doc_info.SetIlluminationAngleInDegrees(45.0);
    EXPECT_NEAR(doc_info.illumination_angle_in_radians, Utilities::DegreesToRadians(45.0), 1e-10);

    doc_info.SetIlluminationAngleInDegrees(0.0);
    EXPECT_NEAR(doc_info.illumination_angle_in_radians, 0.0, 1e-10);

    doc_info.SetIlluminationAngleInDegrees(90.0);
    EXPECT_NEAR(doc_info.illumination_angle_in_radians, Utilities::DegreesToRadians(90.0), 1e-10);
}

TEST(DeskewDocumentInfo, DefaultIlluminationAngle_Is60Degrees)
{
    DeskewDocumentInfo doc_info;

    // Default should be 60 degrees
    EXPECT_NEAR(doc_info.illumination_angle_in_radians, Utilities::DegreesToRadians(60.0), 1e-10);
}
