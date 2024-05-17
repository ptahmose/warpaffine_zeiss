// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "czi_helpers.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <map>
#include <utility>

using namespace std;
using namespace libCZI;

/*static*/DeskewDocumentInfo CziHelpers::GetDocumentInfo(libCZI::ICZIReader* czi_reader)
{
    DeskewDocumentInfo document_info;
    const auto statistics = czi_reader->GetStatistics();
    document_info.width = statistics.boundingBox.w;
    document_info.height = statistics.boundingBox.h;

    int z_count;
    if (!statistics.dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &z_count))
    {
        throw invalid_argument("no z-dimension");
    }

    document_info.depth = z_count;

    const auto czi_metadata = czi_reader->ReadMetadataSegment()->CreateMetaFromMetadataSegment();
    const auto scaling_info = czi_metadata->GetDocumentInfo()->GetScalingInfo();

    if (!scaling_info.IsScaleXValid() || !scaling_info.IsScaleYValid() || !scaling_info.IsScaleZValid())
    {
        // TODO(JBL): check the scale-x equal scale-y and probably some more...
        throw invalid_argument("no scaling information");
    }

    document_info.xy_scaling = scaling_info.scaleX;
    document_info.z_scaling = scaling_info.scaleZ;

    document_info.document_origin_x = statistics.boundingBoxLayer0Only.x;
    document_info.document_origin_y = statistics.boundingBoxLayer0Only.y;

    auto vector_mindex_and_rect = CziHelpers::GetTileIdentifierRectangles(czi_reader);

    // TODO(JBL): 
    // If we have more than one element, then the tile-identifiers must be unique (and there must not be an invalid m-index). We should
    // check for this here.
    for (const auto& item : vector_mindex_and_rect)
    {
        BrickInPlaneIdentifier brick_identifier;
        brick_identifier.m_index = item.tile_identifier.m_index.value_or(std::numeric_limits<int>::min());
        brick_identifier.s_index = item.tile_identifier.scene_index.value_or(std::numeric_limits<int>::min());
        BrickRectPositionInfo position_info{ item.rectangle.x, item.rectangle.y, static_cast<uint32_t>(item.rectangle.w), static_cast<uint32_t>(item.rectangle.h) };
        document_info.map_brickid_position.insert(pair<BrickInPlaneIdentifier, BrickRectPositionInfo>(brick_identifier, position_info));
    }

    document_info.map_channelindex_pixeltype = CziHelpers::GetMapOfChannelsToPixeltype(czi_reader);

    return document_info;
}

