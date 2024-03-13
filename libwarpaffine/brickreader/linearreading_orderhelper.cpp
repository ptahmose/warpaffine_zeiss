// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "linearreading_orderhelper.h"
#include <algorithm>
#include <vector>
#include <utility>

using namespace std;
using namespace libCZI;

/*static*/LinearReadingOrderHelper::CziDocumentInfo LinearReadingOrderHelper::GetCziDocumentInfo(libCZI::ICZIReader* subblock_repository)
{
    CziDocumentInfo document_info;
    document_info.statistics = subblock_repository->GetStatistics();
    int z_count;
    document_info.statistics.dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &z_count);
    document_info.no_of_z = (uint32_t)z_count;
    return document_info;
}

/*static*/LinearReadingOrderHelper::InitialInspectionResult LinearReadingOrderHelper::CreateInitialInspectionResult(libCZI::ICZIReader* subblock_repository, const CziDocumentInfo& info)
{
    InitialInspectionResult result;
    int t_count, c_count;
    info.statistics.dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count);
    info.statistics.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count);
    for (int t = 0; t < t_count; ++t)
    {
        for (int c = 0; c < c_count; ++c)
        {
            result.number_of_slices_per_brick[BrickCoordinate{ t, c }] = 0;
        }
    }

    result.subblocks_ordered_by_fileposition.reserve(info.statistics.subBlockCount);
    std::vector<std::uint64_t> filepositions;
    filepositions.reserve(info.statistics.subBlockCount);

    // TODO(JBL): we could check here whether they are already in correct order, and then skip the sort altogether
    subblock_repository->EnumerateSubBlocksEx(
        [&](int index, const DirectorySubBlockInfo& info)->bool
        {
            result.subblocks_ordered_by_fileposition.emplace_back(index);
            filepositions.emplace_back(info.filePosition);

            int t_coordinate, c_coordinate;
            info.coordinate.TryGetPosition(DimensionIndex::T, &t_coordinate);
            info.coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
            ++result.number_of_slices_per_brick[BrickCoordinate{ t_coordinate, c_coordinate }];

            return true;
        });

    sort(
        result.subblocks_ordered_by_fileposition.begin(),
        result.subblocks_ordered_by_fileposition.end(),
        [&](int a, int b)
        {
            return filepositions[a] < filepositions[b];
        });

    return result;
}

/*static*/std::vector<int> LinearReadingOrderHelper::CreateOrder_OrderedByFilePosition(libCZI::ICZIReader* subblock_repository, const CziDocumentInfo& info)
{
    std::vector<int> subblocks_order_by_fileposition;
    subblocks_order_by_fileposition.reserve(info.statistics.subBlockCount);
    std::vector<std::uint64_t> filepositions;
    filepositions.reserve(info.statistics.subBlockCount);

    // TODO(JBL): we could check here whether they are already in correct order, and then skip the sort altogether
    subblock_repository->EnumerateSubBlocksEx(
        [&](int index, const DirectorySubBlockInfo& info)->bool
        {
            subblocks_order_by_fileposition.emplace_back(index);
            filepositions.emplace_back(info.filePosition);
            return true;
        });

    sort(
        subblocks_order_by_fileposition.begin(),
        subblocks_order_by_fileposition.end(),
        [&](int a, int b)
        {
            return filepositions[a] < filepositions[b];
        });

    return subblocks_order_by_fileposition;
}

/*static*/BrickCoordinate LinearReadingOrderHelper::BrickCoordinateFromSubBlockInfo(const libCZI::SubBlockInfo& subblock_info)
{
    BrickCoordinate brick_coordinate;
    bool b = subblock_info.coordinate.TryGetPosition(DimensionIndex::T, &brick_coordinate.t);
    b = subblock_info.coordinate.TryGetPosition(DimensionIndex::C, &brick_coordinate.c);
    return brick_coordinate;
}

