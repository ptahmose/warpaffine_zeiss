// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "utilities.h"
#include "utilities_windows.h"
#include <locale>
#include <codecvt>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <regex>

#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
#include <ipp.h>
#endif

#if LIBWARPAFFINE_WIN32_ENVIRONMENT
#include <Windows.h>
#else
#include <random>
#endif

using namespace std;

std::string Utilities::convertToUtf8(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
    std::string conv = utf8_conv.to_bytes(str);
    return conv;
}

std::wstring Utilities::convertToWide(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8conv;
    std::wstring conv = utf8conv.from_bytes(str);
    return conv;
}

string Utilities::format_time_in_seconds(double seconds)
{
    uint64_t seconds_integer = static_cast<uint64_t>(seconds + 0.5);
    uint64_t hours = seconds_integer / (60 * 60);
    seconds_integer -= hours * 60 * 60;
    uint64_t minutes = seconds_integer / 60;
    seconds_integer -= minutes * 60;

    ostringstream ss;
    ss << hours << "h " << std::setfill('0') << std::setw(2) << minutes << "m " << std::setw(2) << seconds_integer << "s";
    return ss.str();
}

std::string Utilities::FormatMemorySize(std::uint64_t size, const char* text_between_number_and_unit/*= nullptr*/)
{
    const char* text_to_insert = (text_between_number_and_unit != nullptr) ? text_between_number_and_unit : "";
    ostringstream string_stream;
    if (size < 1000)
    {
        string_stream << size << text_to_insert << "B";
    }
    else if (size < 1000 * 1000)
    {
        string_stream << fixed << setprecision(2) << static_cast<double>(size) / 1000 << text_to_insert << "kB";
    }
    else if (size < 1000 * 1000 * 1000)
    {
        string_stream << fixed << setprecision(2) << static_cast<double>(size) / (1000 * 1000) << text_to_insert << "MB";
    }
    else if (size < 1000 * 1000 * 1000 * 1000ull)
    {
        string_stream << fixed << setprecision(2) << static_cast<double>(size) / (1000 * 1000 * 1000) << text_to_insert << "GB";
    }
    else
    {
        string_stream << fixed << setprecision(2) << static_cast<double>(size) / (1000 * 1000 * 1000 * 1000ull) << text_to_insert << "TB";
    }

    return string_stream.str();
}

bool Utilities::TryParseMemorySize(const std::string& text, std::uint64_t* size)
{
    regex regex(R"(^\s*([+]?(?:[0-9]+(?:[.][0-9]*)?|[.][0-9]+))\s*(k|m|g|t|ki|mi|gi|ti)(?:b?)\s*$)", regex_constants::icase);
    smatch match;
    regex_search(text, match, regex);
    if (match.size() != 3)
    {
        return false;
    }

    double number;

    try
    {
        number = stod(match[1].str());
    }
    catch (invalid_argument&)
    {
        return false;
    }
    catch (out_of_range&)
    {
        return false;
    }

    uint64_t factor;
    string suffix_string = match[2].str();
    if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "k") == 0)
    {
        factor = 1000;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "ki") == 0)
    {
        factor = 1024;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "m") == 0)
    {
        factor = 1000 * 1000;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "mi") == 0)
    {
        factor = 1024 * 1024;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "g") == 0)
    {
        factor = 1000 * 1000 * 1000;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "gi") == 0)
    {
        factor = 1024 * 1024 * 1024;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "t") == 0)
    {
        factor = 1000ULL * 1000 * 1000 * 1000;
    }
    else if (Utilities::StrcmpCaseInsensitive(suffix_string.c_str(), "ti") == 0)
    {
        factor = 1024ULL * 1024 * 1024 * 1024;
    }
    else
    {
        return false;
    }

    const uint64_t memory_size = static_cast<uint64_t>(number * static_cast<double>(factor) + 0.5);
    if (size != nullptr)
    {
        *size = memory_size;
    }

    return true;
}

