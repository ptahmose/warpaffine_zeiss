// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <LibWarpAffine_Config.h>
#include "appcontext.h"
#include "cmdlineoptions.h"
#include "main.h"
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
#define NOMINMAX
#include <Windows.h>
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
#include <clocale>
#endif

#include <chrono>  
#include <thread>
#include <tuple>
#include <memory>
#include <iomanip>
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
#include <ipp.h>
#endif

#include "configure.h"
#include "inc_libCZI.h"
#include "czi_helpers.h"
#include "deskew_helpers.h"
#include "dowarp.h"
#include "sliceswriter/ISlicesWriter.h"
#include "mmstream/mmstream.h"
#include "utilities.h"
#include "utilities_windows.h"
#include "mmstream/StreamEx.h"
#include "warpaffine/IWarpAffine.h"
#include "printstatistics.h"

using namespace std;
using namespace libCZI;

/// Determine the number of "3d-planes" that are to be done. This is currently simply the
/// number of T's times the number of channels.
///
/// \param  reader The reader.
///
/// \returns The number of 3dplanes to process.
static std::uint32_t GetNumberOf3dplanesToProcess(const shared_ptr<ICZIReader>& reader)
{
    const auto statistics = reader->GetStatistics();
    bool t_valid, z_valid, c_valid;
    int t_count, z_count, c_count;
    if (!statistics.dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count))
    {
        t_count = 1;
    }

    if (!statistics.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count))
    {
        c_count = 1;
    }

    return t_count * c_count;
}

static bool ReportSourceInfoAndCheckWhetherItCanBeProcessed(AppContext& app_context, const shared_ptr<ICZIReader>& reader)
{
    try
    {
        bool can_be_processed = true;

        ostringstream ss;
        auto statistics = reader->GetStatistics();
        bool t_valid, z_valid, c_valid;
        int t_count, z_count, c_count;
        t_valid = statistics.dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count);
        z_valid = statistics.dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &z_count);
        c_valid = statistics.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count);
        ss << "Source document information:" << endl;
        ss << "dimension: T=" << (t_valid ? to_string(t_count) : "*invalid*") << " Z=" << (z_valid ? to_string(z_count) : "*invalid*") << " C=" << (c_valid ? to_string(c_count) : "*invalid*");
        app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [&ss](auto log) {log->WriteLineStdOut(ss.str()); });

        // we require to have a z-stack, and we require to have C-dimension
        if (!z_valid)
        {
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [](auto log) {log->WriteLineStdOut("** this document cannot be processed because it has no z-dimension **"); });
            can_be_processed = false;
        }

        if (!c_valid)
        {
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [](auto log) {log->WriteLineStdOut("** this document cannot be processed because it has no c-dimension **"); });
            can_be_processed = false;
        }

        bool is_mosaic = false;
        ss = ostringstream();
        if (statistics.IsMIndexValid())
        {
            ss << "M-index: " << statistics.minMindex << " - " << statistics.maxMindex << endl;
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [&ss](auto log) {log->WriteLineStdOut(ss.str()); });
            if (statistics.maxMindex - statistics.minMindex > 1)
            {
                is_mosaic = true;
            }
        }

        bool tile_found = false;
        libCZI::IntSize size_of_tile;
        // now, let's determine the size of a tile 
        // ...how do we go about it - the simplest approach is:
        // - look for the first tile which is pyramid-layer 0
        // - then... report it's size
        reader->EnumSubset(nullptr, nullptr, true,
            [&](int index, const SubBlockInfo& info)->bool
            {
                size_of_tile = info.physicalSize;
                tile_found = true;
                return false;
            });

        if (tile_found)
        {
            ss = ostringstream();
            ss << "tile-size: " << size_of_tile.w << " x " << size_of_tile.h;
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [&ss](auto log) {log->WriteLineStdOut(ss.str()); });
        }
        else
        {
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [](auto log) {log->WriteLineStdOut("** this document cannot be processed because no layer-0 subblock was found **"); });
            can_be_processed = false;
        }

        // TODO(JBL): we would have to check for quite a few assumptions/preconditions, e.g.
        // - all subblocks with same M-index are at the same position
        // - all subblocks have same extent

        const bool document_is_marked_as_skewed = CziHelpers::CheckWhetherDocumentIsMarkedAsSkewedInMetadata(reader->ReadMetadataSegment()->CreateMetaFromMetadataSegment());
        if (!document_is_marked_as_skewed)
        {
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [](auto log) {log->WriteLineStdOut("** this document cannot be processed is is not marked as a \"skewed z-stack\" **"); });
            if (!app_context.GetCommandLineOptions().GetOverrideCheckForSkewedSourceDocument())
            {
                can_be_processed = false;
            }
        }

        return can_be_processed;
    }
    catch (exception& ex)
    {
        stringstream ss;
        ss << "exception occured: " << ex.what();
        app_context.GetLog()->WriteLineStdErr(ss.str());
        return false;
    }
}

