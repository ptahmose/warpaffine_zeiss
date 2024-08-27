// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "SlicesWriterTbb.h"
#include <iomanip>
#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <iostream>

using namespace std;
using namespace libCZI;

CziSlicesWriterTbb::CziSlicesWriterTbb(AppContext& context, const std::wstring& filename)
    : context_(context)
{
    this->queue_.set_capacity(500 * 10);
    this->use_acquisition_tiles_ = context.GetCommandLineOptions().GetUseAcquisitionTiles();
    this->retilingBaseId_ = Utilities::GenerateGuid();

    // Create an "output-stream-object"
    const auto output_stream = libCZI::CreateOutputStreamForFile(filename.c_str(), true);

    this->writer_ = libCZI::CreateCZIWriter();
    const auto spWriterInfo = make_shared<CCziWriterInfo>(libCZI::GUID{ 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } });
    this->writer_->Create(output_stream, spWriterInfo);

    this->number_of_slicewrite_operations_in_flight_.store(0);
    this->worker_thread_ = std::thread([this] {this->WriteWorker(); });
}

void CziSlicesWriterTbb::AddSlice(const AddSliceInfo& add_slice_info)
{
    SubBlockWriteInfo2 info;
    info.add_slice_info = add_slice_info;
    this->queue_.push(info);
    ++this->number_of_slicewrite_operations_in_flight_;
}

void CziSlicesWriterTbb::AddAttachment(const std::shared_ptr<libCZI::IAttachment>& attachment)
{
    AddAttachmentInfo add_attachment_info;
    const auto& attachment_info = attachment->GetAttachmentInfo();
    add_attachment_info.contentGuid = attachment_info.contentGuid;
    add_attachment_info.SetContentFileType(attachment_info.contentFileType);
    add_attachment_info.SetName(attachment_info.name.c_str());
    size_t size_of_data;
    auto raw_data = attachment->GetRawData(&size_of_data);
    add_attachment_info.ptrData = raw_data.get();
    add_attachment_info.dataSize = static_cast<uint32_t>(size_of_data);
    this->writer_->SyncAddAttachment(add_attachment_info);
}