std::string Utilities::InterpolationToInformativeString(Interpolation interpolation)
{
    switch (interpolation)
    {
    case Interpolation::kNearestNeighbor: return "NearestNeighbor";
    case Interpolation::kBilinear: return "linear";
    case Interpolation::kBicubic: return  "cubic";
    case Interpolation::kBSpline: return  "bspline";
    case Interpolation::kCatMullRom: return  "catmullrom";
    case Interpolation::kB05c03: return "b05c03";
    default: return "unknown";
    }
}

const char* Utilities::BrickReaderImplementationToInformalString(BrickReaderImplementation brick_reader_implementation)
{
    switch (brick_reader_implementation)
    {
    case BrickReaderImplementation::kPlaneReader:
        return "planereader";
    case BrickReaderImplementation::kPlaneReader2:
        return "planereader2";
    case BrickReaderImplementation::kLinearReading:
        return "linearreading";
    }

    return "invalid";
}

const char* Utilities::LibCziReaderImplementationToInformalString(LibCziReaderImplementation libczi_reader_implementation)
{
    switch (libczi_reader_implementation)
    {
    case LibCziReaderImplementation::kStock:
        return "stock";
    case LibCziReaderImplementation::kMmf:
        return "mmf";
    }

    return "invalid";
}

const char* Utilities::OperationTypeToInformalString(OperationType operation_type)
{
    switch (operation_type)
    {
    case OperationType::Identity:
        return "identity";
    case OperationType::Deskew:
        return "deskew";
    case OperationType::CoverGlassTransform:
        return "cover glass transform";
    case OperationType::CoverGlassTransformAndXYRotated:
        return "cover glass transform and XY rotated";
    }

    return "invalid";
}

std::string Utilities::HashToString(const std::array<std::uint8_t, 16>& hash_code)
{
    ostringstream ss;
    for (int i = 0; i < 16; ++i)
    {
        ss << uppercase << setw(2) << hex << setfill('0') << static_cast<int>(hash_code[i]);
    }

    return ss.str();
}

#if !LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
static void CopyStrided(const void* ptr_source, uint32_t source_stride, void* ptr_destination, uint32_t destination_stride, size_t line_length, int height)
{
    const uint8_t* ptr_source_byte = static_cast<const uint8_t*>(ptr_source);
    uint8_t* ptr_destination_byte = static_cast<uint8_t*>(ptr_destination);

    for (int y = 0; y < height; ++y)
    {
        memcpy(ptr_destination_byte, ptr_source_byte, line_length);
        ptr_source_byte += source_stride;
        ptr_destination_byte += destination_stride;
    }
}
#endif


void Utilities::CopyBitmap(libCZI::PixelType pixel_type, const void* ptr_source, uint32_t source_stride, void* ptr_destination, uint32_t destination_stride, int width, int height)
{
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
    switch (pixel_type)
    {
    case libCZI::PixelType::Gray16:
        ippiCopy_16u_C1R(
            static_cast<const Ipp16u*>(ptr_source),
            source_stride,
            static_cast<Ipp16u*>(ptr_destination),
            destination_stride,
            IppiSize{ width, height });
        break;
    case libCZI::PixelType::Gray8:
        ippiCopy_8u_C1R(
            static_cast<const Ipp8u*>(ptr_source),
            source_stride,
            static_cast<Ipp8u*>(ptr_destination),
            destination_stride,
            IppiSize{ width, height });
        break;
    case libCZI::PixelType::Gray32Float:
        ippiCopy_32f_C1R(
            static_cast<const Ipp32f*>(ptr_source),
            source_stride,
            static_cast<Ipp32f*>(ptr_destination),
            destination_stride,
            IppiSize{ width, height });
        break;
    default:
        throw invalid_argument("unsupported pixeltype");
    }
#else
    switch (pixel_type)
    {
    case libCZI::PixelType::Gray16:
        CopyStrided(ptr_source, source_stride, ptr_destination, destination_stride, width * 2, height);
        break;
    case libCZI::PixelType::Gray8:
        CopyStrided(ptr_source, source_stride, ptr_destination, destination_stride, width, height);
        break;
    case libCZI::PixelType::Gray32Float:
        CopyStrided(ptr_source, source_stride, ptr_destination, destination_stride, width * sizeof(float), height);
        break;
    default:
        throw invalid_argument("unsupported pixeltype");
    }
#endif
}

