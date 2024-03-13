// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "ITaskArena.h"
#include "taskarena_tbb.h"

using namespace std;

std::shared_ptr<ITaskArena> CreateTaskArenaTbb(AppContext& context)
{
    return make_shared<TaskArenaTbb>(context);
}


