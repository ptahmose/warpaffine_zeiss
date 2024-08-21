// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include "ISlicesWriter.h"

#include <memory>
#include <atomic>
#include <thread>

#include <tbb/concurrent_queue.h>

/// Implementation of a ICziSlicesWriter that uses a MPSC-queue to serialize the slice-write operations.
/// The actual CZI-writing is handled by libCZI.
class CziSlicesWriterTbb : public ICziSlicesWriter
{
private:
    static constexpr std::intptr_t kItemMarker_ItemToWrite = 1;
    static constexpr std::intptr_t kItemMarker_Shutdown = 0;

    AppContext& context_;
    std::thread worker_thread_;

    std::shared_ptr<libCZI::ICziWriter> writer_;

    std::atomic_uint32_t number_of_slicewrite_operations_in_flight_{ 0 };

    struct SubBlockWriteInfo2
    {
        AddSliceInfo    add_slice_info;
    };

    tbb::concurrent_bounded_queue<SubBlockWriteInfo2> queue_;
    libCZI::GUID retilingBaseId_;
    bool use_acquisition_tiles_;
public:
    CziSlicesWriterTbb(AppContext& context, const std::wstring& filename);

    std::uint32_t GetNumberOfPendingSliceWriteOperations() override;

    void AddSlice(const AddSliceInfo& add_slice_info) override;
    void AddAttachment(const std::shared_ptr<libCZI::IAttachment>& attachment) override;
    void Close(const std::shared_ptr<libCZI::ICziMetadata>& source_metadata,
                const libCZI::ScalingInfo* new_scaling_info,
                const std::function<void(libCZI::IXmlNodeRw*)>& tweak_metadata_hook) override;

private:
    void WriteWorker();
    void CopyMetadata(libCZI::IXmlNodeRead* rootSource, libCZI::IXmlNodeRw* rootDestination);
    libCZI::GUID CreateRetilingIdWithZandSlice(int z, std::uint32_t slice) const;
};
