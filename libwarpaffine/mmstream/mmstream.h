// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <LibWarpAffine_Config.h>

#if LIBWARPAFFINE_WIN32_ENVIRONMENT

#include "IStreamEx.h"
#include <atomic>
#include <memory>
#define NOMINMAX
#include <Windows.h>

/// An implementation of a "libCZI-stream-object" using memory-mapped file. I.e.
/// the whole is mapped into memory, and we simply to a memcpy when "reading" from
/// the file.
/// This is experimental code, the hope is that this might be faster than a 
/// "ReadFile"-based implementation.
class MemoryMappedStream : public IStreamEx
{
private:
    HANDLE handleFile;
    HANDLE handleFileMapping;
    void* mappedMemory;
    std::atomic_uint64_t total_bytes_read_{ 0 };
public:
    explicit MemoryMappedStream(const wchar_t* filename);
    void Read(std::uint64_t offset, void* pv, std::uint64_t size, std::uint64_t* ptrBytesRead) override;
    std::uint64_t GetTotalBytesRead() override;
    ~MemoryMappedStream() override;
};

std::shared_ptr<IStreamEx> CreateMemoryMappedStreamSp(const wchar_t* filename);

#endif
