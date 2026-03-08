// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include "../libwarpaffine/czi_helpers.h"
#include "mem_output_stream.h"
#include <charconv>
#include <limits>
#include <cmath>
#include <array>
#include <sstream>
#include <locale>
#include <iomanip>

using namespace std;
using namespace libCZI;

class CMemBitmapWrapper : public libCZI::IBitmapData
{
private:
    void* ptrData;
    libCZI::PixelType pixeltype;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    int lockCount{ 0 };
public:
    CMemBitmapWrapper(libCZI::PixelType pixeltype, std::uint32_t width, std::uint32_t height) :pixeltype(pixeltype), width(width), height(height)
    {
        int bytesPerPel;
        switch (pixeltype)
        {
        case PixelType::Bgr24:bytesPerPel = 3; break;
        case PixelType::Bgr48:bytesPerPel = 6; break;
        case PixelType::Gray8:bytesPerPel = 1; break;
        case PixelType::Gray16:bytesPerPel = 2; break;
        case PixelType::Gray32Float:bytesPerPel = 4; break;
        default: throw runtime_error("unsupported pixeltype");
        }

        if (pixeltype == PixelType::Bgr24)
        {
            this->stride = ((width * bytesPerPel + 3) / 4) * 4;
        }
        else
        {
            this->stride = width * bytesPerPel;
        }

        size_t s = static_cast<size_t>(this->stride) * height;
        this->ptrData = malloc(s);
    }

    ~CMemBitmapWrapper() override
    {
        free(this->ptrData);
    }

    libCZI::PixelType GetPixelType() const override
    {
        return this->pixeltype;
    }

    libCZI::IntSize	GetSize() const override
    {
        return libCZI::IntSize{ this->width, this->height };
    }

    libCZI::BitmapLockInfo Lock() override
    {
        ++this->lockCount;
        libCZI::BitmapLockInfo bitmapLockInfo;
        bitmapLockInfo.ptrData = this->ptrData;
        bitmapLockInfo.ptrDataRoi = this->ptrData;
        bitmapLockInfo.stride = this->stride;
        bitmapLockInfo.size = static_cast<uint64_t>(this->stride) * this->height;
        return bitmapLockInfo;
    }

    int GetLockCount() const override
    {
        return this->lockCount;
    }

    void Unlock() override
    {
        --this->lockCount;
    }
};

static std::shared_ptr<libCZI::IBitmapData> CreateTestBitmap(libCZI::PixelType pixeltype, std::uint32_t width, std::uint32_t height)
{
    auto bm = make_shared<CMemBitmapWrapper>(pixeltype, width, height);
    ScopedBitmapLockerSP lckBm{ bm };
    switch (bm->GetPixelType())
    {
    case PixelType::Gray8:
    case PixelType::Gray16:
    case PixelType::Bgr24:
    case PixelType::Bgr48:
    {
        uint8_t v = 0;
        for (std::uint64_t i = 0; i < lckBm.size; ++i)
        {
            static_cast<uint8_t*>(lckBm.ptrDataRoi)[i] = v++;
        }

        break;
    }

    default:
        throw  std::runtime_error("Not Implemented");
    }

    return bm;
}

