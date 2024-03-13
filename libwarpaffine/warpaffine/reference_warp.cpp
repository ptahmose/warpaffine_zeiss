// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "reference_warp.h"

using namespace std;
using namespace Eigen;
using namespace libCZI;

void ReferenceWarp::SetTransformation(const Eigen::Matrix4d& transformation)
{
    this->transformation_ = transformation;
    this->transformation_inverse_ = transformation.inverse();
}

void ReferenceWarp::SetInterpolation(Interpolation interpolation)
{
    if (interpolation != Interpolation::kNearestNeighbor && interpolation != Interpolation::kBilinear)
    {
        throw invalid_argument("Only nearest-neighbor and linear interpolation are supported.");
    }

    this->interpolation_ = interpolation;
}

void ReferenceWarp::Do()
{
    switch (this->interpolation_)
    {
    case Interpolation::kNearestNeighbor:
        this->DoNearestNeighbor();
        break;
    case Interpolation::kBilinear:
        this->DoLinearInterpolation();
        break;
    default:
        throw invalid_argument("An invalid/unsupported interpolation was requested.");
    }
}

void ReferenceWarp::DoNearestNeighbor()
{
    switch (this->source_brick_.info.pixelType)
    {
    case PixelType::Gray16:
        ReferenceWarp::NearestNeighborWarp<uint16_t>(this->source_brick_, this->destination_brick_, this->transformation_inverse_);
        break;
    case PixelType::Gray8:
        ReferenceWarp::NearestNeighborWarp<uint8_t>(this->source_brick_, this->destination_brick_, this->transformation_inverse_);
        break;
    case PixelType::Gray32Float:
        ReferenceWarp::NearestNeighborWarp<float>(this->source_brick_, this->destination_brick_, this->transformation_inverse_);
        break;
    default:
        this->ThrowUnsupportedPixelType();
    }
}

void ReferenceWarp::DoLinearInterpolation()
{
    switch (this->source_brick_.info.pixelType)
    {
    case PixelType::Gray16:
        TriLinearWarp<uint16_t>(this->source_brick_, this->destination_brick_, this->transformation_inverse_);
        break;
    case PixelType::Gray8:
        TriLinearWarp<uint8_t>(this->source_brick_, this->destination_brick_, this->transformation_inverse_);
        break;
    case PixelType::Gray32Float:
        TriLinearWarp<float>(this->source_brick_, this->destination_brick_, this->transformation_inverse_);
        break;
    default:
        this->ThrowUnsupportedPixelType();
    }
}

void ReferenceWarp::ThrowUnsupportedPixelType()
{
    ostringstream string_stream;
    string_stream << "An unsupported pixeltype (" << static_cast<int>(this->source_brick_.info.pixelType) << ", " << Utils::PixelTypeToInformalString(this->source_brick_.info.pixelType) << ") was encountered.";
    throw runtime_error(string_stream.str());
}

/*static*/IntPos3 ReferenceWarp::ToNearestNeighbor(const Eigen::Vector4d& position)
{
    return IntPos3{ static_cast<int>(lround(position[0])), static_cast<int>(lround(position[1])), static_cast<int>(lround(position[2])) };
}
