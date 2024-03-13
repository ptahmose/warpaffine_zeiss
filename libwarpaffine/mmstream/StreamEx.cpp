// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "StreamEx.h"
#include <utility>
#include <memory>
#include <map>
#include <atomic>
#include "../utilities.h"

using namespace std;

/// This class extends the stock-libCZI-stream-reader object with the ability to report
/// the number of bytes read.
class StockStreamEx : public IStreamEx
{
private:
    std::shared_ptr<libCZI::IStream> stream_;
    std::atomic_uint64_t total_bytes_read_{ 0 };
public:
    explicit StockStreamEx(std::shared_ptr<libCZI::IStream> stream) :stream_(std::move(stream))
    {}

    void Read(std::uint64_t offset, void* pv, std::uint64_t size, std::uint64_t* ptrBytesRead) override
    {
        std::uint64_t bytes_read;
        this->stream_->Read(offset, pv, size, &bytes_read);
        this->total_bytes_read_ += bytes_read;
        if (ptrBytesRead != nullptr)
        {
            *ptrBytesRead = bytes_read;
        }
    }

    std::uint64_t GetTotalBytesRead() override
    {
        return this->total_bytes_read_.load();
    }
};

std::shared_ptr<IStreamEx> CreateStockStreamEx(const wchar_t* filename, const std::string& stream_class, const std::map<int, libCZI::StreamsFactory::Property>& property_bag)
{
    if (stream_class.empty())
    {
        // if there is no stream class specified, use the stock file stream
        return make_shared<StockStreamEx>(libCZI::CreateStreamFromFile(filename));
    }

    libCZI::StreamsFactory::Initialize();
    libCZI::StreamsFactory::CreateStreamInfo stream_info;
    stream_info.class_name = stream_class;
    if (!property_bag.empty())
    {
        stream_info.property_bag = property_bag;
    }

    auto stream = libCZI::StreamsFactory::CreateStream(stream_info, filename);
    if (!stream)
    {
        ostringstream string_stream;
        string_stream << "Could not create instance of stream class '" << stream_class << "' for file '" << Utilities::convertToUtf8(filename) << "'.";
        throw std::runtime_error(string_stream.str());
    }

    return make_shared<StockStreamEx>(stream);
}
