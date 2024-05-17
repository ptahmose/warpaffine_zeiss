// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "taskarena_tbb.h"
#include <sstream>
#include <tbb/task.h>
#include "../appcontext.h"

using namespace std;

TaskArenaTbb::TaskArenaTbb(AppContext& context)
    : context_(context)
{
    this->arena.initialize();
}

void TaskArenaTbb::AddTask(TaskType task_type, const std::function<void()>& task)
{
    ++this->queue_length;
    this->arena.enqueue(
        [this, task]() ->void
        {
            --this->queue_length;
            ++this->active_tasks;
            try
            {
                task();
            }
            catch (exception& exception)
            {
                ostringstream text;
                text << "Task crashed: " << exception.what() << ".";
                this->context_.FatalError(text.str());
            }

            --this->active_tasks;
        });
}

void TaskArenaTbb::SuspendCurrentTask(const std::function<void(SuspendHandle)>& func_pass_resume_handle)
{
    ++this->suspended_tasks;

    tbb::task::suspend(
        [&](tbb::task::suspend_point tag) 
        {
            // Dedicated user-managed activity that processes async requests.
            func_pass_resume_handle(tag); // could be OpenCL/IO/Database/Network etc.
        }); // execution will be resumed after this function
}

void TaskArenaTbb::ResumeTask(SuspendHandle resume_handle)
{
    // Signal to resume execution of the task referenced by the tbb::task::suspend_point
    // from a dedicated user-managed activity
    tbb::task::resume(static_cast<tbb::task::suspend_point>(resume_handle));
    --this->suspended_tasks;
}

TaskArenaStatistics TaskArenaTbb::GetStatistics()
{
    TaskArenaStatistics statistics;
    statistics.queue_length = this->queue_length.load();
    statistics.active_tasks = this->active_tasks.load();
    statistics.suspended_tasks = this->suspended_tasks.load();
    return statistics;
}