void CziSlicesWriterTbb::WriteWorker()
{
    try
    {
        for (;;)
        {
            SubBlockWriteInfo2 sub_block_write_info;
            this->queue_.pop(sub_block_write_info);

            if (!sub_block_write_info.add_slice_info.subblock_raw_data)
            {
                break;
            }

            if (sub_block_write_info.add_slice_info.scene_index.has_value())
            {
                sub_block_write_info.add_slice_info.coordinate.Set(DimensionIndex::S, sub_block_write_info.add_slice_info.scene_index.value());
            }

            AddSubBlockInfoMemPtr add_subblock_info;
            add_subblock_info.Clear();
            add_subblock_info.coordinate = sub_block_write_info.add_slice_info.coordinate;
            add_subblock_info.x = sub_block_write_info.add_slice_info.x_position;
            add_subblock_info.y = sub_block_write_info.add_slice_info.y_position;
            add_subblock_info.logicalWidth = sub_block_write_info.add_slice_info.width;
            add_subblock_info.logicalHeight = sub_block_write_info.add_slice_info.height;
            add_subblock_info.physicalWidth = sub_block_write_info.add_slice_info.width;
            add_subblock_info.physicalHeight = sub_block_write_info.add_slice_info.height;
            add_subblock_info.PixelType = sub_block_write_info.add_slice_info.pixeltype;
            if (sub_block_write_info.add_slice_info.m_index)
            {
                add_subblock_info.mIndex = sub_block_write_info.add_slice_info.m_index.value();
                add_subblock_info.mIndexValid = true;
            }

            add_subblock_info.SetCompressionMode(sub_block_write_info.add_slice_info.compression_mode);

            add_subblock_info.ptrData = sub_block_write_info.add_slice_info.subblock_raw_data->GetPtr();
            add_subblock_info.dataSize = sub_block_write_info.add_slice_info.subblock_raw_data->GetSizeOfData();

            if (sub_block_write_info.add_slice_info.brick_id.has_value() && this->use_acquisition_tiles_)
            {
                int z;
                sub_block_write_info.add_slice_info.coordinate.TryGetPosition(libCZI::DimensionIndex::Z, &z);
                auto guid = this->CreateRetilingIdWithZandSlice(z, sub_block_write_info.add_slice_info.brick_id.value());

                std::ostringstream oss;
                oss << "<METADATA><Tags><RetilingId>"
                    << std::hex << std::uppercase
                    << std::setw(8) << std::setfill('0') << guid.Data1 << '-'
                    << std::setw(4) << std::setfill('0') << guid.Data2 << '-'
                    << std::setw(4) << std::setfill('0') << guid.Data3 << '-'
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[0])
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[1]) << '-'
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[2])
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[3])
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[4])
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[5])
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[6])
                    << std::setw(2) << static_cast<uint32_t>(guid.Data4[7])
                    << std::dec
                    <<"</RetilingId></Tags></METADATA>";

                const string metadata_xml = oss.str();
                add_subblock_info.ptrSbBlkMetadata = metadata_xml.c_str();
                add_subblock_info.sbBlkMetadataSize = static_cast<uint32_t>(metadata_xml.size());

                this->writer_->SyncAddSubBlock(add_subblock_info);
            }
            else
            {
                this->writer_->SyncAddSubBlock(add_subblock_info);
            }

            --this->number_of_slicewrite_operations_in_flight_;
        }
    }
    catch (libCZI::LibCZIIOException& libCZI_io_exception)
    {
        ostringstream text;
        text << "SlicesWriterTbb-worker crashed due to I/O error (offset=" << libCZI_io_exception.GetOffset() << "): " << endl;
        text << "    message: " << libCZI_io_exception.what() << ".";
        try
        {
            libCZI_io_exception.rethrow_nested();
        }
        catch (exception& nested_exception)
        {
            text << endl;
            text << "    inner exception: " << nested_exception.what() << ".";
        }

        text << endl;
        this->context_.FatalError(text.str());
    }
    catch (exception& exception)
    {
        ostringstream text;
        text << "SlicesWriterTbb-worker crashed: " << exception.what() << ".";
        this->context_.FatalError(text.str());
    }
}

libCZI::GUID CziSlicesWriterTbb::CreateRetilingIdWithZandSlice(int z, uint32_t slice) const
{
    libCZI::GUID guid = this-> retilingBaseId_;
    guid.Data4[0] = static_cast<uint8_t>(z >> 24);
    guid.Data4[1] = static_cast<uint8_t>(z >> 16);
    guid.Data4[2] = static_cast<uint8_t>(z >> 8);
    guid.Data4[3] = static_cast<uint8_t>(z);

    guid.Data4[4] = static_cast<uint8_t>(slice >> 24);
    guid.Data4[5] = static_cast<uint8_t>(slice >> 16);
    guid.Data4[6] = static_cast<uint8_t>(slice >> 8);
    guid.Data4[7] = static_cast<uint8_t>(slice);

    return guid;
}

