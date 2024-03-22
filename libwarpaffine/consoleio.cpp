// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include <cstdarg>
#include "consoleio.h"
#include <iostream>
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
#include <io.h>
#define NOMINMAX
#include <Windows.h>
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
#include <unistd.h>
#endif

using namespace std;

/*static*/std::shared_ptr<ILog> CConsoleLog::CreateInstance()
{
    return std::make_shared<CConsoleLog>();
}

CConsoleLog::CConsoleLog()
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    : consoleHandle(INVALID_HANDLE_VALUE),
        canUseVirtualTerminalSequences(false)
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    : isTerminalOutput(false)
#endif
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    int stdoutFile = _fileno(stdout);

    // c.f. https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/get-osfhandle?view=msvc-170&viewFallbackFrom=vs-2017 , -2 is a magic value indicating
    // no association with a stream
    if (stdoutFile != -2)
    {
        intptr_t osfhandle = _get_osfhandle(stdoutFile);
        if ((HANDLE)osfhandle != INVALID_HANDLE_VALUE)
        {
            DWORD dw = GetFileType((HANDLE)osfhandle);
            if (dw == FILE_TYPE_CHAR)
            {
                CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
                GetConsoleScreenBufferInfo((HANDLE)osfhandle, &screenBufferInfo);
                this->defaultConsoleColor = screenBufferInfo.wAttributes;
                this->consoleHandle = (HANDLE)osfhandle;
                DWORD mode;
                BOOL B = GetConsoleMode(this->consoleHandle, &mode);
                if (B && (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
                {
                    this->canUseVirtualTerminalSequences = true;
                }
            }
        }
    }
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    this->isTerminalOutput = isatty(fileno(stdout)) == 1;
#endif
}

bool CConsoleLog::IsStdOutATerminal() const
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    return this->consoleHandle != INVALID_HANDLE_VALUE;
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    return this->isTerminalOutput;
#endif
}

void CConsoleLog::SetColor(ConsoleColor foreground, ConsoleColor background)
{
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    if (this->consoleHandle != INVALID_HANDLE_VALUE)
    {
        SetConsoleTextAttribute(this->consoleHandle, this->GetColorAttribute(foreground, background));
    }
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    if (this->isTerminalOutput)
    {
        this->SetTextColorAnsi(foreground, background);
    }
#endif
}

void CConsoleLog::WriteLineStdOut(const char* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::cout << sz << endl;
}

void CConsoleLog::WriteLineStdOut(const wchar_t* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::wcout << sz << endl;
}

void CConsoleLog::WriteLineStdErr(const char* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::cout << sz << endl;
}

void CConsoleLog::WriteLineStdErr(const wchar_t* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::wcout << sz << endl;
}

void CConsoleLog::WriteStdOut(const char* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::cout << sz;
}

void CConsoleLog::WriteStdOut(const wchar_t* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::wcout << sz;
}

void CConsoleLog::WriteStdErr(const char* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::cout << sz;
}

void CConsoleLog::WriteStdErr(const wchar_t* sz)
{
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::wcout << sz;
}

void CConsoleLog::MoveUp(int lines_to_move_up)
{
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    cout << "\033[" << lines_to_move_up << "A";
#endif
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    if (this->canUseVirtualTerminalSequences)
    {
        cout << "\033[" << lines_to_move_up << "A";
    }
    else
    {
        CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
        BOOL B = GetConsoleScreenBufferInfo(
            this->consoleHandle,
            &consoleScreenBufferInfo);
        COORD coord = consoleScreenBufferInfo.dwCursorPosition;
        coord.Y -= lines_to_move_up;
        SetConsoleCursorPosition(
            this->consoleHandle,
            coord);
    }
#endif
}

