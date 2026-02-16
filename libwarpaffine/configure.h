// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "appcontext.h"
#include "document_info.h"
#include "dowarp.h"

/// The purpose of this class is to configure the environment - taking into account
/// the document to process and the system parameters (in particular, how many memory
/// is available).
class Configure
{
private:
    AppContext& app_context_;
    std::uint64_t physical_memory_size_;
    bool allow_memory_oversubscription_{ false };
public:
    explicit Configure(AppContext& app_context);

    bool DoConfiguration(const DeskewDocumentInfo& deskew_document_info, const DoWarp& do_warp);
private:
    static std::uint64_t DetermineMainMemorySize();

    struct MemoryCharacteristicsOfOperation
    {
        /// The (maximum) size of the input brick in bytes.
        std::uint64_t max_size_of_input_brick{0};

        /// The (maximum) size of the output-brick (note: tiling is applied to this brick) in bytes.
        std::uint64_t max_size_of_output_brick{ 0 };

        /// The (maximum) size of the tiled output-brick in bytes.
        std::uint64_t max_size_of_output_brick_including_tiling{ 0 };
    };

    static MemoryCharacteristicsOfOperation CalculateMemoryCharacteristics(const DeskewDocumentInfo& deskew_document_info, const DoWarp& do_warp);
};