/*static*/std::vector<TileIdentifierAndRect> CziHelpers::GetTileIdentifierRectangles(libCZI::ICZIReader* czi_reader)
{
    std::vector<TileIdentifierAndRect> tile_identifier_and_rect;
    const auto statistics = czi_reader->GetStatistics();
    if (!statistics.IsMIndexValid() && statistics.sceneBoundingBoxes.empty())
    {
        // the document does not contain an m-index and no scene index -> we assume that we do not have a tiled document

        // TODO(JBL): - we should check that we really have only one subblock per plane
        tile_identifier_and_rect.emplace_back(
            TileIdentifierAndRect
            {
                TileIdentifier::GetForNoMIndexAndNoSceneIndex(),
                statistics.boundingBoxLayer0Only
            });

        return tile_identifier_and_rect;
    }

    if (statistics.sceneBoundingBoxes.empty())
    {
        // this document does not contain a scene index, but it does contain an m-index

        // simplest possible implementation: we calculate the AABB for all subblocks
        for (int m = statistics.minMindex; m <= statistics.maxMindex; ++m)
        {
            int min_x{ numeric_limits<int>::max() },
                min_y{ numeric_limits<int>::max() },
                max_x{ numeric_limits<int>::min() },
                max_y{ numeric_limits<int>::min() };
            czi_reader->EnumSubset(
                nullptr,
                nullptr,
                true,
                [&](int index, const SubBlockInfo& info)->bool
                {
                    if (info.mIndex == m)
                    {
                        min_x = min(min_x, info.logicalRect.x);
                        min_y = min(min_y, info.logicalRect.y);
                        max_x = max(max_x, info.logicalRect.x + info.logicalRect.w);
                        max_y = max(max_y, info.logicalRect.y + info.logicalRect.h);
                    }

                    return true;
                });

            if (min_x == numeric_limits<int>::max() || min_y == numeric_limits<int>::max() ||
                max_x == numeric_limits<int>::min() || max_y == numeric_limits<int>::min())
            {
                // we haven't found any subblock for this m-index -> we skip it
                continue;
            }

            const IntRect rectangle{ min_x, min_y, max_x - min_x, max_y - min_y };
            tile_identifier_and_rect.emplace_back(TileIdentifierAndRect{ TileIdentifier::GetForNoSceneIndex(m), rectangle });
        }
    }
    else
    {
        // this document does contain a scene index

        for (const auto& scene_bounding_box : statistics.sceneBoundingBoxes)
        {
            for (int m = statistics.minMindex; m <= statistics.maxMindex; ++m)
            {
                int min_x{ numeric_limits<int>::max() },
                    min_y{ numeric_limits<int>::max() },
                    max_x{ numeric_limits<int>::min() },
                    max_y{ numeric_limits<int>::min() };
                czi_reader->EnumSubset(
                    nullptr,
                    nullptr,
                    true,
                    [&](int index, const SubBlockInfo& info)->bool
                    {
                        int scene_index;
                        if (info.coordinate.TryGetPosition(DimensionIndex::S, &scene_index))
                        {
                            if (scene_index == scene_bounding_box.first && info.mIndex == m)
                            {
                                min_x = min(min_x, info.logicalRect.x);
                                min_y = min(min_y, info.logicalRect.y);
                                max_x = max(max_x, info.logicalRect.x + info.logicalRect.w);
                                max_y = max(max_y, info.logicalRect.y + info.logicalRect.h);
                            }
                        }

                        return true;
                    });

                if (min_x == numeric_limits<int>::max() || min_y == numeric_limits<int>::max() ||
                    max_x == numeric_limits<int>::min() || max_y == numeric_limits<int>::min())
                {
                    // we haven't found any subblock for this m-index and scene-index -> we skip it

                    // TODO(JBL): should we check that there is at least one m for the scene?
                    continue;
                }

                const IntRect rectangle{ min_x, min_y, max_x - min_x, max_y - min_y };
                tile_identifier_and_rect.emplace_back(TileIdentifierAndRect{ TileIdentifier(scene_bounding_box.first, m), rectangle });
            }
        }
    }

    return tile_identifier_and_rect;
}

/*static*/std::map<int, libCZI::PixelType> CziHelpers::GetMapOfChannelsToPixeltype(libCZI::ICZIReader* czi_reader)
{
    const auto statistics = czi_reader->GetStatistics();
    int channelCount;
    if (!statistics.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &channelCount))
    {
        throw invalid_argument("The document must have a C-dimension.");
    }

    map<int, libCZI::PixelType> map_channelno_to_pixeltype;
    for (int c = 0; c < channelCount; ++c)
    {
        SubBlockInfo subblock_info_of_channel;
        if (!czi_reader->TryGetSubBlockInfoOfArbitrarySubBlockInChannel(c, subblock_info_of_channel))
        {
            ostringstream string_stream;
            string_stream << "Unable to determine pixeltype for C=" << c << ".";
            throw invalid_argument(string_stream.str());
        }

        map_channelno_to_pixeltype[c] = subblock_info_of_channel.pixelType;
    }

    return map_channelno_to_pixeltype;
}