/*static*/void Utilities::CopyBitmapAtOffset(const CopyAtOffsetInfo& info)
{
    const libCZI::IntRect srcRect = libCZI::IntRect{ info.xOffset, info.yOffset, info.srcWidth, info.srcHeight };
    const libCZI::IntRect dstRect = libCZI::IntRect{ 0, 0, info.dstWidth, info.dstHeight };
    const libCZI::IntRect intersection = libCZI::IntRect::Intersect(srcRect, dstRect);

    if (intersection.w == 0 || intersection.h == 0)
    {
        return;
    }

    void* ptrDestination = static_cast<char*>(info.dstPtr) + intersection.y * static_cast<size_t>(info.dstStride) + intersection.x * static_cast<size_t>(libCZI::Utils::GetBytesPerPixel(info.pixelType));
    const void* ptrSource = static_cast<const char*>(info.srcPtr) + (std::max)(-info.yOffset, 0) * static_cast<size_t>(info.srcStride) + (std::max)(-info.xOffset, 0) * static_cast<size_t>(libCZI::Utils::GetBytesPerPixel(info.pixelType));

    Utilities::CopyBitmap(
                        info.pixelType,
                        ptrSource,
                        info.srcStride,
                        ptrDestination,
                        info.dstStride,
                        intersection.w,
                        intersection.h);
}

/*static*/void Utilities::CopyBitmapAtOffsetAndClearNonCoveredArea(const CopyAtOffsetInfo& info)
{
    const libCZI::IntRect srcRect = libCZI::IntRect{ info.xOffset, info.yOffset, info.srcWidth, info.srcHeight };
    const libCZI::IntRect dstRect = libCZI::IntRect{ 0, 0, info.dstWidth, info.dstHeight };
    const libCZI::IntRect intersection = libCZI::IntRect::Intersect(srcRect, dstRect);

    if (intersection.w == 0 || intersection.h == 0)
    {
        // in this case we have to clear the complete destination bitmap
        Utilities::ClearBitmap(info.pixelType, info.dstPtr, info.dstStride, info.dstWidth, info.dstHeight, { 0, 0, info.dstWidth, info.dstHeight });
        return;
    }

    void* ptrDestination = static_cast<char*>(info.dstPtr) + intersection.y * static_cast<size_t>(info.dstStride) + intersection.x * static_cast<size_t>(libCZI::Utils::GetBytesPerPixel(info.pixelType));
    const void* ptrSource = static_cast<const char*>(info.srcPtr) + (std::max)(-info.yOffset, 0) * static_cast<size_t>(info.srcStride) + (std::max)(-info.xOffset, 0) * static_cast<size_t>(libCZI::Utils::GetBytesPerPixel(info.pixelType));
    Utilities::CopyBitmap(
                        info.pixelType,
                        ptrSource,
                        info.srcStride,
                        ptrDestination,
                        info.dstStride,
                        intersection.w,
                        intersection.h);

    // check for the frequent case that the destination bitmap is completely covered by the source bitmap,
    //  in which case we don't have to clear anything
    if (intersection.w != info.dstWidth || intersection.h != info.dstHeight)
    {
        // we now fill the "non-covered parts of the destination bitmap" (i.e. those parts which were not written to above)

        // the "top" part 
        Utilities::ClearBitmap(info.pixelType, info.dstPtr, info.dstStride, info.dstWidth, info.dstHeight, { 0, 0, info.dstWidth, intersection.y });


        // the "left" part
        Utilities::ClearBitmap(info.pixelType, info.dstPtr, info.dstStride, info.dstWidth, info.dstHeight, { 0, intersection.y, intersection.x, intersection.h });


        // the "left" part
        Utilities::ClearBitmap(info.pixelType, info.dstPtr, info.dstStride, info.dstWidth, info.dstHeight, { intersection.x + intersection.w, intersection.y, info.dstWidth, intersection.h });


        // the "bottom" part
        Utilities::ClearBitmap(info.pixelType, info.dstPtr, info.dstStride, info.dstWidth, info.dstHeight, { 0, intersection.y + intersection.h, info.dstWidth, info.dstHeight });
    }
}

