// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <sstream>
#include <cstdint>
#include <string>
#include <vector>
#include "appcontext.h"
#include "dowarp.h"

/// This class is used to format and print various statistics during the operation, and
/// print those to the console.
class PrintStatistics
{
private:
    static constexpr int kLengthOfValueColumn = 25; ///< The length of the column containing the values in characters.

    AppContext& context_;
public:
    PrintStatistics() = delete;

    /// Constructor.
    /// \param [in] context The context.
    explicit PrintStatistics(AppContext& context);

    /// Print the formatted statistics to stdout.
    /// \param  statistics  The statistics.
    void PrintToStdout(const WarpStatistics& statistics);

    /// Move the cursor up as many lines as the statistics-text has lines, and then print the formatted statistics to stdout.
    /// \param  statistics  The statistics.
    void MoveCursorUpAndPrintToStdout(const WarpStatistics& statistics);
private:
    static void AddSpaces(std::ostringstream& stream, size_t number_of_spaces_to_add);
    static void AddToStreamFormatToFillColumn(std::ostringstream& stream, const std::string& text, size_t length_of_field);
    void AddColumnHeaderLine(std::ostringstream& stream);
    [[nodiscard]] int GetNumberOfLinesOfStatisticsText() const
    {
        return static_cast<int>(this->info_items_.size()) + 2;
    }

    struct InfoItemInfo
    {
        std::string name;
        std::function<std::string(const WarpStatistics&)> format_item;
    };

    std::vector<InfoItemInfo> info_items_;
    int max_length_of_name;
    int length_of_value_column_;

    std::string FormatElapsedItem(const WarpStatistics& warp_statistics);
    std::string FormatTaskArenaQueueLength(const WarpStatistics& warp_statistics);
    std::string FormatBytesReadFromSourceFile(const WarpStatistics& warp_statistics);
    std::string FormatDataRateReadingFromSourceFile(const WarpStatistics& warp_statistics);
    std::string FormatTotalInputBrickDataSize(const WarpStatistics& warp_statistics);
    std::string FormatInputBrickCount(const WarpStatistics& warp_statistics);
    std::string FormatInputBrickDataRate(const WarpStatistics& warp_statistics);
    std::string FormatInputBrickRate(const WarpStatistics& warp_statistics);
    std::string FormatInputSlicesRate(const WarpStatistics& warp_statistics);
    std::string FormatWarpAffineTasksInFlight(const WarpStatistics& warp_statistics);
    std::string FormatCompressionTasksInFlight(const WarpStatistics& warp_statistics);
    std::string FormatWriteSlicesQueueLength(const WarpStatistics& warp_statistics);
    std::string FormatBrickReaderThrottled(const WarpStatistics& warp_statistics);
    std::string FormatCurrentlyActiveTasks(const WarpStatistics& warp_statistics);
    std::string FormatCurrentlySuspendedTasks(const WarpStatistics& warp_statistics);
    std::string FormatNumberOfCompressedSubblocksInFlight(const WarpStatistics& warp_statistics);
    std::string FormatNumberOfUncompressedPlanesInFlight(const WarpStatistics& warp_statistics);
    std::string FormatAllocatedMemorySourceBricks(const WarpStatistics& warp_statistics);
    std::string FormatAllocatedMemoryDestinationBricks(const WarpStatistics& warp_statistics);
    std::string FormatAllocatedMemoryCompressedDestinationSlice(const WarpStatistics& warp_statistics);
    std::string FormatNumberOfSlicesAddedToWriter(const WarpStatistics& warp_statistics);
    std::string FormatOverallProgress(const WarpStatistics& warp_statistics);

    const std::locale& GetFormattingLocale() const { return this->context_.GetFormattingLocale(); }
};
