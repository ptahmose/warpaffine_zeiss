// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <LibWarpAffine_Config.h>
#include <string>
#include <memory>
#include <mutex>

#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    #include <Windows.h>
#endif

enum class ConsoleColor : unsigned char
{
    BLACK = 0,
    DARK_BLUE,
    DARK_GREEN,
    DARK_CYAN,
    DARK_RED,
    DARK_MAGENTA,
    DARK_YELLOW,
    DARK_WHITE,
    LIGHT_BLACK,
    LIGHT_BLUE,
    LIGHT_GREEN,
    LIGHT_CYAN,
    LIGHT_RED,
    LIGHT_MAGENTA,
    LIGHT_YELLOW,
    WHITE,
    DEFAULT
};

struct ConsoleCursorPosition
{
    int x;
    int y;
};

class ILog
{
public:
    /// Query if StdOut is a terminal, meaning that we can move the cursor and use colors.
    ///
    /// \returns    True if stdout a terminal, false if not.
    virtual bool IsStdOutATerminal() const = 0;

    virtual void SetColor(ConsoleColor foreground, ConsoleColor background) = 0;

    virtual void MoveUp(int lines_to_move_up) = 0;

    virtual void WriteLineStdOut(const char* sz) = 0;
    virtual void WriteLineStdOut(const wchar_t* sz) = 0;
    virtual void WriteLineStdErr(const char* sz) = 0;
    virtual void WriteLineStdErr(const wchar_t* sz) = 0;

    virtual void WriteStdOut(const char* sz) = 0;
    virtual void WriteStdOut(const wchar_t* sz) = 0;
    virtual void WriteStdErr(const char* sz) = 0;
    virtual void WriteStdErr(const wchar_t* sz) = 0;

    void WriteLineStdOut(const std::string& str)
    {
        this->WriteLineStdOut(str.c_str());
    }

    void WriteLineStdOut(const std::wstring& str)
    {
        this->WriteLineStdOut(str.c_str());
    }

    void WriteLineStdErr(const std::string& str)
    {
        this->WriteLineStdErr(str.c_str());
    }

    void WriteLineStdErr(const std::wstring& str)
    {
        this->WriteLineStdErr(str.c_str());
    }

    void WriteStdOut(const std::string& str)
    {
        this->WriteStdOut(str.c_str());
    }

    void WriteStdOut(const std::wstring& str)
    {
        this->WriteStdOut(str.c_str());
    }

    void WriteStdErr(const std::string& str)
    {
        this->WriteStdErr(str.c_str());
    }

    void WriteStdErr(const std::wstring& str)
    {
        this->WriteStdErr(str.c_str());
    }

    virtual ~ILog() = default;

    // non-copyable and non-moveable
    ILog() = default;
    ILog(const ILog&) = default;             // copy constructor
    ILog& operator=(const ILog&) = default;  // copy assignment
    ILog(ILog&&) = default;                  // move constructor
    ILog& operator=(ILog&&) = default;       // move assignment
};

class CConsoleLog : public ILog
{
private:
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    HANDLE consoleHandle;
    std::uint16_t defaultConsoleColor;
    bool canUseVirtualTerminalSequences;
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    bool isTerminalOutput;
#endif
    std::mutex io_stream_access_mutex_;
public:
    static std::shared_ptr<ILog> CreateInstance();

    CConsoleLog();

    bool IsStdOutATerminal() const override;

    void SetColor(ConsoleColor foreground, ConsoleColor background) override;

    void MoveUp(int lines_to_move_up) override;

    void WriteLineStdOut(const char* sz) override;
    void WriteLineStdOut(const wchar_t* sz) override;
    void WriteLineStdErr(const char* sz) override;
    void WriteLineStdErr(const wchar_t* sz) override;

    void WriteStdOut(const char* sz) override;
    void WriteStdOut(const wchar_t* sz) override;
    void WriteStdErr(const char* sz) override;
    void WriteStdErr(const wchar_t* sz) override;
private:
#if LIBWARPAFFINE_WIN32_ENVIRONMENT
    std::uint16_t  GetColorAttribute(ConsoleColor foreground, ConsoleColor background);
#endif
#if LIBWARPAFFINE_UNIX_ENVIRONMENT
    void SetTextColorAnsi(ConsoleColor foreground, ConsoleColor background);
#endif
};
