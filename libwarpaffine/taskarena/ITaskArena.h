// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <LibWarpAffine_Config.h>

#include <cstdint>
#include <functional>
#include <memory>

class AppContext;

/// Statistics about the state of the task arena.
struct TaskArenaStatistics
{
    std::uint32_t queue_length{ 0 };     ///< How many items are currently in the task-arena's queue.
    std::uint32_t active_tasks{ 0 };     ///< How many tasks are currently executing (this number includes the suspended tasks).
    std::uint32_t suspended_tasks{ 0 };  ///< How many tasks are currently suspended.
};

enum class TaskType
{
    DecompressSlice,
    Compression,
    BrickComposition,
    WarpAffineBrick,
    CompressSlice,
};

/// The "task arena" interface models a simplistic thread-pool. The only supported operation is adding a task, 
/// besides there is just some statistics which can be retrieved. There is no concept of e.g. cancellation.
class ITaskArena
{
public:
    /// Defines an alias representing a "suspend handle".
    typedef void* SuspendHandle;

    /// Adds a task to the task arena. The specified functor will execute at an unspecified point in time, and it
    /// will execute in an arbitrary thread-context, potentially concurrently with other tasks.
    ///
    /// \param  task_type An enum specifying at "type of the task". 
    /// \param  task      The task.
    virtual void AddTask(TaskType task_type, const std::function<void()>& task) = 0;

    /// This allows to pause (or suspend) a task. This method **must** be called from within a task,
    /// and the way this works is:
    /// - A functor is to be provided, which will be called passing in a "suspend handle". It will be called while executing this method  
    ///    (and it will be the last operation which takes place in this task's execution).
    /// - This "suspend handle" is an opaque object to the client. The functor needs to store this handle (for later use in order to resume the task). 
    /// - In order to resume the task, the 'ResumeTask' needs to be called at some later point in time. Obviously,  
    ///    this call needs to be done from some other execution thread (other than the task which requested to be suspended).
    /// Note that the task when it is resumed may execute on a different thread than before.
    /// This is resembling the corresponding TBB-functionality -> -> https://spec.oneapi.io/versions/latest/elements/oneTBB/source/task_scheduler/scheduling_controls/resumable_tasks.html.
    ///
    /// \param  func_pass_resume_handle Functor which will be called passing in a resume handle.
    virtual void SuspendCurrentTask(const std::function<void(SuspendHandle)>& func_pass_resume_handle) = 0;

    /// Resume a suspended task.
    ///
    /// \param  resume_handle A handle representing the task to be resumed.
    virtual void ResumeTask(SuspendHandle resume_handle) = 0;

    /// Gets statistics about the current state of the task arena.
    ///
    /// \returns The statistics.
    virtual TaskArenaStatistics GetStatistics() = 0;
    virtual ~ITaskArena() = default;

    // non-copyable and non-moveable
    ITaskArena() = default;
    ITaskArena(const ITaskArena&) = default;             // copy constructor
    ITaskArena& operator=(const ITaskArena&) = default;  // copy assignment
    ITaskArena(ITaskArena&&) = default;                  // move constructor
    ITaskArena& operator=(ITaskArena&&) = default;       // move assignment
};

std::shared_ptr<ITaskArena> CreateTaskArenaTbb(AppContext& context);

