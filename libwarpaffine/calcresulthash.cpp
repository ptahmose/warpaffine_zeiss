// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#include "calcresulthash.h"
#include <string>

using namespace std;
using namespace libCZI;

CalcResultHash::CalcResultHash()
{
    this->hash_.fill(0);
}

void CalcResultHash::AddSlice(const std::shared_ptr<libCZI::IMemoryBlock>& memory_block, const libCZI::CDimCoordinate& coordinate)
{
    std::array<std::uint8_t, 16> hash_of_data;
    size_t size_of_data = memory_block->GetSizeOfData();
    Utils::CalcMd5SumHash(memory_block->GetPtr(), size_of_data, hash_of_data.data(), sizeof(hash_of_data));

    std::array<std::uint8_t, 16> hash_of_coordinate;
    string coordinate_string_representation = Utils::DimCoordinateToString(&coordinate);
    Utils::CalcMd5SumHash(coordinate_string_representation.c_str(), coordinate_string_representation.size(), hash_of_coordinate.data(), sizeof(hash_of_coordinate));

   /*{
        std::lock_guard<std::mutex> guard(this->mutex_);
        FILE* fp = fopen("N:\\log.txt", "a");
        fprintf(fp, "%s: %s - %s\n", coordinate_string_representation.c_str(), PrintHash(hash_of_coordinate).c_str(), PrintHash(hash_of_data).c_str());
        fclose(fp);
    }*/

    this->AddHash(hash_of_data);
    this->AddHash(hash_of_coordinate);
}

std::array<std::uint8_t, 16> CalcResultHash::GetHash()
{
    std::lock_guard<std::mutex> guard(this->mutex_);
    const array<std::uint8_t, 16> hash = this->hash_;
    return hash;
}

void CalcResultHash::AddHash(const std::array<std::uint8_t, 16>& hash_to_add)
{
    std::lock_guard<std::mutex> guard(this->mutex_);

    // we use a simple XOR-scheme (which gives us order-independence), maybe there are better ways?
    for (int i = 0; i < 16; ++i)
    {
        this->hash_[i] ^= hash_to_add[i];
    }
}
