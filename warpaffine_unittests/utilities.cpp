// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "utilities.h"
#include <cstdint>
#include <memory>

#if WARPAFFINEUNITTESTS_WIN32_ENVIRONMENT
#include <Windows.h>
#endif

using namespace std;

Utilities::GuardedBlock Utilities::AllocateWithGuardPageBehind(size_t size, size_t size_of_guard_block)
{
#if WARPAFFINEUNITTESTS_WIN32_ENVIRONMENT
    // determine page-size
    SYSTEM_INFO sSysInfo;         // useful information about the system
    GetSystemInfo(&sSysInfo);     // initialize the structure
    const DWORD dwPageSize = sSysInfo.dwPageSize;

    // round up the "size of guard block" to a multiple of page-size
    const size_t size_of_guard_area = ((size_of_guard_block + dwPageSize - 1) / dwPageSize) * dwPageSize;

    // we need to round up "size" to multiple of pagesize, then add "size_of_guard_area" (which will
    //  become our guard-pages)
    const size_t sizeRoundedUp = ((size + dwPageSize - 1) / dwPageSize) * dwPageSize;

    // allocate one page more than requested - for this additional page we will next set the
    //  "NoAccess"-status
    const LPVOID buffer = VirtualAlloc(NULL, sizeRoundedUp + size_of_guard_area, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (buffer == NULL)
    {
        return GuardedBlock();
    }

    if (size_of_guard_area > 0)
    {
        // so, now we set the last pages (as indicated by "size_of_guard_area") to "no access" -> any read or write to this page will give an
        //  access-violation
        DWORD oldProtect;
        BOOL B = VirtualProtect(static_cast<uint8_t*>(buffer) + sizeRoundedUp, size_of_guard_area, PAGE_NOACCESS, &oldProtect);
        if (B == 0)
        {
            VirtualFree(buffer, 0, MEM_RELEASE);
            return GuardedBlock();
        }
    }

    // now we put together a pointer of the requested size, so that an access behind it hits the guard-page
    size_t offset = sizeRoundedUp - size;

    return GuardedBlock(buffer, static_cast<uint8_t*>(buffer) + offset);
#endif
#if WARPAFFINEUNITTESTS_UNIX_ENVIRONMENT
    // In a Unix-environment, I currently don't know how to do the trickery with guard pages, we
    //  therefore just do a "normal malloc-allocation".
    void* buffer = malloc(size);
    return GuardedBlock(buffer, buffer);
#endif
}

Utilities::GuardedBlock Utilities::AllocateWithGuardPageBefore(size_t size, size_t size_of_guard_block)
{
#if WARPAFFINEUNITTESTS_WIN32_ENVIRONMENT
    // determine page-size
    SYSTEM_INFO sSysInfo;         // useful information about the system
    GetSystemInfo(&sSysInfo);     // initialize the structure
    const DWORD dwPageSize = sSysInfo.dwPageSize;

    // round up the "size of guard block" to a multiple of page-size
    const size_t size_of_guard_area = ((size_of_guard_block + dwPageSize - 1) / dwPageSize) * dwPageSize;

    // we need to round up "size" to multiple of pagesize, then add "size_of_guard_area" (which will
    //  become our guard-pages)
    const size_t sizeRoundedUp = ((size + dwPageSize - 1) / dwPageSize) * dwPageSize;

    // allocate one page more than requested - for this additional page we will next set the
    //  "NoAccess"-status
    const LPVOID buffer = VirtualAlloc(NULL, sizeRoundedUp + size_of_guard_area, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (buffer == NULL)
    {
        return GuardedBlock();
    }

    if (size_of_guard_area > 0)
    {
        // so, now we set the last page to "no access" -> any read or write to those pages will give an
        //  access-violation
        DWORD oldProtect;
        BOOL B = VirtualProtect(buffer, size_of_guard_area, PAGE_NOACCESS, &oldProtect);
        if (B == 0)
        {
            VirtualFree(buffer, 0, MEM_RELEASE);
            return GuardedBlock();
        }
    }

    // now we put together a pointer of the requested size, so that access before it hits the guard-page
    return GuardedBlock(buffer, static_cast<uint8_t*>(buffer) + size_of_guard_area);
#endif
#if WARPAFFINEUNITTESTS_UNIX_ENVIRONMENT
    // In a Unix-environment, I currently don't know how to do the trickery with guard pages, we
    //  therefore just do a "normal malloc-allocation".
    void* buffer = malloc(size);
    return GuardedBlock(buffer, buffer);
#endif
}

/// Free the "guarded memory block".
/// \param [in] block The guarded memory block.
void Utilities::FreeGuardedBlock(const GuardedBlock& block)
{
#if WARPAFFINEUNITTESTS_WIN32_ENVIRONMENT
    VirtualFree(block.baseAddress, 0, MEM_RELEASE);
#endif
#if WARPAFFINEUNITTESTS_UNIX_ENVIRONMENT
    free(block.baseAddress);
#endif
}

/*static*/Brick Utilities::CreateBrickWithGuardPageBefore(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, size_t size_of_guard_area/* = 1*/)
{
    return CreateBrickWithGuardPage(pixel_type, width, height, depth, size_of_guard_area, Utilities::AllocateWithGuardPageBefore);
}

/*static*/Brick Utilities::CreateBrickWithGuardPageBehind(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, size_t size_of_guard_area/* = 1*/)
{
    return CreateBrickWithGuardPage(pixel_type, width, height, depth, size_of_guard_area, Utilities::AllocateWithGuardPageBehind);
}

/*static*/Brick Utilities::CreateBrick(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth)
{
    Brick brick;
    brick.info.pixelType = pixel_type;
    brick.info.width = width;
    brick.info.height = height;
    brick.info.depth = depth;
    brick.info.stride_line = width * libCZI::Utils::GetBytesPerPixel(pixel_type);
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;

    brick.data = shared_ptr<void>(
        malloc(brick.info.stride_plane * static_cast<size_t>(brick.info.depth)),
        [=](void* vp)
        {
            free(vp);
        });

    return brick;
}

/*static*/Brick Utilities::CreateBrickWithGuardPage(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, size_t size_of_guard_area, const std::function<GuardedBlock(size_t, size_t)>& allocate_guarded_block)
{
    Brick brick;
    brick.info.pixelType = pixel_type;
    brick.info.width = width;
    brick.info.height = height;
    brick.info.depth = depth;
    brick.info.stride_line = width * libCZI::Utils::GetBytesPerPixel(pixel_type);
    brick.info.stride_plane = brick.info.stride_line * brick.info.height;

    auto allocation = allocate_guarded_block(
        brick.info.stride_plane * static_cast<size_t>(brick.info.depth),
        size_of_guard_area);

    brick.data = shared_ptr<void>(
        allocation.Get(),
        [=](void* vp)
        {
            Utilities::FreeGuardedBlock(allocation);
        });

    return brick;
}
