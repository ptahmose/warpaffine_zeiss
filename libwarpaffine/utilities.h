// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <variant>
#include <map>
#include <memory>
#include <vector>
#include <array>
#include <functional>
#include "cmdlineoptions_enums.h"
#include "operationtype.h"
#include "inc_libCZI.h"

class Utilities
{
public:
    /// Converts the specified wide string to its UTF8-representation.
    /// \param str  The (wide) source string.
    /// \returns The given string converted to an UTF8-representation.
    static std::string convertToUtf8(const std::wstring& str);

    /// Converts the UTF8-encoded string 'str' to a wide-string representation.
    ///
    /// \param  str The UTF8-encode string to convert.
    ///
    /// \returns    The given data converted to a wide string.
    static std::wstring convertToWide(const std::string& str);

    /// Format the specified value (representing a duration in seconds) to a
    /// textual representation.
    /// \param  seconds The seconds.
    /// \returns    The formatted time in seconds.
    static std::string format_time_in_seconds(double seconds);

    static std::string InterpolationToInformativeString(Interpolation interpolation);

    static std::string FormatMemorySize(std::uint64_t size, const char* text_between_number_and_unit = nullptr);
    static bool TryParseMemorySize(const std::string& text, std::uint64_t* size);

    static const char* BrickReaderImplementationToInformalString(BrickReaderImplementation brick_reader_implementation);
    static const char* LibCziReaderImplementationToInformalString(LibCziReaderImplementation libczi_reader_implementation);
    static const char* OperationTypeToInformalString(OperationType operation_type);

    static void CopyBitmap(libCZI::PixelType pixel_type, const void* ptr_source, uint32_t source_stride, void* ptr_destination, uint32_t destination_stride, int width, int height);

    /// This struct gathers the information for the "copy bitmap at position" operation.
    struct CopyAtOffsetInfo
    {
        int xOffset;                        ///< The offset in x direction where the source bitmap is to be placed in the destination bitmap.
        int yOffset;                        ///< The offset in x direction where the source bitmap is to be placed in the destination bitmap.
        libCZI::PixelType pixelType;        ///< The pixel type (of both the source and the destination bitmap).
        const void* srcPtr;                 ///< Pointer to the source bitmap.
        uint32_t srcStride;                 ///< The stride of the source bitmap in bytes.
        int srcWidth;                       ///< The width of the source bitmap in pixels.
        int srcHeight;                      ///< The height of the source bitmap in pixels.
        void* dstPtr;                       ///< Pointer to the destination bitmap.
        uint32_t dstStride;                 ///< The stride of the destination bitmap in bytes.
        int dstWidth;                       ///< The width of the destination bitmap in pixels.
        int dstHeight;                      ///< The height of the destination bitmap in pixels.
    };

    /// Copies the source bitmap into a destination bitmap at a specific position. If the source bitmap
    /// is extending over the destination bitmap, then only the overlapping part is copied.
    /// 
    /// \param  info The information describing the operation.
    static void CopyBitmapAtOffset(const CopyAtOffsetInfo& info);

    /// Copies the source bitmap into a destination bitmap at a specific position. If the source bitmap
    /// is extending over the destination bitmap, then only the overlapping part is copied.
    /// The part of the destination bitmap to which we did not copy the source bitmap
    /// will be cleared (filled with zero).
    /// 
    /// \param  info The information describing the operation.
    static void CopyBitmapAtOffsetAndClearNonCoveredArea(const CopyAtOffsetInfo& info);

    /// Fills the specified region in the specified bitmap with zero. The region may extend outside
    /// the bitmap, operation will be clipped.
    ///
    /// \param          pixel_type         The pixel type.
    /// \param          ptr                The pointer to the bitmap.
    /// \param          stride             The stride in bytes.
    /// \param          width              The width in pixels.
    /// \param          height             The height in pixels.
    /// \param          region_of_interest The region of interest (which is to be cleared).
    static void ClearBitmap(libCZI::PixelType pixel_type, void* ptr, uint32_t stride, uint32_t width, uint32_t height, libCZI::IntRect region_of_interest);

    /// Convert the specified array into a string representation (a hex-string).
    /// \param  hash_code   The hash code.
    /// \returns    The string representation.
    static std::string HashToString(const std::array<std::uint8_t, 16>& hash_code);

    inline static void ExecuteIfVerbosityAboveOrEqual(MessagesPrintVerbosity verbosity_setting, MessagesPrintVerbosity verbosity_of_message, const std::function<void()>& func)
    {
        if (static_cast<std::underlying_type_t<MessagesPrintVerbosity>>(verbosity_of_message) <=
            static_cast<std::underlying_type_t<MessagesPrintVerbosity>>(verbosity_setting))
        {
            func();
        }
    }

    static int StrcmpCaseInsensitive(const char* a, const char* b);
    
    static libCZI::GUID GenerateGuid();
};

class IPropBag
{
public:
    typedef std::variant<bool, int, std::string> Variant;

    virtual bool TryGetValue(const std::string& key, Variant* value) const = 0;

    virtual ~IPropBag() = default;

    int GetInt32OrDefault(const std::string& key, int default_value) const;
};

class PropertyBag : public IPropBag
{
private:
    std::map<std::string, Variant> key_value_store_;
public:
    void AddOrSet(const std::string& key, const Variant& value);
public:
    bool TryGetValue(const std::string& key, Variant* value) const override;
};

class PropertyBagTools
{
public:
    enum class  ValueType
    {
        kBoolean,
        kInt32,
        kString
    };

    static PropertyBag CreateFromString(const std::string& text, const std::function<ValueType(const std::string&)>& func_determine_type);
    static std::unique_ptr<IPropBag> CreateUpFromString(const std::string& text, const std::function<ValueType(const std::string&)>& func_determine_type);
    static void ParseFromString(PropertyBag& property_bag, const std::string& text, const std::function<ValueType(const std::string&)>& func_determine_type);
private:
    static void SplitAtSemicolon(const std::string& text, const std::function<void(const std::string& part)>& func);
    static bool IsEmptyOrAllWhitespace(const std::string& text);
    static bool TryParsePart(const std::string& part, std::string& key, std::string& value);
    static std::string Trim(const std::string& text, const std::string& whitespace = " \t");
    static bool TryParseIntoVariant(const std::string& text, PropertyBagTools::ValueType type, IPropBag::Variant& variant);
    static bool icasecmp(const std::string& l, const std::string& r);
    static bool TryParseInt32(const std::string& text, int* value);
};
