// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "ISlicesWriter.h"
#include "NullSlicesWriter.h"
#include "SlicesWriterTbb.h"

using namespace std;

std::shared_ptr<ICziSlicesWriter> CreateNullSlicesWriter()
{
    return make_shared<NullSlicesWriter>();
}

std::shared_ptr<ICziSlicesWriter> CreateSlicesWriterTbb(AppContext& context, const std::wstring& filename)
{
    return make_shared<CziSlicesWriterTbb>(context, filename);
}
