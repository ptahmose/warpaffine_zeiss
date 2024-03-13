// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "printstatistics.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <limits>
#include <string>
#include <cmath>
#include <functional>
#include "utilities.h"

using namespace std;

PrintStatistics::PrintStatistics(AppContext& context)
    : context_(context)
{
    this->info_items_.push_back({ "elapsed time", bind(&PrintStatistics::FormatElapsedItem, this, placeholders::_1) });
    this->info_items_.push_back({ "overall progress", bind(&PrintStatistics::FormatOverallProgress, this, placeholders::_1) });
    this->info_items_.push_back({ "task-arena queue length", bind(&PrintStatistics::FormatTaskArenaQueueLength, this, placeholders::_1) });
    this->info_items_.push_back({ "read from source file", bind(&PrintStatistics::FormatBytesReadFromSourceFile, this, placeholders::_1) });
    this->info_items_.push_back({ "# of subblocks added to writer", bind(&PrintStatistics::FormatNumberOfSlicesAddedToWriter, this, placeholders::_1) });
    this->info_items_.push_back({ "datarate reading from source file", bind(&PrintStatistics::FormatDataRateReadingFromSourceFile, this, placeholders::_1) });
    this->info_items_.push_back({ "input total brick-data size", bind(&PrintStatistics::FormatTotalInputBrickDataSize, this, placeholders::_1) });
    this->info_items_.push_back({ "input brick count", bind(&PrintStatistics::FormatInputBrickCount, this, placeholders::_1) });
    this->info_items_.push_back({ "input brick datarate", bind(&PrintStatistics::FormatInputBrickDataRate, this, placeholders::_1) });
    this->info_items_.push_back({ "input brick rate", bind(&PrintStatistics::FormatInputBrickRate, this, placeholders::_1) });
    this->info_items_.push_back({ "input slices rate", bind(&PrintStatistics::FormatInputSlicesRate, this, placeholders::_1) });
    this->info_items_.push_back({ "warp-affine tasks in flight", bind(&PrintStatistics::FormatWarpAffineTasksInFlight, this, placeholders::_1) });
    this->info_items_.push_back({ "compression tasks in flight", bind(&PrintStatistics::FormatCompressionTasksInFlight, this, placeholders::_1) });
    this->info_items_.push_back({ "write-slices queue-length", bind(&PrintStatistics::FormatWriteSlicesQueueLength, this, placeholders::_1) });
    this->info_items_.push_back({ "brickreader throttled", bind(&PrintStatistics::FormatBrickReaderThrottled, this, placeholders::_1) });
    this->info_items_.push_back({ "# of active tasks", bind(&PrintStatistics::FormatCurrentlyActiveTasks, this, placeholders::_1) });
    this->info_items_.push_back({ "# of suspended tasks", bind(&PrintStatistics::FormatCurrentlySuspendedTasks, this, placeholders::_1) });
    this->info_items_.push_back({ "(compressed) subblocks in flight", bind(&PrintStatistics::FormatNumberOfCompressedSubblocksInFlight, this, placeholders::_1) });
    this->info_items_.push_back({ "(uncompressed) planes in flight", bind(&PrintStatistics::FormatNumberOfUncompressedPlanesInFlight, this, placeholders::_1) });
    this->info_items_.push_back({ "Memory: source bricks", bind(&PrintStatistics::FormatAllocatedMemorySourceBricks, this, placeholders::_1) });
    this->info_items_.push_back({ "Memory: destination bricks", bind(&PrintStatistics::FormatAllocatedMemoryDestinationBricks, this, placeholders::_1) });
    this->info_items_.push_back({ "Memory: compressed dest. slices", bind(&PrintStatistics::FormatAllocatedMemoryCompressedDestinationSlice, this, placeholders::_1) });

    this->max_length_of_name = (numeric_limits<int>::min)();
    for (const auto& item : this->info_items_)
    {
        const int length_of_name = static_cast<int>(item.name.length());
        if (this->max_length_of_name < length_of_name)
        {
            this->max_length_of_name = length_of_name;
        }
    }

    this->length_of_value_column_ = PrintStatistics::kLengthOfValueColumn;
}

std::string PrintStatistics::FormatElapsedItem(const WarpStatistics& warp_statistics)
{
    return Utilities::format_time_in_seconds(warp_statistics.elapsed_time_since_start_in_seconds);
}

std::string PrintStatistics::FormatTaskArenaQueueLength(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.task_arena_queue_length;
    return ss.str();
}

std::string PrintStatistics::FormatBytesReadFromSourceFile(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << std::fixed << setprecision(1) << warp_statistics.bytes_read_from_source_file / 1e6 << " MB";
    return ss.str();
}

std::string PrintStatistics::FormatDataRateReadingFromSourceFile(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << fixed << setprecision(1) << warp_statistics.datarate_read_from_source_file / 1e6 << " MB/s";
    return ss.str();
}

std::string PrintStatistics::FormatTotalInputBrickDataSize(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.source_brick_data_delivered << " byte";
    return ss.str();
}

std::string PrintStatistics::FormatInputBrickCount(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.source_bricks_delivered;
    return ss.str();
}

std::string PrintStatistics::FormatInputBrickDataRate(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << fixed << setprecision(1) << warp_statistics.source_brick_data_delivered_per_second / 1e6 << " MB/s";
    return ss.str();
}