TEST(Czi_Helpers, GetSubblocksForBrickNoMindexTest)
{
    // first we create a CZI-document (Z=0...9 C=0..1) in memory, which
    //  we then load and run our test
    auto writer = CreateCZIWriter();
    auto outStream = make_shared<CMemOutputStream>(0);

    auto spWriterInfo = make_shared<CCziWriterInfo>();
    writer->Create(outStream, spWriterInfo);
    auto bitmap = CreateTestBitmap(PixelType::Gray8, 4, 4);

    ScopedBitmapLockerSP lockBm{ bitmap };
    AddSubBlockInfoStridedBitmap addSbBlkInfo;

    for (int z = 0; z < 10; ++z)
    {
        for (int c = 0; c < 2; ++c)
        {
            addSbBlkInfo.Clear();
            addSbBlkInfo.coordinate.Set(DimensionIndex::C, c);
            addSbBlkInfo.coordinate.Set(DimensionIndex::Z, z);
            addSbBlkInfo.mIndexValid = true;
            addSbBlkInfo.mIndex = 0;
            addSbBlkInfo.x = 0;
            addSbBlkInfo.y = 0;
            addSbBlkInfo.logicalWidth = bitmap->GetWidth();
            addSbBlkInfo.logicalHeight = bitmap->GetHeight();
            addSbBlkInfo.physicalWidth = bitmap->GetWidth();
            addSbBlkInfo.physicalHeight = bitmap->GetHeight();
            addSbBlkInfo.PixelType = bitmap->GetPixelType();
            addSbBlkInfo.ptrBitmap = lockBm.ptrDataRoi;
            addSbBlkInfo.strideBitmap = lockBm.stride;

            writer->SyncAddSubBlock(addSbBlkInfo);
        }
    }

    PrepareMetadataInfo prepare_metadata_info;
    auto metaDataBuilder = writer->GetPreparedMetadata(prepare_metadata_info);

    WriteMetadataInfo write_metadata_info;
    const auto& strMetadata = metaDataBuilder->GetXml();
    write_metadata_info.szMetadata = strMetadata.c_str();
    write_metadata_info.szMetadataSize = strMetadata.size() + 1;
    write_metadata_info.ptrAttachment = nullptr;
    write_metadata_info.attachmentSize = 0;
    writer->SyncWriteMetadata(write_metadata_info);

    writer->Close();
    writer.reset();		// not needed anymore

    size_t cziData_Size;
    auto cziData = outStream->GetCopy(&cziData_Size);
    outStream.reset();	// not needed anymore

    // now, open the CZI-document from the memory-blob
    auto inputStream = CreateStreamFromMemory(cziData, cziData_Size);
    auto reader = CreateCZIReader();
    reader->Open(inputStream);

    auto brick_coordinate = CDimCoordinate::Parse("C0");
    auto map_z_subblocks = CziHelpers::GetSubblocksForBrick(reader.get(), brick_coordinate, TileIdentifier::GetForNoMIndexAndNoSceneIndex());
    ASSERT_EQ(map_z_subblocks.size(), 10);
    for (auto iterator = map_z_subblocks.cbegin(); iterator != map_z_subblocks.cend(); ++iterator)
    {
        auto sub_block = reader->ReadSubBlock(iterator->second);
        ASSERT_TRUE(sub_block);
        int z_coordinate;
        bool b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::Z, &z_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(z_coordinate, iterator->first);
        int c_coordinate;
        b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(c_coordinate, 0);
    }

    brick_coordinate = CDimCoordinate::Parse("C1");
    map_z_subblocks = CziHelpers::GetSubblocksForBrick(reader.get(), brick_coordinate, TileIdentifier::GetForNoMIndexAndNoSceneIndex());
    ASSERT_EQ(map_z_subblocks.size(), 10);
    for (auto iterator = map_z_subblocks.cbegin(); iterator != map_z_subblocks.cend(); ++iterator)
    {
        auto subblock = reader->ReadSubBlock(iterator->second);
        ASSERT_TRUE(subblock);
        int z_coordinate;
        bool b = subblock->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::Z, &z_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(z_coordinate, iterator->first);
        int c_coordinate;
        b = subblock->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(c_coordinate, 1);
    }
}

