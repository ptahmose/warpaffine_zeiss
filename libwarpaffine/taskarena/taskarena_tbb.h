// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <tbb/task_arena.h>
#include "ITaskArena.h"

/// Implementation of a "task arena" based on Intel TBB.
class TaskArenaTbb : public ITaskArena
{
private:
    AppContext& context_;
    std::atomic_uint32_t queue_length{0};
    std::atomic_uint32_t active_tasks{0};
    std::atomic_uint32_t suspended_tasks{ 0 };
    tbb::task_arena arena;
public:
    explicit TaskArenaTbb(AppContext& context);
    void AddTask(TaskType task_type, const std::function<void()>& task) override;

    void SuspendCurrentTask(const std::function<void(SuspendHandle)>& func_pass_resume_handle) override;
    void ResumeTask(SuspendHandle resume_handle) override;

    TaskArenaStatistics GetStatistics() override;
    ~TaskArenaTbb() override;
};