static void ReportDetailsOfOperation(AppContext& app_context, const DeskewDocumentInfo& document_info, const DoWarp& do_warp)
{
    ostringstream ss;

    int max_bytes_per_pixel = 0;
    for (const auto& channel_info : document_info.map_channelindex_pixeltype)
    {
        ss << "channel#" << channel_info.first << " : " << Utils::PixelTypeToInformalString(channel_info.second) << endl;
        if (max_bytes_per_pixel < Utils::GetBytesPerPixel(channel_info.second))
        {
            max_bytes_per_pixel = Utils::GetBytesPerPixel(channel_info.second);
        }
    }

    ss << endl;

    for (const auto& item : document_info.map_brickid_position)
    {
        ss << "Brick: " << item.first.AsInformalString() << " - " << item.second.width << "x" << item.second.height << "x" << document_info.depth << endl;
        ss << "  -> " << Utilities::FormatMemorySize(static_cast<uint64_t>(item.second.width) * item.second.height * document_info.depth * max_bytes_per_pixel) << endl;
        //ss << item.first.AsInformalString() << " : " << item.second.width << "x" << item.second.height << "x" << document_info.depth << endl;
        const auto output_cuboid = do_warp.GetOutputVolume(item.first);
        ss << "  Output: " << output_cuboid.width << "x" << output_cuboid.height << "x" << output_cuboid.depth << endl;
        ss << "  -> " << Utilities::FormatMemorySize(static_cast<uint64_t>(output_cuboid.width) * output_cuboid.height * output_cuboid.depth * max_bytes_per_pixel) << endl;
    }

    app_context.GetLog()->WriteLineStdOut(ss.str());
}

static void ReportOperationSettings(AppContext& app_context, const DeskewDocumentInfo& document_info, const DoWarp& do_warp, const shared_ptr<ICZIReader>& reader, const std::shared_ptr<ICziSlicesWriter>& writer)
{
    ostringstream ss;
    auto statistics = reader->GetStatistics();

    bool t_valid, z_valid, c_valid;
    int t_count, z_count, c_count;
    t_valid = statistics.dimBounds.TryGetInterval(DimensionIndex::T, nullptr, &t_count);
    z_valid = statistics.dimBounds.TryGetInterval(DimensionIndex::Z, nullptr, &z_count);
    c_valid = statistics.dimBounds.TryGetInterval(DimensionIndex::C, nullptr, &c_count);
    ss << "Source document dimension: T=" << (t_valid ? to_string(t_count) : "*invalid*")
        << " Z=" << (z_valid ? to_string(z_count) : "*invalid*")
        << " C=" << (c_valid ? to_string(c_count) : "*invalid*")
        << " M=" << (statistics.IsMIndexValid() ? to_string(statistics.maxMindex) : "*invalid*");
    app_context.GetLog()->WriteLineStdOut(ss.str());

    int brick_count = (t_valid ? t_count : 1) * (c_valid ? c_count : 1) * (statistics.IsMIndexValid() ? statistics.maxMindex : 1);
    ss = ostringstream();
    ss << "number of source-bricks: " << brick_count;
    app_context.GetLog()->WriteLineStdOut(ss.str());

    Utilities::ExecuteIfVerbosityAboveOrEqual(
        app_context.GetCommandLineOptions().GetPrintOutVerbosity(),
        MessagesPrintVerbosity::kChatty,
        [&]()->void
        {
            ss = ostringstream();
            ss << "Transformation matrix:" << endl << do_warp.GetTransformationMatrix();
            app_context.GetLog()->WriteLineStdOut(ss.str());
        });

    ss = ostringstream();
    ss << "Reader: implementation: '" << Utilities::LibCziReaderImplementationToInformalString(app_context.GetCommandLineOptions().GetLibCziReaderImplementation())
        << "'; # of reader-threads: " << app_context.GetCommandLineOptions().GetNumberOfReaderThreads();
    app_context.GetLog()->WriteLineStdOut(ss.str());

    ss = ostringstream();
    ss << "Brickreader: " << Utilities::BrickReaderImplementationToInformalString(app_context.GetCommandLineOptions().GetBrickReaderImplementation());
    app_context.GetLog()->WriteLineStdOut(ss.str());

    ss = ostringstream();
    ss << "warp operation: " << Utilities::OperationTypeToInformalString(app_context.GetCommandLineOptions().GetTypeOfOperation())
        << "; interpolation=" << Utilities::InterpolationToInformativeString(app_context.GetCommandLineOptions().GetInterpolationMode());
    app_context.GetLog()->WriteLineStdOut(ss.str());

    ss = ostringstream();
    ss << "Input document: width=" << document_info.width << " x height=" << document_info.height << " x depth=" << document_info.depth;
    app_context.GetLog()->WriteLineStdOut(ss.str());

    ss = ostringstream();
    auto output_extent = do_warp.GetOutputExtent();
    ss << "Output document: width=" << get<0>(output_extent) << " x height=" << get<1>(output_extent) << " x depth=" << get<2>(output_extent);
    app_context.GetLog()->WriteLineStdOut(ss.str());

    app_context.GetLog()->WriteLineStdOut("");
}