TEST(Czi_Helpers, GetSubblocksForBrickWithMindexTest)
{
    // first we create a CZI-document (Z=0...9 C=0..1 M=0..2) in memory, which
    //  we then load and run our test
    auto writer = CreateCZIWriter();
    auto outStream = make_shared<CMemOutputStream>(0);

    auto spWriterInfo = make_shared<CCziWriterInfo>();
    writer->Create(outStream, spWriterInfo);
    auto bitmap = CreateTestBitmap(PixelType::Gray8, 4, 4);

    ScopedBitmapLockerSP lockBm{ bitmap };
    AddSubBlockInfoStridedBitmap addSbBlkInfo;

    for (int z = 0; z < 10; ++z)
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int m = 0; m < 3; ++m)
            {
                addSbBlkInfo.Clear();
                addSbBlkInfo.coordinate.Set(DimensionIndex::C, c);
                addSbBlkInfo.coordinate.Set(DimensionIndex::Z, z);
                addSbBlkInfo.mIndexValid = true;
                addSbBlkInfo.mIndex = m;
                addSbBlkInfo.x = m * 10;
                addSbBlkInfo.y = 0;
                addSbBlkInfo.logicalWidth = bitmap->GetWidth();
                addSbBlkInfo.logicalHeight = bitmap->GetHeight();
                addSbBlkInfo.physicalWidth = bitmap->GetWidth();
                addSbBlkInfo.physicalHeight = bitmap->GetHeight();
                addSbBlkInfo.PixelType = bitmap->GetPixelType();
                addSbBlkInfo.ptrBitmap = lockBm.ptrDataRoi;
                addSbBlkInfo.strideBitmap = lockBm.stride;

                writer->SyncAddSubBlock(addSbBlkInfo);
            }
        }
    }

    PrepareMetadataInfo prepare_metadata_info;
    auto metaDataBuilder = writer->GetPreparedMetadata(prepare_metadata_info);

    WriteMetadataInfo write_metadata_info;
    const auto& strMetadata = metaDataBuilder->GetXml();
    write_metadata_info.szMetadata = strMetadata.c_str();
    write_metadata_info.szMetadataSize = strMetadata.size() + 1;
    write_metadata_info.ptrAttachment = nullptr;
    write_metadata_info.attachmentSize = 0;
    writer->SyncWriteMetadata(write_metadata_info);

    writer->Close();
    writer.reset();		// not needed anymore

    size_t cziData_Size;
    auto cziData = outStream->GetCopy(&cziData_Size);
    outStream.reset();	// not needed anymore

    // now, open the CZI-document from the memory-blob
    auto inputStream = CreateStreamFromMemory(cziData, cziData_Size);
    auto reader = CreateCZIReader();
    reader->Open(inputStream);

    auto brick_coordinate = CDimCoordinate::Parse("C0");
    auto map_z_subblocks = CziHelpers::GetSubblocksForBrick(reader.get(), brick_coordinate, TileIdentifier::GetForNoSceneIndex(0));
    ASSERT_EQ(map_z_subblocks.size(), 10);
    for (auto iterator = map_z_subblocks.cbegin(); iterator != map_z_subblocks.cend(); ++iterator)
    {
        auto sub_block = reader->ReadSubBlock(iterator->second);
        ASSERT_TRUE(sub_block);
        int z_coordinate;
        bool b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::Z, &z_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(z_coordinate, iterator->first);
        EXPECT_TRUE(sub_block->GetSubBlockInfo().IsMindexValid());
        EXPECT_EQ(sub_block->GetSubBlockInfo().mIndex, 0);
        int c_coordinate;
        b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(c_coordinate, 0);
    }

    brick_coordinate = CDimCoordinate::Parse("C1");
    map_z_subblocks = CziHelpers::GetSubblocksForBrick(reader.get(), brick_coordinate, TileIdentifier::GetForNoSceneIndex(2));
    ASSERT_EQ(map_z_subblocks.size(), 10);
    for (auto iterator = map_z_subblocks.cbegin(); iterator != map_z_subblocks.cend(); ++iterator)
    {
        auto sub_block = reader->ReadSubBlock(iterator->second);
        ASSERT_TRUE(sub_block);
        int z_coordinate;
        bool b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::Z, &z_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(z_coordinate, iterator->first);
        EXPECT_TRUE(sub_block->GetSubBlockInfo().IsMindexValid());
        EXPECT_EQ(sub_block->GetSubBlockInfo().mIndex, 2);
        int c_coordinate;
        b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(c_coordinate, 1);
    }
}

namespace
{
    string FormatDoubleForXml(double value)
    {
        // XSD requires the special floating-point values to be written as NaN, INF and -INF.
        // Handling them explicitly avoids implementation-specific spellings from stream formatting.
        if (std::isnan(value))
        {
            return "NaN";
        }

        if (std::isinf(value))
        {
            return std::signbit(value) ? "-INF" : "INF";
        }

        // Use to_chars for finite values because it is locale-independent and does not allocate.
        std::array<char, 64> buffer;
        const auto result = std::to_chars(
            buffer.data(),
            buffer.data() + buffer.size(),
            value,
            std::chars_format::general,
            std::numeric_limits<double>::max_digits10);

        if (result.ec == std::errc())
        {
            return string(buffer.data(), result.ptr);
        }

        // Fallback for platforms where floating-point to_chars is not fully available.
        // The classic locale guarantees '.' as decimal separator.
        ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return stream.str();
    }

    string GetSubBlockMetadataXml(double stage_pos_x, double stage_pos_y)
    {
        string metadata;
        metadata.reserve(192);
        metadata += "<METADATA><Tags>";
        metadata += "<StageXPosition>";
        metadata += FormatDoubleForXml(stage_pos_x);
        metadata += "</StageXPosition>";
        metadata += "<StageYPosition>";
        metadata += FormatDoubleForXml(stage_pos_y);
        metadata += "</StageYPosition>";
        metadata += "</Tags></METADATA>";
        return metadata;
    }
}

