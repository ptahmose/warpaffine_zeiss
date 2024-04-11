// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "configure.h"
#include <LibWarpAffine_Config.h>
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
#include <Windows.h>
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
#include <unistd.h>
#endif
#include "inc_libCZI.h"
#include <algorithm>

using namespace std;

Configure::Configure(AppContext& app_context) : app_context_(app_context)
{
    if (!app_context.GetCommandLineOptions().GetIsMainMemorySizeOverrideValid())
    {
        this->physical_memory_size_ = DetermineMainMemorySize();
    }
    else
    {
        this->physical_memory_size_ = app_context.GetCommandLineOptions().GetMainMemorySizeOverride();
    }

    /*
    ostringstream string_stream;
    string_stream << "Physical RAM: " << this->physical_memory_size_ << endl;
    this->app_context_.WriteDebugString(string_stream.str().c_str());
    */
}

bool Configure::DoConfiguration(const DeskewDocumentInfo& deskew_document_info, const DoWarp& do_warp)
{
    // determine the size of a source brick

    // 1st step - we determine the max. sizes of the source bricks, the destination brick (and
    //  also tiled destination brick)
    const auto memory_characteristics = CalculateMemoryCharacteristics(deskew_document_info, do_warp);

    // In current implementation, we need to read the source-brick completely to memory. So, let us check
    //  whether this is fitting into memory. 
    // The heuristic we apply her is:
    // * The very minimum is something like: we need to have on input-brick in memory and (for the very least) one tiled-output-brick
    // * If this does not fit into main-memory - we better give up immediately. This will in all likelihood not work, might bog down the machine and
    //    be for sure incredibly slow.
    const auto minimal_amount_of_memory_required = memory_characteristics.max_size_of_input_brick + memory_characteristics.max_size_of_output_brick_including_tiling;
    if (minimal_amount_of_memory_required > this->physical_memory_size_)
    {
        ostringstream string_stream;
        string_stream.imbue(this->app_context_.GetFormattingLocale());
        string_stream << "Unable to process this document: This machine is detected to have a main" << endl
            << "memory of " << Utilities::FormatMemorySize(this->physical_memory_size_) << ", and the minimal amount of memory required to run the" << endl
            << "operation has been determined as " << Utilities::FormatMemorySize(minimal_amount_of_memory_required) << "." << endl
            << "This program will exit now. (Check the synopsis for how this check can be disabled)" << endl;
        this->app_context_.GetLog()->WriteLineStdOut(string_stream.str());
        return false;
    }

    // and now... more heuristic and black art
    // * We limit the memory available for "destination brick" to about 1/3 of the main-memory
    // * We set the high-water mark to 60% of the main memory. The high-water mark determines around when the
    //    reader throttles, or - when that amount of memory is allocated (in total), then the reader throttles.
    
    uint64_t high_water_mark_limit = this->physical_memory_size_ * 6 / 10;

    // high_water_mark_limit must be larger than "memory_characteristics.max_size_of_input_brick" - if this is not the case
    //  (with the heuristic of "60%"), then we set it to this
    high_water_mark_limit = max(memory_characteristics.max_size_of_input_brick, high_water_mark_limit);

    uint64_t limit_for_memory_type_destination_brick = this->physical_memory_size_ / 3;

    // well, this limit must not be larger than "what's remaining if we subtract the high_water_mark_limit"
    if (limit_for_memory_type_destination_brick > this->physical_memory_size_- high_water_mark_limit)
    {
        limit_for_memory_type_destination_brick = this->physical_memory_size_ - high_water_mark_limit;
    }

    this->app_context_.GetAllocator().SetMaximumMemoryLimitForMemoryType(BrickAllocator::MemoryType::DestinationBrick, limit_for_memory_type_destination_brick);
    this->app_context_.GetAllocator().SetHighWatermark(high_water_mark_limit);

    return true;
}

/*static*/std::uint64_t Configure::DetermineMainMemorySize()
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    MEMORYSTATUSEX memory_status = {};
    memory_status.dwLength = sizeof(memory_status);
    GlobalMemoryStatusEx(&memory_status);
    return memory_status.ullTotalPhys;
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    const uint64_t page_size = sysconf(_SC_PAGESIZE);

    // According to https://www.gnu.org/software/libc/manual/html_node/Query-Memory-Parameters.html,
    //  _SC_PHYS_PAGES gives us the number of pages of physical memory, with the limitation that there
    //  is no guarantee that the memory is actually available. There is also _SC_AVPHYS_PAGES, which
    //  gives the number of pages we can use "without hindering any other processes" (which is not
    //  available on macOS it seems, btw). However, for our purposes the former parameter seems
    //  best suited.
    const uint64_t page_number = sysconf(_SC_PHYS_PAGES);

    return page_size * page_number;
#endif
}

/*static*/Configure::MemoryCharacteristicsOfOperation Configure::CalculateMemoryCharacteristics(const DeskewDocumentInfo& deskew_document_info, const DoWarp& do_warp)
{
    MemoryCharacteristicsOfOperation memory_characteristics;

    // we run through all "brick-ids" and search for the largest (in x- and y)
    const auto& max_stack = max_element(
        deskew_document_info.map_brickid_position.cbegin(),
        deskew_document_info.map_brickid_position.cend(),
        [](const auto& x, const auto& y)
        {
            return x.second.width * x.second.height < y.second.width* y.second.height;
        });

    // and now, we determine the "largest pixeltype"
    const auto& max_pixelsize = max_element(
        deskew_document_info.map_channelindex_pixeltype.cbegin(),
        deskew_document_info.map_channelindex_pixeltype.cend(),
        [](const auto& x, const auto& y)
        {
            return libCZI::Utils::GetBytesPerPixel(x.second) < libCZI::Utils::GetBytesPerPixel(y.second);
        });

    const auto max_bytes_per_pixel = libCZI::Utils::GetBytesPerPixel(max_pixelsize->second);

    memory_characteristics.max_size_of_input_brick =
        static_cast<uint64_t>(max_stack->second.width) *
        max_stack->second.height *
        deskew_document_info.depth *
        max_bytes_per_pixel;

    const auto output_extent = do_warp.GetOutputVolume(max_stack->first);
    memory_characteristics.max_size_of_output_brick =
        static_cast<uint64_t>(output_extent.width) *
        output_extent.height *
        output_extent.depth *
        max_bytes_per_pixel;

    const auto output_extent_including_tiling = do_warp.GetLargestOutputExtentIncludingTiling(max_stack->first);
    memory_characteristics.max_size_of_output_brick_including_tiling =
        static_cast<uint64_t>(output_extent_including_tiling.width) *
        output_extent_including_tiling.height *
        output_extent_including_tiling.depth *
        max_bytes_per_pixel;

    return memory_characteristics;
}
