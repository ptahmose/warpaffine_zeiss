// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "czi_brick_reader_base.h"

using namespace std;

/*static */void CziBrickReaderBase::FillOutInformationFromSubBlockMetadata(const libCZI::ISubBlock* sub_block, BrickCoordinateInfo* brick_coordinate_info)
{
    const auto stage_position = CziHelpers::GetStagePositionFromXmlMetadata(sub_block);
    brick_coordinate_info->stage_x_position = get<0>(stage_position);
    brick_coordinate_info->stage_y_position = get<1>(stage_position);
}
