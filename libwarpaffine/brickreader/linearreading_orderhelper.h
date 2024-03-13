// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "../inc_libCZI.h"
#include "brick_coordinate.h"

class LinearReadingOrderHelper
{
private:
    struct CziDocumentInfo
    {
        libCZI::SubBlockStatistics statistics;
        std::uint32_t   no_of_z{ 0 };
    };

    struct UnfinishedBrickInfo
    {
        std::uint32_t        number_of_subblocks_required_for_brick{ 0 };
        std::uint32_t        number_of_subblocks_present_for_brick{ 0 };

        std::uint32_t CalcNumberOfSubblocksMissing() const
        {
            return this->number_of_subblocks_required_for_brick - this->number_of_subblocks_present_for_brick;
        }
    };

    struct InitialInspectionResult
    {
        std::vector<int> subblocks_ordered_by_fileposition;

        std::map<BrickCoordinate, std::uint32_t> number_of_slices_per_brick;
    };
public:
    struct ReadingConstraints
    {
        std::uint32_t max_number_of_subblocks_inflight{ 0 };
    };

    struct OrderReadingResult
    {
        std::vector<int> reading_order;
        std::uint32_t max_number_of_subblocks_inflight{ 0 };
        std::map<BrickCoordinate, std::uint32_t> number_of_slices_per_brick;
    };

    /// The purpose of this function is to determine a order (in which to read the subblocks) which
    /// ensures that the number of "subblocks-in-flight" has a certain limit. With "subblocks-in-flight"
    /// we refer to "unfinished" bricks, i.e. bricks which are not complete because some of its parts
    /// are not read read from disk.
    /// The currently implemented algorithm does not try to give a perfect result, where perfect would mean:
    /// minimize the number of seeks we have to do while obeying the given limit, or: read as much as possible
    /// in a consecutive way. What we do is something like:
    /// * We sort the subblocks by the position in the file  
    /// * Then, we simulate how much "subblocks-in-flight" we have when using this order  
    /// * If we get beyond the limit specified, we re-order the following subblocks to-be-read so that a brick is finished  
    ///   (and we finish the brick which has least missing subblocks first).
    /// * continue like so until all subblocks are being read  
    /// This means that the specified limit is not exactly guaranteed, it just gives the threshold when we start
    /// counter-measures (i.e. change the order). However, note that the max amount of subblocks-in-flight is
    /// reported (and, as said, it may be larger than the max_number specified on input).
    ///
    /// \param [in,out] subblock_repository The subblock repository.
    /// \param          options             Options for controlling the operation.
    ///
    /// \returns    The result of determining the read order (including the max number of "subblocks-in-flight" when processing subblocks in this order).
    static OrderReadingResult DetermineOrder(libCZI::ICZIReader* subblock_repository, const ReadingConstraints& options);

private:
    static InitialInspectionResult CreateInitialInspectionResult(libCZI::ICZIReader* subblock_repository, const CziDocumentInfo& info);
    static std::vector<int> CreateOrder_OrderedByFilePosition(libCZI::ICZIReader* subblock_repository, const CziDocumentInfo& info);
    static CziDocumentInfo GetCziDocumentInfo(libCZI::ICZIReader* subblock_repository);
    static BrickCoordinate BrickCoordinateFromSubBlockInfo(const libCZI::SubBlockInfo& subblock_info);
    static void Reorder(libCZI::ICZIReader* subblock_repository, std::vector<int>& subblocks_list, size_t index, const std::pair<BrickCoordinate, UnfinishedBrickInfo>& brick);
};
