// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "../czi_helpers.h"
#include "../inc_libCZI.h"
#include "IBrickReader.h"
#include "../appcontext.h"
#include <map>
#include <memory>

class CziBrickReaderBase
{
private:
    AppContext& context_;
    libCZI::SubBlockStatistics statistics_;
    std::map<int, libCZI::PixelType> map_channelno_to_pixeltype_;
    std::shared_ptr<libCZI::ICZIReader> underlying_reader_;
public:
    CziBrickReaderBase() = delete;

    CziBrickReaderBase(AppContext& context, const std::shared_ptr<libCZI::ICZIReader>& reader)
        : context_(context)
    {
        this->statistics_ = reader->GetStatistics();
        this->map_channelno_to_pixeltype_ = CziHelpers::GetMapOfChannelsToPixeltype(reader.get());
        this->underlying_reader_ = reader;
    }

    std::shared_ptr<libCZI::ICZIReader>& GetUnderlyingReaderBase()
    {
        return this->underlying_reader_;
    }

    AppContext& GetContextBase()
    {
        return this->context_;
    }

    const libCZI::SubBlockStatistics& GetStatistics()
    {
        return this->statistics_;
    }

    libCZI::PixelType GetPixelTypeForChannelNo(int c)
    {
        return this->map_channelno_to_pixeltype_[c];
    }

    static void FillOutInformationFromSubBlockMetadata(const libCZI::ISubBlock* sub_block, BrickCoordinateInfo* brick_coordinate_info);
};
