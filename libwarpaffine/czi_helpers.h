// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "document_info.h"
#include "inc_libCZI.h"
#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <optional>
#include <iostream>
#include <limits>

/// This struct gather the information to uniquely identify a tile (and a brick) in a plane. If it is composed
/// of the m-index and the scene-index.
struct TileIdentifier
{
    /// The CZI scene-index of the tile.
    std::optional<int> scene_index;

    /// The CZI m-index of the tile.
    std::optional<int> m_index;

    /// Constructor.
    ///
    /// \param  scene_index The scene index.
    /// \param  m_index     The m index.
    TileIdentifier(std::optional<int> scene_index, std::optional<int> m_index)
        : scene_index(scene_index)
        , m_index(m_index)
    {
    }

    /// Default constructor (creating an instance which both scene- and m-index invalid).
    TileIdentifier()
        : scene_index(std::nullopt)
        , m_index(std::nullopt)
    {
    }

    /// Query if the scene index is valid.
    ///
    /// \returns True if the scene index is valid, false otherwise.
    bool IsSceneIndexValid() const noexcept
    {
        return scene_index.has_value();
    }

    /// Query if the m index is valid.
    ///
    /// \returns True if the m index is valid, false otherwise.
    bool IsMIndexValid() const noexcept
    {
        return m_index.has_value();
    }

    /// Create an instance where both m-index and scene-index are invalid.
    ///
    /// \returns An instance where both m-index and scene-index are invalid.
    static TileIdentifier GetForNoMIndexAndNoSceneIndex()
    {
        return TileIdentifier{ std::nullopt, std::nullopt };
    }

    /// Creates an instance where the m-index is valid (and has the value specified) and the scene-index
    /// is invalid.
    ///
    /// \param  m_index The m-index.
    ///
    /// \returns An instance where the m-index has the specified value, and the scene-index is invalid.
    static TileIdentifier GetForNoSceneIndex(int m_index)
    {
        return TileIdentifier{ std::nullopt, m_index };
    }

    /// Less-than comparison operator - the operator must be suitable for usage in a map, i.e. fullfil
    /// the "strict weak ordering" requirements.  
    ///
    /// \param  other The other object to compare to.
    ///
    /// \returns True if the first parameter is less than the second.
    bool operator<(const TileIdentifier& other) const noexcept
    {
        const int this_scene_index_or_default = this->scene_index.value_or(std::numeric_limits<int>::min());
        const int other_scene_index_or_default = other.scene_index.value_or(std::numeric_limits<int>::min());
        if (this_scene_index_or_default < other_scene_index_or_default)
        {
            return true;
        }

        if (this_scene_index_or_default > other_scene_index_or_default)
        {
            return false;
        }

        // scene_indices are equal, compare m_index
        return (this->m_index.value_or(std::numeric_limits<int>::min()) < other.m_index.value_or(std::numeric_limits<int>::min()));
    }

    /// Get an informal string describing the content of the object.
    ///
    /// \returns An informal string describing the content of the object.
    std::string ToInformalString() const
    {
        return TileIdentifier::ToInformalString(*this);
    }

    /// Get an informal string describing the content of the object.
    ///
    /// \param  tile_identifier The tile identifier.
    ///
    /// \returns An informal string describing the content of the object.
    static std::string ToInformalString(const TileIdentifier& tile_identifier)
    {
        std::stringstream ss;
        ss << "TileIdentifier{ scene_index="
            << (tile_identifier.IsSceneIndexValid() ? std::to_string(tile_identifier.scene_index.value()) : "invalid")
            << ", m_index="
            << (tile_identifier.IsMIndexValid() ? std::to_string(tile_identifier.m_index.value()) : "invalid")
            << " }";
        return ss.str();
    }
};

/// This structure combines the tile identifier and its position (in the "CZI pixel coordinate system").
struct TileIdentifierAndRect
{
    /// The tile identifier of the tile.
    TileIdentifier tile_identifier;

    /// The rectangle of the tile (in the "CZI pixel coordinate system").
    libCZI::IntRect rectangle;
};

