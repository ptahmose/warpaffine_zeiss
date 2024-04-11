// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "cmdlineoptions.h"
#include "consoleio.h"
#include "taskarena/ITaskArena.h"
#include "BrickAllocator.h"
#include <locale>
#include <memory>
#include <string>
#include <functional>

/// This class gathers "application global objects and services".
class AppContext
{
private:
    CCmdLineOptions commandline_options_;
    std::shared_ptr<ILog> log_;
    std::shared_ptr<ITaskArena> task_arena_;
    BrickAllocator allocator_;
    std::locale formatting_locale_;
public:
    AppContext();

    /// Initializes this object and parses the command line. If the command line is invalid, this
    /// method will throw an exception. If this is the case, all error-reporting is already done
    /// and the application should exit immediately. If there is nothing to do anymore (e.g. if the
    /// application was invoked with the '--help' argument), this method will return false. In this case,
    /// the application should exit immediately, but not in error state.
    ///
    /// \param      argc    The number of command line arguments.
    /// \param [in] argv    The array of command line arguments (in UTF-8 encoding).
    ///
    /// \returns    True if operation should continue; if false the application should terminate immediately (but not in error state).
    bool Initialize(int argc, char** argv);

    ILog* GetLog();
    const std::shared_ptr<ITaskArena>& GetTaskArena();
    const CCmdLineOptions& GetCommandLineOptions() const;
    BrickAllocator& GetAllocator();
    void FatalError(const std::string& message);
    void WriteDebugString(const char* message);
    const std::locale& GetFormattingLocale() const;

    /// The specified functor 'action' is called if the verbosity (given on the command-line) is greater or equal to
    /// the one specified.
    ///
    /// \param  verbosity The verbosity.
    /// \param  action    The action.
    void DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity verbosity, const std::function<void(ILog*)>& action);
};