/*static*/void Utilities::ClearBitmap(libCZI::PixelType pixel_type, void* ptr, uint32_t stride, uint32_t width, uint32_t height, libCZI::IntRect region_of_interest)
{
    libCZI::IntRect roi_clipped = region_of_interest.Intersect(libCZI::IntRect{ 0, 0, static_cast<int>(width), static_cast<int>(height) });
    if (roi_clipped.w == 0 || roi_clipped.h == 0)
    {
        return;
    }

#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE  
    switch (pixel_type)
    {
    case libCZI::PixelType::Gray16:
        ippiSet_16u_C1R(
            0,
            reinterpret_cast<Ipp16u*>(static_cast<uint8_t*>(ptr) + roi_clipped.y * static_cast<size_t>(stride) + roi_clipped.x * sizeof(Ipp16u)),
            stride,
            IppiSize{ roi_clipped.w, roi_clipped.h });
        break;
    case libCZI::PixelType::Gray8:
        ippiSet_8u_C1R(
            0,
            reinterpret_cast<Ipp8u*>(static_cast<uint8_t*>(ptr) + roi_clipped.y * static_cast<size_t>(stride) + roi_clipped.x * sizeof(Ipp8u)),
            stride,
            IppiSize{ roi_clipped.w, roi_clipped.h });
        break;
    case libCZI::PixelType::Gray32Float:
        ippiSet_32f_C1R(
            0,
            reinterpret_cast<Ipp32f*>(static_cast<uint8_t*>(ptr) + roi_clipped.y * static_cast<size_t>(stride) + roi_clipped.x * sizeof(Ipp32f)),
            stride,
            IppiSize{ roi_clipped.w, roi_clipped.h });
        break;
    default:
        throw invalid_argument("unsupported pixeltype");
    }
#else
    std::uint8_t* p;
    switch (pixel_type)
    {
    case libCZI::PixelType::Gray16:
        p = static_cast<uint8_t*>(ptr) + roi_clipped.y * static_cast<size_t>(stride) + roi_clipped.x * 2;
        for (int y = 0; y < roi_clipped.h; ++y)
        {
            memset(p, 0, roi_clipped.w * 2);
            p += stride;
        }

        break;
    case libCZI::PixelType::Gray8:
        p = static_cast<uint8_t*>(ptr) + roi_clipped.y * static_cast<size_t>(stride) + roi_clipped.x;
        for (int y = 0; y < roi_clipped.h; ++y)
        {
            memset(p, 0, roi_clipped.w);
            p += stride;
        }

        break;
    case libCZI::PixelType::Gray32Float:
        p = static_cast<uint8_t*>(ptr) + roi_clipped.y * static_cast<size_t>(stride) + roi_clipped.x * 4;
        for (int y = 0; y < roi_clipped.h; ++y)
        {
            memset(p, 0, roi_clipped.w * 4);
            p += stride;
        }

        break;
    default:
        throw invalid_argument("unsupported pixeltype");
    }
#endif
}

int Utilities::StrcmpCaseInsensitive(const char* a, const char* b)
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    return _stricmp(a, b);
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    return strcasecmp(a, b);
#endif
}

