// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "cmdlineoptions.h"
#include "utilities.h"
#include "brickreader/IBrickReader.h"
#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"

#include <rapidjson/document.h>

#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
#include <ippi.h>
#endif

#include <vector>
#include <map>
#include <memory>
#include <limits>
#include <utility>

using namespace std;

/// A custom formatter for CLI11 - used to have nicely formatted descriptions.
class CustomFormatter : public CLI::Formatter
{
public:
    CustomFormatter() : Formatter()
    {
        this->column_width(20);
    }

    std::string make_usage(const CLI::App* app, std::string name) const override
    {
        // 'name' is the full path of the executable, we only take the path "after the last slash or backslash"
        size_t offset = name.rfind('/');
        if (offset == string::npos)
        {
            offset = name.rfind('\\');
        }

        if (offset != string::npos && offset < name.length())
        {
            name = name.substr(1 + offset);
        }

        const auto result_from_stock_implementation = this->CLI::Formatter::make_usage(app, name);
        ostringstream ss(result_from_stock_implementation);
        ss << result_from_stock_implementation << endl << "  version: " << LIBWARPAFFINE_VERSION_MAJOR << "." << LIBWARPAFFINE_VERSION_MINOR << "." << LIBWARPAFFINE_VERSION_PATCH << endl;
        return ss.str();
    }

    std::string make_footer(const CLI::App* app) const override
    {
        ostringstream string_stream;
        string_stream << Formatter::make_footer(app) << endl << endl;
        int major_version, minor_version, tweak_version;
        libCZI::GetLibCZIVersion(&major_version, &minor_version, &tweak_version);
        libCZI::BuildInformation build_information;
        libCZI::GetLibCZIBuildInformation(build_information);
        string_stream << "libCZI version: " << major_version << "." << minor_version << "." << tweak_version;
        if (!build_information.compilerIdentification.empty())
        {
            string_stream << " (built with " << build_information.compilerIdentification << ")";
        }
        string_stream << endl;
        const int stream_classes_count = libCZI::StreamsFactory::GetStreamClassesCount();
        string_stream << " stream-classes: ";
        for (int i = 0; i < stream_classes_count; ++i)
        {
            libCZI::StreamsFactory::StreamClassInfo stream_info;
            libCZI::StreamsFactory::GetStreamInfoForClass(i, stream_info);
            if (i > 0)
            {
                string_stream << ", ";
            }

            string_stream << stream_info.class_name;
        }

        string_stream << endl;
        string_stream << "TBB version: " << LIBWARPAFFINE_TBB_VERSION << endl;
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
        const IppLibraryVersion* ipp_version = ippiGetLibVersion();
        string_stream << "IPP version: " << ipp_version->Version << " -  " << ipp_version->Name << endl;
#endif
        return string_stream.str();
    }

    std::string make_option_desc(const CLI::Option* o) const override
    {
        // we wrap the text so that it fits in the "second column"
        const auto lines = wrap(o->get_description().c_str(), 80 - this->get_column_width());

        string options_description;
        options_description.reserve(accumulate(lines.cbegin(), lines.cend(), static_cast<size_t>(0), [](size_t sum, const std::string& str) { return 1 + sum + str.size(); }));
        for (const auto& l : lines)
        {
            options_description.append(l).append("\n");
        }

        return options_description;
    }
private:
    static std::vector<std::string> wrap(const char* text, size_t line_length)
    {
        istringstream iss(text);
        std::string word, line;
        std::vector<std::string> vec;

        for (; ;)
        {
            iss >> word;
            if (!iss)
            {
                break;
            }

            // '\n' before a word means "insert a linebreak", and '\N' means "insert a linebreak and one more empty line"
            if (word.size() > 2 && word[0] == '\\' && (word[1] == 'n' || word[1] == 'N'))
            {
                line.erase(line.size() - 1);	// remove trailing space
                vec.push_back(line);
                if (word[1] == 'N')
                {
                    vec.emplace_back("");
                }

                line.clear();
                word.erase(0, 2);
            }

            if (line.length() + word.length() > line_length)
            {
                line.erase(line.size() - 1);
                vec.push_back(line);
                line.clear();
            }

            line += word + ' ';
        }

        if (!line.empty())
        {
            line.erase(line.size() - 1);
            vec.push_back(line);
        }

        return vec;
    }
};

