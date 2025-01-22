// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <map>
#include <sstream>
#include "inc_libCZI.h"

/// This struct is used to uniquely identify a brick (within the document). A brick here is defined
/// as a stack of subblocks ("stacked" in Z), and we assume that all subblocks identified with this
/// structure are at the same x-y-position (in pixel-coordinate-system) and have same extent.
/// Note: In a well-formed CZI, the m-index is unique within a scene. However - what complicates matters
/// is that an s-index may or may not be present.
struct BrickInPlaneIdentifier
{
    int m_index{ std::numeric_limits<int>::max() }; ///< The m-index. A value of max-int or min-int means - not valid.
    int s_index{ std::numeric_limits<int>::max() }; ///< The s-index. A value of max-int or min-int means - not valid.

    bool IsMIndexValid() const { return this->m_index != std::numeric_limits<int>::max() && this->m_index != std::numeric_limits<int>::min(); }
    bool IsSIndexValid() const { return this->s_index != std::numeric_limits<int>::max() && this->s_index != std::numeric_limits<int>::min(); }

    /// Less-than comparison operator - required to use this struct as a key in a map.
    /// \param  other The object to compare with.
    /// \returns {bool} True if the current object appears before the specified object.
    bool operator<(const BrickInPlaneIdentifier& other) const
    {
        if (this->IsMIndexValid() && this->IsSIndexValid())
        {
            // s has the highest precedence
            return this->s_index < other.s_index || (this->s_index == other.s_index && this->m_index < other.m_index);
        }
        else if (this->IsMIndexValid() && !this->IsSIndexValid())
        {
            return this->m_index < other.m_index;
        }
        else if (!this->IsMIndexValid() && this->IsSIndexValid())
        {
            return this->s_index < other.s_index;
        }

        return false;
    }

    /// Equality operator - all "valid" fields must be equal. If all fields are invalid, then the
    /// two objects are considered equal.
    ///
    /// \param  other   The other object to compare with.
    ///
    /// \returns    True if the parameters are considered equal.
    bool operator==(const BrickInPlaneIdentifier& other) const
    {
        if (this->IsMIndexValid() && this->IsSIndexValid())
        {
            return this->m_index == other.m_index && this->s_index == other.s_index;
        }
        else if (this->IsMIndexValid() && !this->IsSIndexValid())
        {
            return this->m_index == other.m_index;
        }
        else if (!this->IsMIndexValid() && this->IsSIndexValid())
        {
            return this->s_index == other.s_index;
        }

        return true;
    }

    /// Inequality operator - just the negation of the equality operator.
    ///
    /// \param  other   The other object to compare with.
    ///
    /// \returns    True if the parameters are not considered equal.
    bool operator!= (const BrickInPlaneIdentifier& other) const
    {
        return !this->operator==(other);
    }

    /// Gets an informal string representation of this object.
    ///
    /// \returns    The informal string representation.
    std::string AsInformalString() const
    {
        if (!this->IsMIndexValid() && !this->IsSIndexValid())
        {
            return "<NoTiles>";
        }

        std::ostringstream string_stream;
        string_stream << "<";
        if (this->IsMIndexValid())
        {
            string_stream << "M=" << this->m_index;
        }

        if (this->IsSIndexValid() && IsMIndexValid())
        {
            string_stream << ",";
        }

        if (this->IsSIndexValid())
        {
            string_stream << "S=" << this->s_index;
        }

        string_stream << ">";
        return string_stream.str();
    }
};

struct BrickRectPositionInfo
{
    BrickRectPositionInfo(int x_position, int y_position, std::uint32_t width, std::uint32_t height)
        : x_position(x_position), y_position(y_position), width(width), height(height)
    {}

    int x_position{ 0 };        ///< The x-position of the tile in CZI-document's pixel-coordinate-system.
    int y_position{ 0 };        ///< The y-position of the tile in CZI-document's pixel-coordinate-system.
    std::uint32_t width{ 0 };   ///< The width of the tile in CZI-document's pixel-coordinate-system.
    std::uint32_t height{ 0 };  ///< The height of the tile in CZI-document's pixel-coordinate-system.
};

/// This structure gathers information about the document which is relevant for the deskew-operation.
struct DeskewDocumentInfo
{
    std::uint32_t   width{ 0 };     ///< Size of the document in x-direction in pixels.
    std::uint32_t   height{ 0 };    ///< Size of the document in y-direction in pixels.
    std::uint32_t   depth{ 0 };     ///< Size of the document in z-direction in pixels.

    int document_origin_x;          ///< The x-position of the top-left of the document, or - the origin of the pixel-coordinate system.
    int document_origin_y;          ///< The y-position of the top-left of the document, or - the origin of the pixel-coordinate system.

    /// A map containing all the "stacks" in the source document.
    std::map<BrickInPlaneIdentifier, BrickRectPositionInfo> map_brickid_position;

    /// A map which associates the channel index with the pixel-type.
    std::map<int, libCZI::PixelType> map_channelindex_pixeltype;

    double          z_scaling{ std::numeric_limits<double>::quiet_NaN() };  ///< The size of one pixel in z-direction in units of micro-meters.
    double          xy_scaling{ std::numeric_limits<double>::quiet_NaN() }; ///< The size of one pixel in x- or y-direction in units of micro-meters.

    /// The angle between the light sheet illumination and the vertical direction,  i.e., the normal of the cover glass.
    /// This also defines the tilt of the measurement planes with respect to the vertical direction.
    const double illumination_angle_in_radians = 60.0 / 180.0 * 3.14159265358979323846;
};

