# SPDX-FileCopyrightText: 2024 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

# download the key to system keyring
wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB| gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
# add signed entry to apt sources and configure the APT client to use Intel repository:
echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list
# Update packages list and repository index
sudo apt update
#
sudo apt install intel-oneapi-ipp-devel rapidjson-dev libssl-dev libeigen3-dev libcurl4-openssl-dev
#
vcpkg install --triplet x64-linux tbb

source /opt/intel/oneapi/setvars.sh
cmake -B $1 -DCMAKE_BUILD_TYPE=Release -DLIBCZI_BUILD_CURL_BASED_STREAM=ON -DLIBCZI_BUILD_PREFER_EXTERNALPACKAGE_LIBCURL=OFF -DWARPAFFINE_BUILD_PREFER_EXTERNALPACKAGE_RAPIDJSON=ON -DLIBCZI_BUILD_PREFER_EXTERNALPACKAGE_EIGEN3=ON -DCMAKE_TOOLCHAIN_FILE=/usr/local/share/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build $1 --config Release "-j$(nproc)"