static shared_ptr<IWarpAffine> CreateWarpAffineEngine(AppContext& context)
{
    return CreateWarpAffine(context.GetCommandLineOptions().GetWarpAffineEngineImplementation());
}

static tuple<shared_ptr<ICZIReader>, shared_ptr<IStreamEx>> CreateCziReader(AppContext& context)
{
    shared_ptr<IStreamEx> stream;
    try
    {
        switch (context.GetCommandLineOptions().GetLibCziReaderImplementation())
        {
        case LibCziReaderImplementation::kStock:
            stream = CreateStockStreamEx(
                context.GetCommandLineOptions().GetSourceCZIFilenameW().c_str(), 
                context.GetCommandLineOptions().GetSourceStreamClass(), 
                context.GetCommandLineOptions().GetPropertyBagForStreamClass());
            break;
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
        case LibCziReaderImplementation::kMmf:
            stream = CreateMemoryMappedStreamSp(context.GetCommandLineOptions().GetSourceCZIFilenameW().c_str());
            break;
#endif
        }
    }
    catch (exception& ex)
    {
        stringstream ss;
        ss << "Could not access the input file : " << ex.what();
        context.GetLog()->WriteLineStdErr(ss.str());
        return make_tuple(nullptr, nullptr);
    }

    auto spReader = libCZI::CreateCZIReader();

    try
    {
        spReader->Open(stream);
    }
    catch (exception& ex)
    {
        stringstream ss;
        ss << "Could not open the CZI : " << ex.what();
        context.GetLog()->WriteLineStdErr(ss.str());
        return make_tuple(nullptr, nullptr);
    }

    return make_tuple(spReader, stream);
}

static std::shared_ptr<ICziSlicesWriter> CreateCziWriter(AppContext& context)
{
    if (!context.GetCommandLineOptions().GetUseNullWriter())
    {
        return CreateSlicesWriterTbb(context, context.GetCommandLineOptions().GetDestinationCZIFilenameW());
    }

    return CreateNullSlicesWriter();
}

static shared_ptr<ICziBrickReader> CreateCziBrickSource(AppContext& context, std::shared_ptr<ICZIReader> reader, std::shared_ptr<IStreamEx> stream)
{
    shared_ptr<ICziBrickReader> brick_reader;
    try
    {
        switch (context.GetCommandLineOptions().GetBrickReaderImplementation())
        {
        case BrickReaderImplementation::kPlaneReader:
            brick_reader = CreateBrickReaderPlaneReader(context, reader, stream);
            break;
        case BrickReaderImplementation::kLinearReading:
            brick_reader = CreateBrickReaderLinearReading(context, reader, stream);
            break;
        case BrickReaderImplementation::kPlaneReader2:
            brick_reader = CreateBrickReaderPlaneReader2(context, reader, stream);
            break;
        }
    }
    catch (exception& ex)
    {
        stringstream ss;
        ss << "Could not construct 'brick-reader': " << ex.what();
        context.GetLog()->WriteLineStdErr(ss.str());
        return nullptr;
    }

    return brick_reader;
}

