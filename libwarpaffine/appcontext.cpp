// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "appcontext.h"
#include <exception>
#include <memory>
#include <string>
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
#include <Windows.h>
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
#include <cstdio>
#endif

using namespace std;

AppContext::AppContext()
    : allocator_(*this)
{
    this->log_ = CConsoleLog::CreateInstance();
}

bool AppContext::Initialize(int argc, char** argv)
{
    const auto parse_result = this->commandline_options_.Parse(argc, argv);
    switch (parse_result)
    {
        case CCmdLineOptions::ParseResult::Error:
            this->log_->WriteLineStdErr("There was an error parsing the command line -> exiting");
            throw runtime_error("error parsing command line");
        case CCmdLineOptions::ParseResult::Exit:
            return false;
        case CCmdLineOptions::ParseResult::OK:
            break;
    }

    switch (this->commandline_options_.GetTaskArenaImplementation())
    {
        case TaskArenaImplementation::kTBB:
            this->task_arena_ = CreateTaskArenaTbb(*this);
            break;
        default:
            this->log_->WriteLineStdErr("Unknown task-arena implementation encountered -> exiting");
            throw runtime_error("unknown task-arena implementation");
    }

    // let's use the "US-UTF8" locale if it is available, otherwise we just go with the default one
    try
    {
        this->formatting_locale_ = locale("en_US.UTF-8");
    }
    catch (runtime_error&)
    {
    }

    return true;
}

ILog* AppContext::GetLog()
{
    return this->log_.get();
}

const std::shared_ptr<ITaskArena>& AppContext::GetTaskArena()
{
    return this->task_arena_;
}

const CCmdLineOptions& AppContext::GetCommandLineOptions() const
{
    return this->commandline_options_;
}

BrickAllocator& AppContext::GetAllocator()
{
    return this->allocator_;
}

void AppContext::FatalError(const std::string& message)
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    this->WriteDebugString(message.c_str());
#endif
    this->log_->WriteLineStdErr(message);

    // note: the exit-code of the application will be 134 in this case
    abort();
}

void AppContext::WriteDebugString(const char* message)
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    OutputDebugStringA(message);
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    // TODO(JBL): find an equivalent for Linux, maybe something like - pass a filename on the command line and write to that file
    //fputs(message, stderr);
#endif
}

const std::locale& AppContext::GetFormattingLocale() const
{
    return this->formatting_locale_;
}

void AppContext::DoIfVerbosityGreaterOrEqual(MessagesPrintVerbosity verbosity, const std::function<void(ILog*)>& action)
{
    if (this->commandline_options_.GetPrintOutVerbosity() >= verbosity)
    {
        if (this->log_ != nullptr)
        {
            action(this->log_.get());
        }
    }
}