std::string PrintStatistics::FormatInputBrickRate(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << fixed << setprecision(1) << warp_statistics.source_bricks_delivered_per_minute << " 1/min";
    return ss.str();
}

std::string PrintStatistics::FormatInputSlicesRate(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << fixed << setprecision(1) << warp_statistics.source_slices_read_per_second << " 1/s";
    return ss.str();
}

std::string PrintStatistics::FormatWarpAffineTasksInFlight(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.warp_tasks_in_flight;
    return ss.str();
}

std::string PrintStatistics::FormatCompressionTasksInFlight(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.compression_tasks_in_flight;
    return ss.str();
}

std::string PrintStatistics::FormatWriteSlicesQueueLength(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.write_slices_queue_length;
    return ss.str();
}

std::string PrintStatistics::FormatBrickReaderThrottled(const WarpStatistics& warp_statistics)
{
    return warp_statistics.reader_throttled ? "yes" : "no";
}

std::string PrintStatistics::FormatCurrentlyActiveTasks(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.currently_active_tasks;
    return ss.str();
}

std::string PrintStatistics::FormatCurrentlySuspendedTasks(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.currently_suspended_tasks;
    return ss.str();
}

std::string PrintStatistics::FormatNumberOfCompressedSubblocksInFlight(const WarpStatistics& warp_statistics)
{
    if (warp_statistics.brickreader_compressed_subblocks_in_flight != numeric_limits<uint64_t>::max())
    {
        std::ostringstream ss;
        ss.imbue(this->GetFormattingLocale());
        ss << warp_statistics.brickreader_compressed_subblocks_in_flight;
        return ss.str();
    }

    return "N/A";
}

std::string PrintStatistics::FormatNumberOfUncompressedPlanesInFlight(const WarpStatistics& warp_statistics)
{
    if (warp_statistics.brickreader_uncompressed_planes_in_flight != numeric_limits<uint64_t>::max())
    {
        std::ostringstream ss;
        ss.imbue(this->GetFormattingLocale());
        ss << warp_statistics.brickreader_uncompressed_planes_in_flight;
        return ss.str();
    }

    return "N/A";
}

std::string PrintStatistics::FormatAllocatedMemorySourceBricks(const WarpStatistics& warp_statistics)
{
    return Utilities::FormatMemorySize(warp_statistics.memory_status[static_cast<size_t>(BrickAllocator::MemoryType::SourceBrick)], " ");
}

std::string PrintStatistics::FormatAllocatedMemoryDestinationBricks(const WarpStatistics& warp_statistics)
{
    return Utilities::FormatMemorySize(warp_statistics.memory_status[static_cast<size_t>(BrickAllocator::MemoryType::DestinationBrick)], " ");
}

std::string PrintStatistics::FormatAllocatedMemoryCompressedDestinationSlice(const WarpStatistics& warp_statistics)
{
    return Utilities::FormatMemorySize(warp_statistics.memory_status[static_cast<size_t>(BrickAllocator::MemoryType::CompressedDestinationSlice)], " ");
}

std::string PrintStatistics::FormatNumberOfSlicesAddedToWriter(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << warp_statistics.subblocks_added_to_writer;
    return ss.str();
}

std::string PrintStatistics::FormatOverallProgress(const WarpStatistics& warp_statistics)
{
    std::ostringstream ss;
    ss.imbue(this->GetFormattingLocale());
    ss << fixed << setprecision(1);
    if (!isnan(warp_statistics.total_progress_percent))
    {
        ss << warp_statistics.total_progress_percent << " %";
    }
    else
    {
        ss << "N/A";
    }

    return ss.str();
}

void PrintStatistics::AddSpaces(std::ostringstream& stream, size_t number_of_spaces_to_add)
{
    for (size_t i = 0; i < number_of_spaces_to_add; ++i)
    {
        stream << ' ';
    }
}

void PrintStatistics::AddColumnHeaderLine(std::ostringstream& stream)
{
    stream << "+";
    for (int i = 0; i < this->max_length_of_name; ++i)
    {
        stream << "-";
    }

    stream << "+";

    for (int i = 0; i < this->length_of_value_column_; ++i)
    {
        stream << "-";
    }

    stream << "+" << endl;
}

void PrintStatistics::MoveCursorUpAndPrintToStdout(const WarpStatistics& statistics)
{
    this->context_.GetLog()->MoveUp(this->GetNumberOfLinesOfStatisticsText());
    this->PrintToStdout(statistics);
}

void PrintStatistics::PrintToStdout(const WarpStatistics& statistics)
{
    std::ostringstream ss;
    this->AddColumnHeaderLine(ss);

    for (const auto& item : this->info_items_)
    {
        size_t spaces_to_insert = this->max_length_of_name - item.name.length();
        ss << "|";
        AddToStreamFormatToFillColumn(ss, item.name, this->max_length_of_name);

        ss << "| ";
        string value = item.format_item(statistics);
        AddToStreamFormatToFillColumn(ss, value, this->length_of_value_column_ - 1);
        ss << "|" << endl;
    }

    this->AddColumnHeaderLine(ss);

    this->context_.GetLog()->WriteStdOut(ss.str());
}

/*static*/void PrintStatistics::AddToStreamFormatToFillColumn(std::ostringstream& stream, const std::string text, size_t length_of_field)
{
    stream << left << setw(static_cast<streamsize>(length_of_field)) << text;
}