#if LIBWARPAFFINE_WIN32_ENVIRONMENT
std::uint16_t  CConsoleLog::GetColorAttribute(ConsoleColor foreground, ConsoleColor background)
{
    std::uint16_t attribute;
    switch (foreground)
    {
    case ConsoleColor::BLACK:
        attribute = 0;
        break;
    case ConsoleColor::DARK_BLUE:
        attribute = FOREGROUND_BLUE;
        break;
    case ConsoleColor::DARK_GREEN:
        attribute = FOREGROUND_GREEN;
        break;
    case ConsoleColor::DARK_CYAN:
        attribute = FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case ConsoleColor::DARK_RED:
        attribute = FOREGROUND_RED;
        break;
    case ConsoleColor::DARK_MAGENTA:
        attribute = FOREGROUND_RED | FOREGROUND_BLUE;
        break;
    case ConsoleColor::DARK_YELLOW:
        attribute = FOREGROUND_RED | FOREGROUND_GREEN;
        break;
    case ConsoleColor::DARK_WHITE:
        attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_BLACK:
        attribute = FOREGROUND_INTENSITY;
        break;
    case ConsoleColor::LIGHT_BLUE:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_GREEN:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
        break;
    case ConsoleColor::LIGHT_CYAN:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_RED:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_RED;
        break;
    case ConsoleColor::LIGHT_MAGENTA:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_YELLOW:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
        break;
    case ConsoleColor::WHITE:
        attribute = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case ConsoleColor::DEFAULT:
        attribute = this->defaultConsoleColor & (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        break;
    }

    switch (background)
    {
    case ConsoleColor::BLACK:
        attribute |= 0;
        break;
    case 	ConsoleColor::DARK_BLUE:
        attribute |= BACKGROUND_BLUE;
        break;
    case 	ConsoleColor::DARK_GREEN:
        attribute |= BACKGROUND_GREEN;
        break;
    case ConsoleColor::DARK_CYAN:
        attribute |= BACKGROUND_GREEN | BACKGROUND_BLUE;
        break;
    case ConsoleColor::DARK_RED:
        attribute |= BACKGROUND_RED;
        break;
    case ConsoleColor::DARK_MAGENTA:
        attribute |= BACKGROUND_RED | BACKGROUND_BLUE;
        break;
    case ConsoleColor::DARK_YELLOW:
        attribute |= BACKGROUND_RED | BACKGROUND_GREEN;
        break;
    case ConsoleColor::DARK_WHITE:
        attribute |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_BLACK:
        attribute |= BACKGROUND_INTENSITY;
        break;
    case ConsoleColor::LIGHT_BLUE:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_GREEN:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_GREEN;
        break;
    case ConsoleColor::LIGHT_CYAN:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_RED:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_RED;
        break;
    case ConsoleColor::LIGHT_MAGENTA:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE;
        break;
    case ConsoleColor::LIGHT_YELLOW:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN;
        break;
    case ConsoleColor::WHITE:
        attribute |= BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        break;
    case ConsoleColor::DEFAULT:
        attribute |= (this->defaultConsoleColor & (BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE));
        break;
    }

    return attribute;
}
#endif

#if LIBWARPAFFINE_UNIX_ENVIRONMENT
void CConsoleLog::SetTextColorAnsi(ConsoleColor foreground, ConsoleColor background)
{
    const char* ansiForeground;
    const char* ansiBackground;
    switch (foreground)
    {
    case ConsoleColor::BLACK:
        ansiForeground = "\033[30m";
        break;
    case ConsoleColor::DARK_BLUE:
        ansiForeground = "\033[34m";
        break;
    case ConsoleColor::DARK_GREEN:
        ansiForeground = "\033[32m";
        break;
    case ConsoleColor::DARK_CYAN:
        ansiForeground = "\033[36m";
        break;
    case ConsoleColor::DARK_RED:
        ansiForeground = "\033[31m";
        break;
    case ConsoleColor::DARK_MAGENTA:
        ansiForeground = "\033[35m";
        break;
    case ConsoleColor::DARK_YELLOW:
        ansiForeground = "\033[33m";
        break;
    case ConsoleColor::DARK_WHITE:
        ansiForeground = "\033[37m";
        break;
    case ConsoleColor::LIGHT_BLACK:
        ansiForeground = "\033[90m";
        break;
    case ConsoleColor::LIGHT_BLUE:
        ansiForeground = "\033[94m";
        break;
    case ConsoleColor::LIGHT_GREEN:
        ansiForeground = "\033[92m";
        break;
    case ConsoleColor::LIGHT_CYAN:
        ansiForeground = "\033[96m";
        break;
    case ConsoleColor::LIGHT_RED:
        ansiForeground = "\033[91m";
        break;
    case ConsoleColor::LIGHT_MAGENTA:
        ansiForeground = "\033[95m";
        break;
    case ConsoleColor::LIGHT_YELLOW:
        ansiForeground = "\033[93m";
        break;
    case ConsoleColor::WHITE:
        ansiForeground = "\033[97m";
        break;
    case ConsoleColor::DEFAULT:
        ansiForeground = "\033[39m";
        break;
    default:
        ansiForeground = "";
        break;
    }

    switch (background)
    {
    case ConsoleColor::BLACK:
        ansiBackground = "\033[40m";
        break;
    case ConsoleColor::DARK_BLUE:
        ansiBackground = "\033[44m";
        break;
    case ConsoleColor::DARK_GREEN:
        ansiBackground = "\033[42m";
        break;
    case ConsoleColor::DARK_CYAN:
        ansiBackground = "\033[46m";
        break;
    case ConsoleColor::DARK_RED:
        ansiBackground = "\033[41m";
        break;
    case ConsoleColor::DARK_MAGENTA:
        ansiBackground = "\033[45m";
        break;
    case ConsoleColor::DARK_YELLOW:
        ansiBackground = "\033[43m";
        break;
    case ConsoleColor::DARK_WHITE:
        ansiBackground = "\033[47m";
        break;
    case ConsoleColor::LIGHT_BLACK:
        ansiBackground = "\033[100m";
        break;
    case ConsoleColor::LIGHT_BLUE:
        ansiBackground = "\033[104m";
        break;
    case ConsoleColor::LIGHT_GREEN:
        ansiBackground = "\033[102m";
        break;
    case ConsoleColor::LIGHT_CYAN:
        ansiBackground = "\033[106m";
        break;
    case ConsoleColor::LIGHT_RED:
        ansiBackground = "\033[101m";
        break;
    case ConsoleColor::LIGHT_MAGENTA:
        ansiBackground = "\033[105m";
        break;
    case ConsoleColor::LIGHT_YELLOW:
        ansiBackground = "\033[103m";
        break;
    case ConsoleColor::WHITE:
        ansiBackground = "\033[107m";
        break;
    case ConsoleColor::DEFAULT:
        ansiBackground = "\033[49m";
        break;
    default:
        ansiBackground = "";
        break;
    }

    lock_guard<std::mutex> lock(this->io_stream_access_mutex_);
    std::cout << ansiForeground << ansiBackground;
}
#endif