/// CLI11-validator for the option "--override-memory-size".
struct MemorySizeValidator : public CLI::Validator
{
    MemorySizeValidator()
    {
        this->name_ = "MemorySizeValidator";
        this->func_ = [](const std::string& str) -> string
            {
                const bool parsed_ok = Utilities::TryParseMemorySize(str, nullptr);
                if (!parsed_ok)
                {
                    ostringstream string_stream;
                    string_stream << "Invalid memory-size \"" << str << "\"";
                    throw CLI::ValidationError(string_stream.str());
                }

                return {};
            };
    }
};

/*static*/const char* CCmdLineOptions::kDefaultCompressionOptions = "zstd1:ExplicitLevel=1;PreProcess=HiLoByteUnpack";

CCmdLineOptions::ParseResult CCmdLineOptions::Parse(int argc, char** argv)
{
    CLI::App app{ "Deskew-processing" };

    // specify the string-to-enum-mapping for "operation-type"
    std::map<std::string, OperationType> map_string_to_operationtype
    {
        { "identity", OperationType::Identity},
        { "deskew", OperationType::Deskew },
        { "coverglasstransform", OperationType::CoverGlassTransform },
        { "coverglasstransform_and_xy_rotated", OperationType::CoverGlassTransformAndXYRotated },
    };

    // specify the string-to-enum-mapping for "interpolation-mode"
    std::map<std::string, Interpolation> map_string_to_interpolationmode
    {
        { "NN", Interpolation::kNearestNeighbor},
        { "NearestNeighbor", Interpolation::kNearestNeighbor },
        { "linear", Interpolation::kBilinear },
        { "cubic", Interpolation::kBicubic },
        { "bspline", Interpolation::kBSpline },
        { "catmullrom", Interpolation::kCatMullRom },
        { "b05c03", Interpolation::kB05c03 },
    };

    // specify the string-to-enum-mapping for "libCZI-reader-implementation"
    std::map<std::string, LibCziReaderImplementation> map_string_to_libczi_reader_implementation
    {
        { "stock", LibCziReaderImplementation::kStock},
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
        { "mmf", LibCziReaderImplementation::kMmf },
#endif
    };

    // specify the string-to-enum-mapping for "brick-source-implementation"
    std::map<std::string, BrickReaderImplementation> map_string_to_brick_source_implementation
    {
        { "planereader", BrickReaderImplementation::kPlaneReader},
        { "planereader2", BrickReaderImplementation::kPlaneReader2 },
        { "linearreading", BrickReaderImplementation::kLinearReading },
    };

    // specify the string-to-enum-mapping for "warp-affine-transformation-implementation"
    std::map<std::string, WarpAffineImplementation> map_string_to_warp_affine_transformation_implementation
    {
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
        { "IPP", WarpAffineImplementation::kIPP},
#endif
        { "null", WarpAffineImplementation::kNull },
        { "reference", WarpAffineImplementation::kReference },
    };

    // specify the string-to-enum-mapping for "test-stop-pipeline-after-operation"
    std::map<std::string, TestStopPipelineAfter> map_string_to_stop_pipeline_after_operation
    {
        { "none", TestStopPipelineAfter::kNone},
        { "read", TestStopPipelineAfter::kReadFromSource },
        { "decompress", TestStopPipelineAfter::kDecompress },
    };

    // specify the string-to-enum-mapping for "task-arena-implementation"
    std::map<std::string, TaskArenaImplementation> map_string_to_task_arena_implementation
    {
        { "tbb", TaskArenaImplementation::kTBB},
    };

    // specify the string-to-enum-mapping for "verbosity"
    std::map<std::string, MessagesPrintVerbosity> map_string_to_print_out_verbosity
    {
        { "minimal", MessagesPrintVerbosity::kMinimal},
        { "normal", MessagesPrintVerbosity::kNormal },
        { "chatty", MessagesPrintVerbosity::kChatty },
        { "maximal", MessagesPrintVerbosity::kMaximal },
    };

    static const MemorySizeValidator memory_size_validator;

    std::string source_filename;
    std::string destination_filename;
    OperationType operation_type;
    LibCziReaderImplementation libczi_reader;
    BrickReaderImplementation brick_reader_source;
    WarpAffineImplementation warp_affine_engine_implementation;
    TestStopPipelineAfter test_stop_pipeline_after;
    TaskArenaImplementation task_arena_implementation;
    Interpolation interpolation = Interpolation::kNearestNeighbor;
    int number_of_reader_threads = 1;
    string compression_options_text;
    string brickreader_parameters;
    string argument_source_stream_class;
    string argument_source_stream_creation_propbag;
    MessagesPrintVerbosity print_out_verbosity;
    bool hash_result = false;
    int max_tile_extent;
    string override_ram_size_parameter;
    bool override_check_for_skewed_source = false;
    bool use_acquisition_tiles = false;
    app.add_option("-s,--source", source_filename, "The source CZI-file to be processed.")
        ->option_text("SOURCE_FILE")
        ->required();
    app.add_option("--source-stream-class", argument_source_stream_class,
        "Specifies the stream-class used for reading the source CZI-file. If not specified, the default file-reader stream-class is used."
        " Run with argument '--version' to get a list of available stream-classes.")
        ->option_text("STREAMCLASS");
    app.add_option("--propbag-source-stream-creation", argument_source_stream_creation_propbag,
        "Specifies the property-bag used for creating the stream used for reading the source CZI-file. The data is given in JSON-notation.")
        ->option_text("PROPBAG");
    app.add_option("-d,--destination", destination_filename, "The destination CZI-file to be written. If \"nul\" is specified here, then the processed data is not written out, it is discarded instead.")
        ->option_text("DESTINATION_FILE")
        ->required();
    app.add_option("-o,--operation", operation_type, "Specifies the mode of operation. Possible values are 'Deskew', 'CoverGlassTransform', 'CoverGlassTransform_and_xy_rotated' and 'Identify'.")
        ->option_text("MODE_OF_OPERATION")
        ->default_val(OperationType::Identity)
        ->transform(CLI::CheckedTransformer(map_string_to_operationtype, CLI::ignore_case));
    app.add_option("-i,--interpolation", interpolation,
        "Specifies the interpolation mode to be used. Possible values are 'NN' or 'NearestNeighbor', 'linear', "
        "'cubic', 'bspline', 'catmullrom' and 'b05c03'.")
        ->option_text("INTERPOLATION")
        ->default_val(Interpolation::kNearestNeighbor)
        ->transform(CLI::CheckedTransformer(map_string_to_interpolationmode, CLI::ignore_case));
    app.add_option("-r,--reader", libczi_reader, "Which libCZI-reader-implementation to use. Possible values are 'stock' and (on Windows) 'mmf'.")
        ->option_text("READER_IMPLEMENTATION")
        ->default_val(LibCziReaderImplementation::kStock)
        ->transform(CLI::CheckedTransformer(map_string_to_libczi_reader_implementation, CLI::ignore_case));
    app.add_option("-t,--number_of_reader_threads", number_of_reader_threads, "The number of reader-threads.")
        ->option_text("NUMBER_OF_READER_THREADS")
        ->default_val(1)
        ->check(CLI::NonNegativeNumber);
    app.add_option("-b,--bricksource", brick_reader_source, "Which brick-reader-implementation to use. Possible values are 'planereader', 'planereader2' or 'linearreading'.")
        ->option_text("BRICK_READER_IMPLEMENTATION")
        ->default_val(BrickReaderImplementation::kPlaneReader2)
        ->transform(CLI::CheckedTransformer(map_string_to_brick_source_implementation, CLI::ignore_case));
    app.add_option("-w,--warp_engine", warp_affine_engine_implementation,
        "Which warp-affine transformation implementation to use. Possible values are 'IPP', 'reference' or 'null'.")
        ->option_text("WARP_ENGINE_IMPLEMENTATION")
        ->default_val(CCmdLineOptions::kDefaultWarpAffineEngineImplementation)
        ->transform(CLI::CheckedTransformer(map_string_to_warp_affine_transformation_implementation, CLI::ignore_case));
    app.add_option("--stop_pipeline_after", test_stop_pipeline_after,
        "For testing: stop the pipeline after operation. Possible values are 'read', 'decompress' or 'none'.")
        ->option_text("STOP_AFTER_OPERATION")
        ->default_val(TestStopPipelineAfter::kNone)
        ->transform(CLI::CheckedTransformer(map_string_to_stop_pipeline_after_operation, CLI::ignore_case));
    app.add_option("--task_arena_implementation", task_arena_implementation, "For testing: choose the task-arena implementation. Currently, there is only one available: 'tbb'.")
        ->option_text("TASK_ARENA_IMPLEMENTATION")
        ->default_val(TaskArenaImplementation::kTBB)
        ->transform(CLI::CheckedTransformer(map_string_to_task_arena_implementation, CLI::ignore_case));
    app.add_option("-c,--compression_options", compression_options_text, "Specify compression parameters.")
        ->option_text("COMPRESSION_OPTIONS")
        ->default_val(CCmdLineOptions::kDefaultCompressionOptions);
    app.add_option("--parameters_bricksource", brickreader_parameters, "Specify parameters for the brick-reader");
    app.add_option("--verbosity", print_out_verbosity,
        "Specify the verbosity for messages from the application. Possible values are "
        "'maximal' (3), 'chatty' (2), 'normal' (1) or 'minimal' (0).")
        ->option_text("VERBOSITY")
        ->default_val(MessagesPrintVerbosity::kNormal)
        ->transform(CLI::CheckedTransformer(map_string_to_print_out_verbosity, CLI::ignore_case));
    app.add_flag("--hash-result", hash_result, "Calculate a hash for the result data.");
    app.add_option("-m,--max-tile-extent", max_tile_extent,
        "Specify the max width/height of a tile. If larger, the tile is split into smaller tiles. Default is 2048.")
        ->option_text("MAX_TILE_EXTENT")
        ->default_val(2048)
        ->check(CLI::PositiveNumber);
    app.add_option("--override-memory-size", override_ram_size_parameter,
        "Override the main-memory size.")
        ->option_text("RAM-SIZE")
        ->check(memory_size_validator);
    app.add_flag("--override-check-for-skewed-source", override_check_for_skewed_source,
        "Override check of source-document whether it is marked as containing 'skewed z-stacks'.");
    app.add_flag("--use-acquisition-tiles", use_acquisition_tiles,
        "Adds metadata to identify which subblocks were split during processing, but can be treated as one contiguous area.");

    auto formatter = make_shared<CustomFormatter>();
    app.formatter(formatter);

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::CallForHelp& e)
    {
        app.exit(e);
        return ParseResult::Exit;
    }
    catch (const CLI::ParseError& e)
    {
        app.exit(e);
        return ParseResult::Error;
    }

    this->cziSourceFilename = source_filename;
    this->cziDestinationFilename = destination_filename;
    this->interpolation_ = interpolation;
    this->type_of_operation_ = operation_type;
    this->libCziReaderImplementation_ = libczi_reader;
    this->number_of_reader_threads_ = number_of_reader_threads;
    this->brick_reader_implementation_ = brick_reader_source;
    this->warp_affine_engine_implementation_ = warp_affine_engine_implementation;
    this->test_stop_pipeline_after_ = test_stop_pipeline_after;
    this->task_arena_implementation_ = task_arena_implementation;
    this->compression_option_ = libCZI::Utils::ParseCompressionOptions(compression_options_text);
    this->verbosity_ = print_out_verbosity;
    this->hash_result_ = hash_result;
    this->max_tile_extent_ = max_tile_extent;
    this->override_check_for_skewed_source_ = override_check_for_skewed_source;
    this->use_acquisition_tiles_ = use_acquisition_tiles;
    this->source_stream_class_ = argument_source_stream_class;

    if (!override_ram_size_parameter.empty())
    {
        if (!Utilities::TryParseMemorySize(override_ram_size_parameter, &this->override_main_memory_size_))
        {
            return ParseResult::Error;
        }
    }

    if (!brickreader_parameters.empty())
    {
        PropertyBagTools::ParseFromString(
            this->property_bag_brick_source_,
            brickreader_parameters,
            [](const string& key)->PropertyBagTools::ValueType
            {
                // "max_number_of_subblocks_to_wait_for"
                if (key == ICziBrickReader::kPropertyBagKey_LinearReader_max_number_of_subblocks_to_wait_for)
                {
                    return PropertyBagTools::ValueType::kInt32;
                }

                return PropertyBagTools::ValueType::kString;
            });
    }

    if (!argument_source_stream_creation_propbag.empty())
    {
        const bool b = TryParseInputStreamCreationPropertyBag(argument_source_stream_creation_propbag, &this->property_bag_for_stream_class);
        if (!b)
        {
            ostringstream string_stream;
            string_stream << "Error parsing argument for '--propbag-source-stream-creation' -> \"" << argument_source_stream_creation_propbag << "\".";
            throw logic_error(string_stream.str());
        }
    }

    return ParseResult::OK;
}