libCZI::GUID Utilities::GenerateGuid()
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    ::GUID guid;
    CoCreateGuid(&guid);
    libCZI::GUID guid_value
    {
        guid.Data1,
            guid.Data2,
            guid.Data3,
        { guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7] } };
    return guid_value;
#else
    std::mt19937 rng;
    rng.seed(std::random_device()());
    uniform_int_distribution<uint32_t> distu32;
    libCZI::GUID g;
    g.Data1 = distu32(rng);
    auto r = distu32(rng);
    g.Data2 = (uint16_t)r;
    g.Data3 = (uint16_t)(r >> 16);

    r = distu32(rng);
    for (int i = 0; i < 4; ++i)
    {
        g.Data4[i] = (uint8_t)r;
        r >>= 8;
    }

    r = distu32(rng);
    for (int i = 4; i < 8; ++i)
    {
        g.Data4[i] = (uint8_t)r;
        r >>= 8;
    }

    return g;
#endif
}

//-----------------------------------------------------------------------------------------

#if LIBWARPAFFINE_WIN32_ENVIRONMENT
CommandlineArgsWindowsHelper::CommandlineArgsWindowsHelper()
{
    int number_arguments;
    const unique_ptr<LPWSTR, decltype(LocalFree)*> wide_argv
    {
        CommandLineToArgvW(GetCommandLineW(), &number_arguments),
        &LocalFree
    };

    this->pointers_to_arguments_.reserve(number_arguments);
    this->arguments_.reserve(number_arguments);

    for (int i = 0; i < number_arguments; ++i)
    {
        this->arguments_.emplace_back(Utilities::convertToUtf8(wide_argv.get()[i]));
    }

    for (int i = 0; i < number_arguments; ++i)
    {
        this->pointers_to_arguments_.emplace_back(
            this->arguments_[i].data());
    }
}

char** CommandlineArgsWindowsHelper::GetArgv()
{
    return this->pointers_to_arguments_.data();
}

int CommandlineArgsWindowsHelper::GetArgc()
{
    return static_cast<int>(this->pointers_to_arguments_.size());
}
#endif


//-----------------------------------------------------------------------------------------

int IPropBag::GetInt32OrDefault(const std::string& key, int default_value) const
{
    Variant value;
    if (this->TryGetValue(key, &value))
    {
        if (holds_alternative<int>(value))
        {
            return get<int>(value);
        }
    }

    return default_value;
}

void PropertyBag::AddOrSet(const std::string& key, const Variant& value)
{
    this->key_value_store_[key] = value;
}

/*virtual*/bool PropertyBag::TryGetValue(const std::string& key, Variant* value) const
{
    const auto& iterator = this->key_value_store_.find(key);
    if (iterator == this->key_value_store_.cend())
    {
        return false;
    }

    if (value != nullptr)
    {
        *value = iterator->second;
    }

    return true;
}

/*static*/PropertyBag PropertyBagTools::CreateFromString(const std::string& text, const std::function<ValueType(const std::string&)>& func_determine_type)
{
    PropertyBag property_bag;
    PropertyBagTools::ParseFromString(property_bag, text, func_determine_type);
    return property_bag;
}

/*static*/std::unique_ptr<IPropBag> PropertyBagTools::CreateUpFromString(const std::string& text, const std::function<ValueType(const std::string&)>& func_determine_type)
{
    unique_ptr<PropertyBag> property_bag = make_unique<PropertyBag>();
    PropertyBagTools::ParseFromString(*property_bag, text, func_determine_type);
    return property_bag;
}

/*static*/void PropertyBagTools::ParseFromString(
    PropertyBag& property_bag,
    const std::string& text,
    const std::function<ValueType(const std::string&)>& func_determine_type)
{
    SplitAtSemicolon(
        text,
        [&](const string& part)->void
        {
            string key, value;
            bool b = TryParsePart(part, key, value);
            if (b == false)
            {
                throw invalid_argument("Error parsing the text.");
            }

            // try to determine the type
            PropertyBagTools::ValueType type = PropertyBagTools::ValueType::kString;
            if (func_determine_type)
            {
                type = func_determine_type(key);
            }

            IPropBag::Variant variant;
            b = TryParseIntoVariant(value, type, variant);
            if (b == false)
            {
                throw invalid_argument("Error parsing the text.");
            }

            property_bag.AddOrSet(key, variant);
        });
}