void CziSlicesWriterTbb::Close(const std::shared_ptr<libCZI::ICziMetadata>& source_metadata,
                                const libCZI::ScalingInfo* new_scaling_info,
                                const std::function<void(libCZI::IXmlNodeRw*)>& tweak_metadata_hook)
{
    SubBlockWriteInfo2 sub_block_write_info;
    this->queue_.push(sub_block_write_info);
    this->worker_thread_.join();

    if (!source_metadata)
    {
        // if we are not provided with "metadata of source-document", then we only write the minimal set of metadata
        PrepareMetadataInfo prepareInfo;
        prepareInfo.funcGenerateIdAndNameForChannel = [](int channelIndex)->tuple<string, tuple<bool, string>>
            {
                stringstream ssId, ssName;
                ssId << "Channel:" << channelIndex;
                ssName << "Channel #" << channelIndex;
                return make_tuple(ssId.str(), make_tuple(true, ssName.str()));
            };

        const auto metadata_builder = this->writer_->GetPreparedMetadata(prepareInfo);

        // now we could add additional information
        GeneralDocumentInfo docInfo;
        docInfo.SetName(L"WarpAffine");
        docInfo.SetTitle(L"WarpAffine generated");
        docInfo.SetComment(L"");
        MetadataUtils::WriteGeneralDocumentInfo(metadata_builder.get(), docInfo);

        if (new_scaling_info != nullptr)
        {
            MetadataUtils::WriteScalingInfo(metadata_builder.get(), *new_scaling_info);
        }

        if (tweak_metadata_hook)
        {
            tweak_metadata_hook(metadata_builder->GetRootNode().get());
        }

        WriteMetadataInfo metadata_info;
        metadata_info.Clear();
        const string source_metadata_xml = metadata_builder->GetXml();
        metadata_info.szMetadata = source_metadata_xml.c_str();
        metadata_info.szMetadataSize = source_metadata_xml.size();
        this->writer_->SyncWriteMetadata(metadata_info);
    }
    else
    {
        const auto metadata_builder_from_source = CreateMetadataBuilderFromXml(source_metadata->GetXml());

        // We copy some specific values from the automatically generated metadata to the copy of the source
        //  we created above. This may not be the best way to approach this, but seems to fit the bill so far.  
        const auto automatically_generated_metadata = this->writer_->GetPreparedMetadata(PrepareMetadataInfo());
        this->CopyMetadata(automatically_generated_metadata->GetRootNode().get(), metadata_builder_from_source->GetRootNode().get());

        GeneralDocumentInfo docInfo;
        docInfo.SetComment(L"WarpAffine generated");
        MetadataUtils::WriteGeneralDocumentInfo(metadata_builder_from_source.get(), docInfo);

        if (new_scaling_info != nullptr)
        {
            MetadataUtils::WriteScalingInfo(metadata_builder_from_source.get(), *new_scaling_info);
        }

        if (new_scaling_info->IsScaleZValid())
        {
            auto increment_node = metadata_builder_from_source->GetRootNode()->GetOrCreateChildNode("Metadata/Information/Image/Dimensions/Z/Positions/Interval/Increment");
            if (increment_node)
            {
                const double original_scaling_meters = new_scaling_info->scaleZ;

                // increment should be in micrometers
                increment_node->SetValueDbl(original_scaling_meters * 1e6);
            }
        }

        // allow the caller to tweak the metadata before it is written out
        if (tweak_metadata_hook)
        {
            tweak_metadata_hook(metadata_builder_from_source->GetRootNode().get());
        }

        WriteMetadataInfo metadata_info;
        metadata_info.Clear();
        const string source_metadata_xml = metadata_builder_from_source->GetXml();
        metadata_info.szMetadata = source_metadata_xml.c_str();
        metadata_info.szMetadataSize = source_metadata_xml.size();
        this->writer_->SyncWriteMetadata(metadata_info);
    }

    this->writer_->Close();
    this->writer_.reset();
}

std::uint32_t CziSlicesWriterTbb::GetNumberOfPendingSliceWriteOperations()
{
    return this->number_of_slicewrite_operations_in_flight_.load();
}

void CziSlicesWriterTbb::CopyMetadata(libCZI::IXmlNodeRead* rootSource, libCZI::IXmlNodeRw* rootDestination)
{
    // what we do here is to simple copy the values of those nodes from the source to the destination
    static constexpr array<const char*, 5> paths_to_copy =
    {
        "Metadata/Information/Image/SizeX",
        "Metadata/Information/Image/SizeY",
        "Metadata/Information/Image/SizeZ",
        "Metadata/Information/Image/SizeT",
        "Metadata/Information/Image/SizeC"
    };

    for (const auto path : paths_to_copy)
    {
        if (const auto nodeSource = rootSource->GetChildNodeReadonly(path))
        {
            wstring value;
            if (nodeSource->TryGetValue(&value))
            {
                rootDestination->GetOrCreateChildNode(path)->SetValue(value);
            }
        }
    }
}
