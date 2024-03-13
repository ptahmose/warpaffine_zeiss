// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "IBrickReader.h"
#include "czi_brick_reader.h"
#include "czi_brick_reader2.h"
#include "czi_linear_brick_reader.h"

using namespace std;
using namespace libCZI;

std::shared_ptr<ICziBrickReader> CreateBrickReaderPlaneReader(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream)
{
    return make_shared<CziBrickReader>(context, reader, stream);
}

std::shared_ptr<ICziBrickReader> CreateBrickReaderLinearReading(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream)
{
    return make_shared<CziBrickReaderLinearReading>(context, reader, stream);
}

std::shared_ptr<ICziBrickReader> CreateBrickReaderPlaneReader2(AppContext& context, std::shared_ptr<libCZI::ICZIReader> reader, std::shared_ptr<IStreamEx> stream)
{
    return make_shared<CziBrickReader2>(context, reader, stream);
}

/*static*/const char* ICziBrickReader::kPropertyBagKey_LinearReader_max_number_of_subblocks_to_wait_for = "max_number_of_subblocks_to_wait_for";