/*static*/void PropertyBagTools::SplitAtSemicolon(const std::string& text, const std::function<void(const std::string& part)>& func)
{
    size_t pos = 0;
    for (;;)
    {
        const size_t position_of_next_semicolon = text.find(';', pos);
        string part;
        if (position_of_next_semicolon == string::npos)
        {
            part = text.substr(pos);
        }
        else
        {
            part = text.substr(pos, position_of_next_semicolon - pos);
        }

        if (!IsEmptyOrAllWhitespace(part))
        {
            func(part);
        }

        if (position_of_next_semicolon == string::npos)
        {
            break;
        }

        pos = position_of_next_semicolon + 1;
    }
}

/*static*/bool PropertyBagTools::TryParsePart(const std::string& part, std::string& key, std::string& value)
{
    // first, find the "equal sign" in the string
    const auto position_of_equal_sign = part.find('=');
    if (position_of_equal_sign == string::npos)
    {
        return false;
    }

    key = PropertyBagTools::Trim(part.substr(0, position_of_equal_sign));
    value = PropertyBagTools::Trim(part.substr(position_of_equal_sign + 1));

    return !PropertyBagTools::IsEmptyOrAllWhitespace(key) && !PropertyBagTools::IsEmptyOrAllWhitespace(value);
}

/*static*/bool PropertyBagTools::IsEmptyOrAllWhitespace(const std::string& text)
{
    for (const auto c : text)
    {
        if (!isspace(c))
        {
            return false;
        }
    }

    return true;
}

/*static*/string PropertyBagTools::Trim(const std::string& text, const std::string& whitespace /*= " \t"*/)
{
    const auto strBegin = text.find_first_not_of(whitespace);
    if (strBegin == string::npos)
    {
        return {}; // no content
    }

    const auto strEnd = text.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return text.substr(strBegin, strRange);
}

/*static*/bool PropertyBagTools::TryParseIntoVariant(const std::string& text, PropertyBagTools::ValueType type, IPropBag::Variant& variant)
{
    switch (type)
    {
    case PropertyBagTools::ValueType::kString:
        variant.emplace<string>(text);
        return true;
    case PropertyBagTools::ValueType::kBoolean:
        if (icasecmp(text, "true") || icasecmp(text, "on") || icasecmp(text, "1"))
        {
            variant.emplace<bool>(true);
            return true;
        }

        if (icasecmp(text, "false") || icasecmp(text, "off") || icasecmp(text, "0"))
        {
            variant.emplace<bool>(true);
            return true;
        }

        return false;
    case PropertyBagTools::ValueType::kInt32:
        int value;
        if (TryParseInt32(text, &value))
        {
            variant.emplace<int>(value);
            return true;
        }

        return false;
    }

    return false;
}

/*static*/bool PropertyBagTools::icasecmp(const std::string& l, const std::string& r)
{
    return l.size() == r.size()
        && equal(l.cbegin(), l.cend(), r.cbegin(),
            [](std::string::value_type l1, std::string::value_type r1)->bool
            {
                return toupper(l1) == toupper(r1);
            });
}

/*static*/bool PropertyBagTools::TryParseInt32(const std::string& text, int* value)
{
    size_t number_of_characters_processed;
    try
    {
        int v = stoi(text, &number_of_characters_processed, 10);
        if (number_of_characters_processed != text.size())
        {
            return false;
        }

        if (value != nullptr)
        {
            *value = v;
        }

        return true;
    }
    catch (invalid_argument&)
    {
        return false;
    }
    catch (out_of_range&)
    {
        return false;
    }
}