std::wstring CCmdLineOptions::GetSourceCZIFilenameW() const
{
    return Utilities::convertToWide(this->cziSourceFilename);
}

std::wstring CCmdLineOptions::GetDestinationCZIFilenameW() const
{
    return Utilities::convertToWide(this->cziDestinationFilename);
}

bool CCmdLineOptions::GetUseNullWriter() const
{
    return this->cziDestinationFilename == "nul";
}

/*static*/bool CCmdLineOptions::TryParseInputStreamCreationPropertyBag(const std::string& s, std::map<int, libCZI::StreamsFactory::Property>* property_bag)
{
    // Here we parse the JSON-formatted string that contains the property bag for the input stream and
    //  construct a map<int, libCZI::StreamsFactory::Property> from it.

    int property_info_count;
    const libCZI::StreamsFactory::StreamPropertyBagPropertyInfo* property_infos = libCZI::StreamsFactory::GetStreamPropertyBagPropertyInfo(&property_info_count);

    rapidjson::Document document;
    document.Parse(s.c_str());
    if (document.HasParseError() || !document.IsObject())
    {
        return false;
    }

    for (rapidjson::Value::ConstMemberIterator itr = document.MemberBegin(); itr != document.MemberEnd(); ++itr)
    {
        if (!itr->name.IsString())
        {
            return false;
        }

        string name = itr->name.GetString();
        size_t index_of_key = numeric_limits<size_t>::max();
        for (size_t i = 0; i < static_cast<size_t>(property_info_count); ++i)
        {
            if (name == property_infos[i].property_name)
            {
                index_of_key = i;
                break;
            }
        }

        if (index_of_key == numeric_limits<size_t>::max())
        {
            return false;
        }

        switch (property_infos[index_of_key].property_type)
        {
        case libCZI::StreamsFactory::Property::Type::String:
            if (!itr->value.IsString())
            {
                return false;
            }

            if (property_bag != nullptr)
            {
                property_bag->insert(std::make_pair(property_infos[index_of_key].property_id, libCZI::StreamsFactory::Property(itr->value.GetString())));
            }

            break;
        case libCZI::StreamsFactory::Property::Type::Boolean:
            if (!itr->value.IsBool())
            {
                return false;
            }

            if (property_bag != nullptr)
            {
                property_bag->insert(std::make_pair(property_infos[index_of_key].property_id, libCZI::StreamsFactory::Property(itr->value.GetBool())));
            }

            break;
        case libCZI::StreamsFactory::Property::Type::Int32:
            if (!itr->value.IsInt())
            {
                return false;
            }

            if (property_bag != nullptr)
            {
                property_bag->insert(std::make_pair(property_infos[index_of_key].property_id, libCZI::StreamsFactory::Property(itr->value.GetInt())));
            }

            break;
        default:
            // this actually indicates an internal error - the table property_infos contains a not yet implemented property type
            return false;
        }
    }

    return true;
}