static void WaitUntilDoneWithStatisticsReporting(AppContext& app_context, DoWarp& do_warp)
{
    PrintStatistics print_statistics(app_context);
    bool is_first_iteration = true;
    for (;;)
    {
        if (app_context.GetLog()->IsStdOutATerminal())
        {
            WarpStatistics statistics = do_warp.GetStatistics();
            if (is_first_iteration)
            {
                print_statistics.PrintToStdout(statistics);
                is_first_iteration = false;
            }
            else
            {
                print_statistics.MoveCursorUpAndPrintToStdout(statistics);
            }
        }

        this_thread::sleep_for(chrono::milliseconds(500));

        if (do_warp.IsDone())
        {
            break;
        }
    }

    do_warp.WaitUntilDone();

    // ok, after we are done - we do one additional update of the statistics with the "final state"
    if (app_context.GetLog()->IsStdOutATerminal())
    {
        WarpStatistics statistics = do_warp.GetStatistics();
        print_statistics.MoveCursorUpAndPrintToStdout(statistics);
    }
}

static void WriteOutTotalProgress(AppContext& app_context, float total_progress)
{
    ostringstream ss;
    ss.imbue(app_context.GetFormattingLocale());
    ss << fixed << setprecision(1) << total_progress << "%";
    app_context.GetLog()->WriteLineStdOut(ss.str());
}

static void WaitUntilDoneMinimalVerbosity(AppContext& app_context, DoWarp& do_warp)
{
    // what we do here is:
    // - we loop-with-a-sleep until the DoWarp-object is reporting that it's operation is done
    // - we check the progress state at each iteration, and print a line like this "34.4%" every time the progress has 
    //    increased by at least 1% compared to the last report we printed

    bool is_first_iteration = true;
    float last_time_reported_total_progress;
    for (;;)
    {
        float total_progress = do_warp.GetStatistics().total_progress_percent;
        if (!isnan(total_progress))
        {
            if (is_first_iteration == true)
            {
                WriteOutTotalProgress(app_context, total_progress);
                last_time_reported_total_progress = total_progress;
                is_first_iteration = false;
            }
            else
            {
                // report only if the progress has increased by at least 1% (comapred to the last report we printed out before)
                if (total_progress - last_time_reported_total_progress >= 1)
                {
                    WriteOutTotalProgress(app_context, total_progress);
                    last_time_reported_total_progress = total_progress;
                }
            }
        }

        this_thread::sleep_for(chrono::milliseconds(500));

        if (do_warp.IsDone())
        {
            break;
        }
    }

    do_warp.WaitUntilDone();
}

static void WaitUntilDone(AppContext& app_context, DoWarp& do_warp)
{
    // if stdout is not the console or if the specified verbosity is not at least "normal", we use a "reduced progress reporting"
    if (app_context.GetLog()->IsStdOutATerminal() && app_context.GetCommandLineOptions().GetPrintOutVerbosity() >= MessagesPrintVerbosity::kNormal)
    {
        WaitUntilDoneWithStatisticsReporting(app_context, do_warp);
    }
    else
    {
        WaitUntilDoneMinimalVerbosity(app_context, do_warp);
    }
}

/// This function is used to modify the metadata of the output-document, depending on the operation-type.
///
/// \param [in,out] root_node      The root node of the CZI-document's XML-metadata.
/// \param          operation_type Type of the operation.
static void TweakMetadata(libCZI::IXmlNodeRw* root_node, OperationType operation_type)
{
    // here we tweak the metadata (specific to LLS-documents), depending on the operation-type
    auto zaxis_shear_node = root_node->GetChildNode("Metadata/Information/Image/Dimensions/Z/ZAxisShear");
    if (zaxis_shear_node)
    {
        switch (operation_type)
        {
        case OperationType::Deskew:
            zaxis_shear_node->SetValue("Shift60");
            break;
        case OperationType::CoverGlassTransformAndXYRotated:
        case OperationType::CoverGlassTransform:
            zaxis_shear_node->SetValue("None");
            break;
        case OperationType::Identity:
            break;
        }
    }
}