/*static*/LinearReadingOrderHelper::OrderReadingResult LinearReadingOrderHelper::DetermineOrder(libCZI::ICZIReader* subblock_repository, const ReadingConstraints& options)
{
    const auto document_info = GetCziDocumentInfo(subblock_repository);
    //auto ordered_by_fileposition = CreateOrder_OrderedByFilePosition(subblock_repository, document_info);
    auto initial_inspection_result = CreateInitialInspectionResult(subblock_repository, document_info);
    //auto ordered_by_fileposition = initial_inspection_result.subblocks_ordered_by_fileposition;

    OrderReadingResult result;

    map<BrickCoordinate, UnfinishedBrickInfo> unfinished_bricks;
    BrickCoordinate brick_coordinate_of_brick_it_was_reordered_for;
    brick_coordinate_of_brick_it_was_reordered_for.MakeInvalid();
    uint32_t subblocks_currently_inflight = 0;

    for (size_t i = 0; i < initial_inspection_result.subblocks_ordered_by_fileposition.size(); ++i)
    {
        int subblock_index = initial_inspection_result.subblocks_ordered_by_fileposition[i];

        SubBlockInfo subblock_info;
        bool b = subblock_repository->TryGetSubBlockInfo(subblock_index, &subblock_info);
        BrickCoordinate brick_coordinate = BrickCoordinateFromSubBlockInfo(subblock_info);

        // so, ok, is this a new brick or have we seen this before?
        auto unfinished_brick_info_iterator = unfinished_bricks.find(brick_coordinate);
        if (unfinished_brick_info_iterator == unfinished_bricks.end())
        {
            unfinished_brick_info_iterator = get<0>(unfinished_bricks.emplace(
                brick_coordinate, UnfinishedBrickInfo{ /*document_info.no_of_z*/initial_inspection_result.number_of_slices_per_brick[brick_coordinate], 1 }));
            ++subblocks_currently_inflight;
        }
        else
        {
            ++(unfinished_brick_info_iterator->second.number_of_subblocks_present_for_brick);
            ++subblocks_currently_inflight;
            if (unfinished_brick_info_iterator->second.number_of_subblocks_present_for_brick == unfinished_brick_info_iterator->second.number_of_subblocks_required_for_brick)
            {
                // subtract this from the number of currently allocated subblocks
                subblocks_currently_inflight -= unfinished_brick_info_iterator->second.number_of_subblocks_present_for_brick;

                // ok, then this brick is done and fine, let's remove it from the map
                unfinished_bricks.erase(unfinished_brick_info_iterator);
            }
        }

        // If we have too many items in flight, we re-order the remaining items so that a brick is completed next.
        // Re-ordering only makes sense if there is more than one item remaining.
        if (brick_coordinate != brick_coordinate_of_brick_it_was_reordered_for)
        {
            brick_coordinate_of_brick_it_was_reordered_for.MakeInvalid();
            if (subblocks_currently_inflight >= options.max_number_of_subblocks_inflight &&
                i + 2 < initial_inspection_result.subblocks_ordered_by_fileposition.size())
            {
                // ok, so now we change the order so that all remaining subblocks for the brick which has
                //  the least amount of missing subblock come next
                const auto brick_with_lowest_number_of_subblocks_missing = min_element(unfinished_bricks.begin(), unfinished_bricks.end(),
                    [=](const pair<BrickCoordinate, UnfinishedBrickInfo>& first_element, const pair< BrickCoordinate, UnfinishedBrickInfo>& second_element)->bool
                    {
                        return first_element.second.CalcNumberOfSubblocksMissing() < second_element.second.CalcNumberOfSubblocksMissing();
                    });

                LinearReadingOrderHelper::Reorder(
                    subblock_repository,
                    initial_inspection_result.subblocks_ordered_by_fileposition,
                    i + 1,
                    *brick_with_lowest_number_of_subblocks_missing);

                // After we did a re-ordering, we don't have to do this again for the next iterations (where we have
                //  ensured that the brick we chosen above gets completed as fast as possible).
                brick_coordinate_of_brick_it_was_reordered_for = brick_coordinate;
            }
        }

        // keep track of the "largest number of subblocks-in-flight"
        if (result.max_number_of_subblocks_inflight < subblocks_currently_inflight)
        {
            result.max_number_of_subblocks_inflight = subblocks_currently_inflight;
        }
    }

    result.reading_order = std::move(initial_inspection_result.subblocks_ordered_by_fileposition);
    result.number_of_slices_per_brick = std::move(initial_inspection_result.number_of_slices_per_brick);
    return result;
}

/*static*/void LinearReadingOrderHelper::Reorder(libCZI::ICZIReader* subblock_repository, std::vector<int>& subblocks_list, size_t index_where_to_insert, const std::pair<BrickCoordinate, UnfinishedBrickInfo>& brick)
{
    // what we do here is:
    // * starting at the specified index, we scan the remainder for subblocks belonging to the specified brick
    // * if we find one, we move (or: swap) it with the position behind the index

    uint32_t no_of_subblocks_missing_for_brick = brick.second.CalcNumberOfSubblocksMissing();
    for (size_t index = index_where_to_insert; index < subblocks_list.size(); ++index)
    {
        int subblock_index = subblocks_list[index];
        SubBlockInfo subblock_info;
        bool b = subblock_repository->TryGetSubBlockInfo(subblock_index, &subblock_info);
        BrickCoordinate brick_coordinate = BrickCoordinateFromSubBlockInfo(subblock_info);
        if (brick_coordinate == brick.first)
        {
            swap(subblocks_list[index_where_to_insert], subblocks_list[index]);
            ++index_where_to_insert;

            if (--no_of_subblocks_missing_for_brick == 0)
            {
                // we can exit the loop early if we found all subblocks required for the brick
                break;
            }
        }
    }
}
