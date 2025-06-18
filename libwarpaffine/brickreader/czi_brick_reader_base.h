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
#include <tuple>

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

    /// Try to get the stage position from sub block metadata. This method
    /// will return the stage position if it is available in the sub block metadata and
    /// if this option is enabled in the application context. If the stage position is not available
    /// or the option is disabled, it will return (NaN, NaN) as a default value.
    ///
    /// \param  sub_block   The sub block.
    ///
    /// \returns    The stage position from sub block metadata if available and enabled;  (NaN, NaN) otherwise.
    std::tuple<double, double> GetStagePositionFromSubBlockMetadata(const libCZI::ISubBlock* sub_block);

    /// Fill out information about the stage-position in the BrickCoordinateInfo structure. This method
    /// will either fill out the respective fields in the BrickCoordinateInfo structure with the actual
    /// stage position or set the fields to NaN if the stage position is not available.
    ///
    /// \param          sub_block               The sub block.
    /// \param [out]    brick_coordinate_info   The information structure to fill out the stage position fields.
    void FillOutInformationFromSubBlockMetadata(const libCZI::ISubBlock* sub_block, BrickCoordinateInfo* brick_coordinate_info);
};