/*static*/TileIdentifierToRectangleMap CziHelpers::DetermineTileIdentifierToRectangleMap(libCZI::ICZIReader* czi_reader)
{
    TileIdentifierToRectangleMap tile_identifier_and_rect;
    const auto statistics = czi_reader->GetStatistics();
    if (!statistics.IsMIndexValid() && /*!statistics.dimBounds.TryGetInterval(DimensionIndex::S, nullptr, nullptr)*/statistics.sceneBoundingBoxes.empty())
    {
        // Having no M-index is considered "valid", but then we assume that we do not have a tiled document.
        // TODO(JBL): we'd need to verify this assumption
        tile_identifier_and_rect[TileIdentifier::GetForNoMIndexAndNoSceneIndex()] = statistics.boundingBoxLayer0Only;
        return tile_identifier_and_rect;
    }

    if (/*!statistics.dimBounds.TryGetInterval(DimensionIndex::S, nullptr, nullptr)*/statistics.sceneBoundingBoxes.empty())
    {
        // there is no S-index, but there is an M-index -> we assume that we have a tiled document

        // simplest possible implementation: we calculate the AABB for all subblocks
        for (int m = statistics.minMindex; m <= statistics.maxMindex; ++m)
        {
            int min_x{ numeric_limits<int>::max() },
                min_y{ numeric_limits<int>::max() },
                max_x{ numeric_limits<int>::min() },
                max_y{ numeric_limits<int>::min() };
            czi_reader->EnumSubset(
                nullptr,
                nullptr,
                true,
                [&](int index, const SubBlockInfo& info)->bool
                {
                    if (info.mIndex == m)
                    {
                        min_x = min(min_x, info.logicalRect.x);
                        min_y = min(min_y, info.logicalRect.y);
                        max_x = max(max_x, info.logicalRect.x + info.logicalRect.w);
                        max_y = max(max_y, info.logicalRect.y + info.logicalRect.h);
                    }

                    return true;
                });

            if (min_x == numeric_limits<int>::max() || min_y == numeric_limits<int>::max() ||
                max_x == numeric_limits<int>::min() || max_y == numeric_limits<int>::min())
            {
                continue;
            }

            IntRect rectangle{ min_x, min_y, max_x - min_x, max_y - min_y };
            tile_identifier_and_rect[TileIdentifier::GetForNoSceneIndex(m)] = rectangle;
        }
    }
    else
    {
        for (const auto& scene_bounding_box : statistics.sceneBoundingBoxes)
        {
            for (int m = statistics.minMindex; m <= statistics.maxMindex; ++m)
            {
                int min_x{ numeric_limits<int>::max() },
                    min_y{ numeric_limits<int>::max() },
                    max_x{ numeric_limits<int>::min() },
                    max_y{ numeric_limits<int>::min() };
                czi_reader->EnumSubset(
                                    nullptr,
                                    nullptr,
                                    true,
                                    [&](int index, const SubBlockInfo& info)->bool
                                    {
                                        int scene_index;
                                        if (info.coordinate.TryGetPosition(DimensionIndex::S, &scene_index))
                                        {
                                            if (scene_index == scene_bounding_box.first && info.mIndex == m)
                                            {
                                                min_x = min(min_x, info.logicalRect.x);
                                                min_y = min(min_y, info.logicalRect.y);
                                                max_x = max(max_x, info.logicalRect.x + info.logicalRect.w);
                                                max_y = max(max_y, info.logicalRect.y + info.logicalRect.h);
                                            }
                                        }

                                        return true;
                                    });

                if (min_x == numeric_limits<int>::max() || min_y == numeric_limits<int>::max() ||
                    max_x == numeric_limits<int>::min() || max_y == numeric_limits<int>::min())
                {
                    continue;
                }

                tile_identifier_and_rect[TileIdentifier(scene_bounding_box.first, m)] = IntRect{ min_x, min_y, max_x - min_x, max_y - min_y };
            }
        }
    }

    return tile_identifier_and_rect;
}

