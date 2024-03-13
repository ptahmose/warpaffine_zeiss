// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <warpafine_unittests_config.h>
#include "../libwarpaffine/warpaffine/IWarpAffine.h"

class Utilities
{
public:
    struct GuardedBlock
    {
    private:
        void* baseAddress;
        void* userBuffer;
    public:
        GuardedBlock(void* baseAddress, void* userBuffer)
            : baseAddress(baseAddress), userBuffer(userBuffer)
        {
        }

        GuardedBlock() : GuardedBlock(nullptr, nullptr) {}

        void* Get() { return this->userBuffer; }

        bool IsEmpty() const { return this->userBuffer == nullptr; }

        friend class Utilities;
    };

    /// Allocate the specified amount of memory, with guard-pages (or more exactly "NoAccess"-pages)
    /// directly after it. That means that accessing a byte (for read or write) behind the allocated
    /// block will result in an access-violation. This is useful for ensuring that no access behind a
    /// given size is made. The allocated memory must be freed with "FreeGuardedBlock". The size of the
    /// guard area is given with the parameter "size_of_guard_block" and will be rounded up to a multiple
    /// of the page-size.
    /// In case of an error, an empty GuardedBlock-object will be returned.
    /// \param size The size of the memory block to allocated in bytes.
    /// \param size_of_guard_block The size of the guard block (directly behind the memory block) in bytes, will be rounded up to a multiple of the page-size.
    /// \returns A structure with the pointer to the allocated memory.
    static GuardedBlock AllocateWithGuardPageBehind(size_t size, size_t size_of_guard_block);

    /// Allocate the specified amount of memory, with guard-pages (or more exactly "NoAccess"-pages)
    /// directly before it. That means that accessing a byte (for read or write) before the allocated
    /// block will result in an access-violation. This is useful for ensuring that no access behind a
    /// given size is made. The allocated memory must be freed with "FreeGuardedBlock". The size of the
    /// guard area is given with the parameter "size_of_guard_block" and will be rounded up to a multiple
    /// of the page-size.
    /// In case of an error, an empty GuardedBlock-object will be returned.
    /// \param size The size of the memory block to allocated in bytes.
    /// \param size_of_guard_block The size of the guard block (directly before the memory block) in bytes, will be rounded up to a multiple of the page-size.
    /// \returns A structure with the pointer to the allocated memory.
    static GuardedBlock AllocateWithGuardPageBefore(size_t size, size_t size_of_guard_block);

    /// Releases the "guarded block".
    /// \param  block   The "guarded block" to be released.
    static void FreeGuardedBlock(const GuardedBlock& block);

    static Brick CreateBrick(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth);
    static Brick CreateBrickWithGuardPageBefore(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, size_t size_of_guard_area = 1);
    static Brick CreateBrickWithGuardPageBehind(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, size_t size_of_guard_area = 1);

private:
    static Brick CreateBrickWithGuardPage(libCZI::PixelType pixel_type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, size_t size_of_guard_area, const std::function<GuardedBlock(size_t, size_t)>& allocate_guarded_block);
};