typedef std::map<TileIdentifier, libCZI::IntRect> TileIdentifierToRectangleMap;

/// A bunch of helper functions related to "CZI-documents" are gathered here.
class CziHelpers
{
public:
    /// Extract the "DeskewDocumentInfo" from the CZI, and throw an exception if the document is not
    /// found "suitable" for our purposes.
    /// \param [out] czi_reader If non-null, the czi reader.
    /// \returns  The document information.
    static DeskewDocumentInfo GetDocumentInfo(libCZI::ICZIReader* czi_reader);

    /// If the specified document has M-index, then we gather an array with "m-index" and "region for this
    /// m-index". If the specified document does not have an m-index, we use a value of numeric_limits<int>::min()
    /// for the single item in the returned vector.
    ///
    /// \param [in] czi_reader  The czi-reader-object.
    ///
    /// \returns    A vector containing "m-index and region"-pairs.
    //static std::vector<MIndexAndRect> GetMIndexRectangles(libCZI::ICZIReader* czi_reader);

    /// Create a vector which contains the tiles in the document (identified by their tile-identifier) and 
    /// the rectangle of the tile (in the "CZI pixel coordinate system"). Depending on whether the document
    /// uses m-index or not, and scene-index or not, the tile-identifier may be composed of the m-index and
    /// the scene-index. Otherwise, the tile-identifier contains only the m-index or only the scene-index.
    /// It is also possible that both m-index and scene-index are invalid.
    ///
    /// \param [in,out] czi_reader  The CZI reader object.
    ///
    /// \returns    The tile identifier-to-rectangles map.
    static std::vector<TileIdentifierAndRect> GetTileIdentifierRectangles(libCZI::ICZIReader* czi_reader);

    /// Tries to retrieve a map "channel index - pixel type". If this information cannot be retrieved,
    /// an exception is thrown.
    /// \param [in,out] czi_reader  The CZI-reader object.
    /// \returns    The map of channels index to pixeltype.
    static std::map<int, libCZI::PixelType> GetMapOfChannelsToPixeltype(libCZI::ICZIReader* czi_reader);

    /// Create a map which associates the tiles in the document (identified by their tile-identifier) with
    /// the rectangle of the tile (in the "CZI pixel coordinate system"). Depending on whether the document
    /// uses m-index or not, and scene-index or not, the tile-identifier may be composed of the m-index and
    /// the scene-index. Otherwise, the tile-identifier contains only the m-index or only the scene-index.
    /// It is also possible that both m-index and scene-index are invalid.
    ///
    /// \param [in,out] czi_reader  The CZI reader object.
    ///
    /// \returns    The tile identifier-to-rectangles map.
    static TileIdentifierToRectangleMap DetermineTileIdentifierToRectangleMap(libCZI::ICZIReader* czi_reader);

    /// Gets the subblock which are "inside" the specified brick.
    /// Note: It is currently required/assumed that the is only one subblock per plane. In case
    /// multiple subblocks are found, an exception is thrown.
    /// \param [in,out] czi_reader          The CZI-reader object.
    /// \param          brick_coordinate    The brick coordinate.
    /// \param          tile_identifier     The tile-identifier (which specifies the brick to be considered here).
    ///
    /// \returns    A map with key "z-index" and value "subblock-index".
    static std::map<int, int> GetSubblocksForBrick(libCZI::ICZIReader* czi_reader, const libCZI::CDimCoordinate& brick_coordinate, TileIdentifier tile_identifier);

    /// Check whether document is marked as skewed in metadata - or, in other words, if the source document is
    /// an unprocessed LLS-acquisition.
    ///
    /// \param  metadata The metadata.
    ///
    /// \returns True if the document is marked as a "skewed LLS-acquisition"; false otherwise.
    static bool CheckWhetherDocumentIsMarkedAsSkewedInMetadata(const std::shared_ptr<libCZI::ICziMetadata>& metadata);

    static std::tuple<double, double> GetStagePositionFromXmlMetadata(libCZI::ISubBlock* sub_block);
};