TEST(Czi_Helpers, GetSubblocksAndCheckGetStagePosition)
{
    // first we create a CZI-document (Z=0...9 C=0) in memory, which
    //  we then load and run our test
    auto writer = CreateCZIWriter();
    auto outStream = make_shared<CMemOutputStream>(0);

    auto spWriterInfo = make_shared<CCziWriterInfo>();
    writer->Create(outStream, spWriterInfo);
    auto bitmap = CreateTestBitmap(PixelType::Gray8, 4, 4);

    ScopedBitmapLockerSP lockBm{ bitmap };
    AddSubBlockInfoStridedBitmap addSbBlkInfo;

    for (int z = 0; z < 10; ++z)
    {
        addSbBlkInfo.Clear();
        addSbBlkInfo.coordinate.Set(DimensionIndex::C, 0);
        addSbBlkInfo.coordinate.Set(DimensionIndex::Z, z);
        addSbBlkInfo.mIndexValid = true;
        addSbBlkInfo.mIndex = 0;
        addSbBlkInfo.x = 0;
        addSbBlkInfo.y = 0;
        addSbBlkInfo.logicalWidth = bitmap->GetWidth();
        addSbBlkInfo.logicalHeight = bitmap->GetHeight();
        addSbBlkInfo.physicalWidth = bitmap->GetWidth();
        addSbBlkInfo.physicalHeight = bitmap->GetHeight();
        addSbBlkInfo.PixelType = bitmap->GetPixelType();
        addSbBlkInfo.ptrBitmap = lockBm.ptrDataRoi;
        addSbBlkInfo.strideBitmap = lockBm.stride;

        if (z % 2 == 0)
        {
            string metadata = GetSubBlockMetadataXml(1.5 * z, -2.5 * z);
            addSbBlkInfo.ptrSbBlkMetadata = metadata.c_str();
            addSbBlkInfo.sbBlkMetadataSize = static_cast<uint32_t>(metadata.size());
            writer->SyncAddSubBlock(addSbBlkInfo);
        }
        else
        {
            addSbBlkInfo.ptrSbBlkMetadata = nullptr;
            addSbBlkInfo.sbBlkMetadataSize = 0;
            writer->SyncAddSubBlock(addSbBlkInfo);
        }
    }

    PrepareMetadataInfo prepare_metadata_info;
    auto metaDataBuilder = writer->GetPreparedMetadata(prepare_metadata_info);

    WriteMetadataInfo write_metadata_info;
    const auto& strMetadata = metaDataBuilder->GetXml();
    write_metadata_info.szMetadata = strMetadata.c_str();
    write_metadata_info.szMetadataSize = strMetadata.size() + 1;
    write_metadata_info.ptrAttachment = nullptr;
    write_metadata_info.attachmentSize = 0;
    writer->SyncWriteMetadata(write_metadata_info);

    writer->Close();
    writer.reset();		// not needed anymore

    size_t cziData_Size;
    auto cziData = outStream->GetCopy(&cziData_Size);
    outStream.reset();	// not needed anymore

    // now, open the CZI-document from the memory-blob
    auto inputStream = CreateStreamFromMemory(cziData, cziData_Size);
    auto reader = CreateCZIReader();
    reader->Open(inputStream);

    auto brick_coordinate = CDimCoordinate::Parse("C0");
    auto map_z_subblocks = CziHelpers::GetSubblocksForBrick(reader.get(), brick_coordinate, TileIdentifier::GetForNoMIndexAndNoSceneIndex());
    ASSERT_EQ(map_z_subblocks.size(), 10);
    for (auto iterator = map_z_subblocks.cbegin(); iterator != map_z_subblocks.cend(); ++iterator)
    {
        auto sub_block = reader->ReadSubBlock(iterator->second);
        ASSERT_TRUE(sub_block);
        int z_coordinate;
        bool b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::Z, &z_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(z_coordinate, iterator->first);
        int c_coordinate;
        b = sub_block->GetSubBlockInfo().coordinate.TryGetPosition(DimensionIndex::C, &c_coordinate);
        EXPECT_TRUE(b);
        EXPECT_EQ(c_coordinate, 0);

        const auto stage_position = CziHelpers::GetStagePositionFromXmlMetadata(sub_block.get());
        if (z_coordinate % 2 == 0)
        {
            EXPECT_DOUBLE_EQ(get<0>(stage_position), 1.5 * z_coordinate);
            EXPECT_DOUBLE_EQ(get<1>(stage_position), -2.5 * z_coordinate);
        }
        else
        {
            EXPECT_TRUE(isnan(get<0>(stage_position)));
            EXPECT_TRUE(isnan(get<1>(stage_position)));
        }
    }
}
