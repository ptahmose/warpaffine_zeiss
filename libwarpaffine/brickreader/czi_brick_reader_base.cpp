// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "czi_brick_reader_base.h"
#include <limits>

using namespace std;

void CziBrickReaderBase::FillOutInformationFromSubBlockMetadata(const libCZI::ISubBlock* sub_block, BrickCoordinateInfo* brick_coordinate_info)
{
    const auto stage_position = this->GetStagePositionFromSubBlockMetadata(sub_block);
    brick_coordinate_info->stage_x_position = get<0>(stage_position);
    brick_coordinate_info->stage_y_position = get<1>(stage_position);
}

tuple<double, double> CziBrickReaderBase::GetStagePositionFromSubBlockMetadata(const libCZI::ISubBlock* sub_block)
{
    if (this->GetContextBase().GetCommandLineOptions().GetWriteStagePositionsInSubblockMetadata())
    {
        return CziHelpers::GetStagePositionFromXmlMetadata(sub_block);
    }

    return make_tuple(numeric_limits<double>::quiet_NaN(), numeric_limits<double>::quiet_NaN());
}