int libmain(int argc, char** _argv)
{
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    ippInit();
#endif
    AppContext app_context;

    try
    {
        bool operation_should_continue;
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
        CommandlineArgsWindowsHelper args_helper;
        operation_should_continue = app_context.Initialize(args_helper.GetArgc(), args_helper.GetArgv());
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
        setlocale(LC_CTYPE, "");
        operation_should_continue = app_context.Initialize(argc, _argv);
#endif
        if (!operation_should_continue)
        {
            return EXIT_SUCCESS;
        }

        auto reader_and_stream = CreateCziReader(app_context);
        
        if (!get<0>(reader_and_stream) || !get<1>(reader_and_stream))
        {
            return EXIT_FAILURE;
        }

        const bool can_process_document = ReportSourceInfoAndCheckWhetherItCanBeProcessed(app_context, get<0>(reader_and_stream));
        if (!can_process_document)
        {
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [](auto log) {log->WriteLineStdOut("-> Document was determined to be unsuitable and cannot be processed with this tool."); });
            return EXIT_FAILURE;
        }

        auto writer = CreateCziWriter(app_context);

        auto brick_source = CreateCziBrickSource(app_context, get<0>(reader_and_stream), get<1>(reader_and_stream));
        auto warp_affine_engine = CreateWarpAffineEngine(app_context);

        DeskewDocumentInfo document_info = CziHelpers::GetDocumentInfo(get<0>(reader_and_stream).get());

        auto transformation_matrix = DeskewHelpers::GetTransformationMatrixSoThatEdgePointIsAtOrigin(
            app_context.GetCommandLineOptions().GetTypeOfOperation(),
            document_info);

        DoWarp doWarp(
            app_context,
            GetNumberOf3dplanesToProcess(get<0>(reader_and_stream)),
            document_info,
            transformation_matrix,
            brick_source,
            writer,
            warp_affine_engine);

        Configure configurator(app_context);
        const bool configuration_successful = configurator.DoConfiguration(document_info, doWarp);
        if (!configuration_successful)
        {
            app_context.DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity::kNormal, [](auto log) {log->WriteLineStdOut("-> Configuration was not successful, exiting."); });
            return EXIT_FAILURE;
        }

        app_context.DoIfVerbosityGreaterOrEqual(
            MessagesPrintVerbosity::kNormal,
            [&](auto log)
            {ReportOperationSettings(app_context, document_info, doWarp, get<0>(reader_and_stream), writer); });

        app_context.DoIfVerbosityGreaterOrEqual(
            MessagesPrintVerbosity::kNormal,
            [&](auto log)
            {ReportDetailsOfOperation(app_context, document_info, doWarp); });

        doWarp.DoOperation();
        WaitUntilDone(app_context, doWarp);

        get<0>(reader_and_stream)->EnumerateAttachments(
            [&writer, &reader_and_stream](int index, const libCZI::AttachmentInfo& info) -> bool
            {
                writer->AddAttachment(get<0>(reader_and_stream)->ReadAttachment(index));
                return true;
            });

        switch (const auto type_of_operation = app_context.GetCommandLineOptions().GetTypeOfOperation())
        {
        case OperationType::Deskew:
        {
            ScalingInfo scaling_info;
            scaling_info.scaleX = scaling_info.scaleY = document_info.xy_scaling;
            scaling_info.scaleZ = 0.5 * document_info.z_scaling;
            writer->Close(
                get<0>(reader_and_stream)->ReadMetadataSegment()->CreateMetaFromMetadataSegment(),
                &scaling_info,
                [type_of_operation](libCZI::IXmlNodeRw* root_node)->void {TweakMetadata(root_node, type_of_operation); });
            break;
        }
        case OperationType::CoverGlassTransform:
        case OperationType::CoverGlassTransformAndXYRotated:
            // for those operations the transformation is constructed so that the scaling is isotrophic, so
            // we know that the scaling in x,y,z is the same (and as the x-y-scaling was in the source)
        {
            ScalingInfo scaling_info;
            scaling_info.scaleX = scaling_info.scaleY = scaling_info.scaleZ = document_info.xy_scaling;
            writer->Close(
                get<0>(reader_and_stream)->ReadMetadataSegment()->CreateMetaFromMetadataSegment(),
                &scaling_info,
                [type_of_operation](libCZI::IXmlNodeRw* root_node)->void {TweakMetadata(root_node, type_of_operation); });
            break;
        }
        default:
            // otherwise, we do not change the scaling
            writer->Close(
                get<0>(reader_and_stream)->ReadMetadataSegment()->CreateMetaFromMetadataSegment(),
                nullptr,
                [type_of_operation](libCZI::IXmlNodeRw* root_node)->void {TweakMetadata(root_node, type_of_operation); });
            break;
        }

        array<uint8_t, 16> hash_code;
        if (doWarp.TryGetHash(&hash_code))
        {
            // here the strategy is - if the hash was requested on the command-line, then it will be printed here, independently of the verbosity-setting
            ostringstream ss;
            ss << endl << "hash of result: " << Utilities::HashToString(hash_code);
            app_context.GetLog()->WriteLineStdOut(ss.str());
        }

        return EXIT_SUCCESS;
    }
    catch (exception& exception)
    {
        stringstream ss;
        ss << "An unhandled exception occurred : " << exception.what();
        app_context.GetLog()->WriteLineStdErr(ss.str());
        return EXIT_FAILURE;
    }
}
