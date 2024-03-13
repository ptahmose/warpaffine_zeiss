// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <LibWarpAffine_Config.h>
#include <memory>
#include "IWarpAffine.h"
#include "WarpAffine_IPP.h"
#include "WarpAffineNull.h"
#include "WarpAffine_Reference.h"

using namespace std;

std::shared_ptr<IWarpAffine> CreateWarpAffine(WarpAffineImplementation implementation)
{
    switch (implementation)
    {
    case WarpAffineImplementation::kIPP:
#if LIBWARPAFFINE_INTELPERFORMANCEPRIMITIVES_AVAILABLE
        return std::make_shared<WarpAffineIPP>();
#else
        return {};
#endif
    case WarpAffineImplementation::kNull:
        return std::make_shared<WarpAffineNull>();
    case WarpAffineImplementation::kReference:
        return std::make_shared<WarpAffine_Reference>();
    }

    throw invalid_argument("unknown 'implementation' given.");
}