static bool IsCoordinateInBrick(const libCZI::CDimCoordinate& brick_coordinate, TileIdentifier tile_identifier, const SubBlockInfo& info)
{
    if (tile_identifier.IsMIndexValid())
    {
        if (info.IsMindexValid() && info.mIndex != tile_identifier.m_index)
        {
            return false;
        }
    }

    if (tile_identifier.IsSceneIndexValid())
    {
        int scene_index;
        if (!info.coordinate.TryGetPosition(DimensionIndex::S, &scene_index))
        {
            return false;
        }

        if (scene_index != tile_identifier.scene_index)
        {
            return false;
        }
    }

    /*    if (Utils::IsValidMindex(m_index))
        {
            if (info.IsMindexValid() && info.mIndex != m_index)
            {
                return false;
            }

            // TODO(JBL): what to do is info.mIndex is not valid? I guess - we should bail out then?
        }*/

        // we check whether all dimension present in "brick_coordinate" are present in the subblock, and that
        //  they have the same value -> then we consider this subblock as being "in the brick"
    bool is_equal = true;
    brick_coordinate.EnumValidDimensions(
        [&](libCZI::DimensionIndex dim, int value)->bool
        {
            int coordinate_brick;
            brick_coordinate.TryGetPosition(dim, &coordinate_brick);
            int coordinate_subblock;
            if (!info.coordinate.TryGetPosition(dim, &coordinate_subblock))
            {
                is_equal = false;
                return false;
            }

            if (coordinate_brick != coordinate_subblock)
            {
                is_equal = false;
                return false;
            }

            return true;
        });

    return is_equal;
}


static bool IsCoordinateInBrick(const libCZI::CDimCoordinate& brick_coordinate, int m_index, const SubBlockInfo& info)
{
    if (Utils::IsValidMindex(m_index))
    {
        if (info.IsMindexValid() && info.mIndex != m_index)
        {
            return false;
        }

        // TODO(JBL): what to do is info.mIndex is not valid? I guess - we should bail out then?
    }

    // we check whether all dimension present in "brick_coordinate" are present in the subblock, and that
    //  they have the same value -> then we consider this subblock as being "in the brick"
    bool is_equal = true;
    brick_coordinate.EnumValidDimensions(
        [&](libCZI::DimensionIndex dim, int value)->bool
        {
            int coordinate_brick;
            brick_coordinate.TryGetPosition(dim, &coordinate_brick);
            int coordinate_subblock;
            if (!info.coordinate.TryGetPosition(dim, &coordinate_subblock))
            {
                is_equal = false;
                return false;
            }

            if (coordinate_brick != coordinate_subblock)
            {
                is_equal = false;
                return false;
            }

            return true;
        });

    return is_equal;
}

/*static*/std::map<int, int> CziHelpers::GetSubblocksForBrick(libCZI::ICZIReader* czi_reader, const libCZI::CDimCoordinate& brick_coordinate, /*int m_index*/TileIdentifier tile_identifier)
{
    map<int, int> map_z_subblockindex;

    czi_reader->EnumerateSubBlocks(
        [&](int index, const SubBlockInfo& info)->bool
        {
            if (IsCoordinateInBrick(brick_coordinate, tile_identifier, info))
            {
                // for the time being, we only support/expect to have exactly one subblock per z
                int z;
                info.coordinate.TryGetPosition(DimensionIndex::Z, &z);
                const auto iterator = map_z_subblockindex.insert(pair<int, int>(z, index));

                // c.f. https://cplusplus.com/reference/map/map/insert/ -> the 2nd element is set to true
                //                                                         if the element was inserted, false if it existed before
                if (iterator.second != true)
                {
                    throw logic_error("more than one subblock");
                }
            }

            return true;
        });

    return map_z_subblockindex;
}

/*static*/bool CziHelpers::CheckWhetherDocumentIsMarkedAsSkewedInMetadata(const std::shared_ptr<libCZI::ICziMetadata>& metadata)
{
    auto dimensions_z_node = metadata->GetChildNodeReadonly("ImageDocument/Metadata/Information/Image/Dimensions/Z/ZAxisShear");
    if (dimensions_z_node)
    {
        wstring value;
        if (dimensions_z_node->TryGetValue(&value))
        {
            return value == L"Skew60";
        }
    }

    return false;
}
