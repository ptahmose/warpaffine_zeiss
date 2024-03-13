// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "mmstream.h"
#include <memory>

#if LIBWARPAFFINE_WIN32_ENVIRONMENT

MemoryMappedStream::MemoryMappedStream(const wchar_t* filename)
{
    this->handleFile = CreateFileW(filename,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    this->handleFileMapping = CreateFileMapping(
        this->handleFile,           // current file handle
        NULL,                       // default security
        PAGE_READONLY,              // read/write permission
        0,                          // size of mapping object, high
        0,                          // size of mapping object, low
        NULL);                      // name of mapping object

    this->mappedMemory = MapViewOfFile(
        this->handleFileMapping,
        FILE_MAP_READ,
        0,
        0,
        0);
}

MemoryMappedStream::~MemoryMappedStream()
{
    UnmapViewOfFile(this->mappedMemory);
    CloseHandle(this->handleFileMapping);
    CloseHandle(this->handleFile);
}

void MemoryMappedStream::Read(std::uint64_t offset, void* pv, std::uint64_t size, std::uint64_t* ptrBytesRead)
{
    memcpy(
        pv,
        static_cast<const char*>(this->mappedMemory) + offset,
        size);
    if (ptrBytesRead != nullptr)
    {
        *ptrBytesRead = size;
    }

    this->total_bytes_read_.fetch_add(size);
}

std::uint64_t MemoryMappedStream::GetTotalBytesRead()
{
    return this->total_bytes_read_.load();
}

std::shared_ptr<IStreamEx> CreateMemoryMappedStreamSp(const wchar_t* filename)
{
    return std::make_shared<MemoryMappedStream>(filename);
}

#endif
