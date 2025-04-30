# SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0091 NEW) # enable new "MSVC runtime library selection" (https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html)

if(WIN32)
  # use "static C-runtime" -> https://stackoverflow.com/questions/14172856/compile-with-mt-instead-of-md-using-cmake
  # Note: this requires CMAKE version 3.15+
  cmake_policy(SET CMP0091 NEW)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif(WIN32)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

project ("warpaffine"
          VERSION 0.5.3
          DESCRIPTION "experimental Deskew operation")

option(WARPAFFINE_BUILD_CLANGTIDY "Build with Clang-Tidy" OFF)

# This option is only relevant if IPP is not found - i.e. we still try to find the IPP libraries, but if we don't find them, 
# we don't fail the build. The application will then be somewhat crippled (i.e. way slower), but should nevertheless be operational.
option(WARPAFFINE_ALLOW_BUILD_WITHOUT_IPP "Allow for a build without Intel-IPP" OFF)

# This option allows to "downlad a private copy of Eigen3 and usage of this for libCZI and warpaffine". If this option is not given,
#  then the availability of an Eigen3-package (from the system's package manager') is assumed and required.
option(WARPAFFINE_USE_PRIVATE_EIGEN3 "Eigen3 is downloaded as part of the build (as opposed to a package available on the system is used)" OFF)

# When this option is ON, we try to use an existing RapidJSON-library. Otherwise, we download a private
#  copy of RapidJSON during the CMake-run.
option(WARPAFFINE_BUILD_PREFER_EXTERNALPACKAGE_RAPIDJSON "Prefer an RapidJSON-package present on the system" OFF)

if (WARPAFFINE_BUILD_CLANGTIDY)
  # How "clang-tidy" organization works (if this option is enable here):
  # - the compiler is looking for a ".clang-tidy"-file (containing configuration) in the parent folder of each compilation unit
  # - this ".clang-tidy"-file in turn my refer to another ".clang-tidy"-file in its parent directory if the option "InheritParentConfig" is set to true
  # - _on top_ of that the option we give here with the checks-argument is applied (where we globally turn off some checkers)
  # please see -> https://clang.llvm.org/extra/clang-tidy for details
  # Note: this means that if there is no ".clang-tidy"-file (in the parent folder of a .cpp file), then no checks are done
  set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=-llvm-*,-llvmlibc-*,-fuchsia-*,-altera-*,-hicpp-*,-abseil-*")
endif()

include(ExternalProject)

if (WARPAFFINE_USE_PRIVATE_EIGEN3)
  include(FetchContent)
  FetchContent_Declare(
          Eigen
          GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
          GIT_TAG 3.4.0
          GIT_SHALLOW TRUE
          GIT_PROGRESS TRUE)

  set(EIGEN_BUILD_DOC OFF)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(EIGEN_BUILD_PKGCONFIG OFF)
  FetchContent_MakeAvailable(Eigen)
endif()

include("${CMAKE_SOURCE_DIR}/modules/libCZI.cmake")
FetchContent_GetProperties(libCZI)
set(LIBCZI_INCLUDE_DIR "${libczi_SOURCE_DIR}/Src/libCZI")
message(STATUS ${LIBCZI_INCLUDE_DIR})

find_package(IPP REQUIRED)
if (NOT IPP_FOUND AND NOT WARPAFFINE_ALLOW_BUILD_WITHOUT_IPP)
  message(FATAL_ERROR "IPP not found. Please install IPP (or set WARPAFFINE_ALLOW_BUILD_WITHOUT_IPP to ON).")
endif()

find_package(TBB REQUIRED)
message(VERBOSE "*** TBB found at: ${TBB_DIR} ***")

include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.5.0
)

if (NOT WARPAFFINE_USE_PRIVATE_EIGEN3)
  find_package(Eigen3 REQUIRED)
endif()

FetchContent_MakeAvailable(cli11)

if (WARPAFFINE_BUILD_PREFER_EXTERNALPACKAGE_RAPIDJSON)
    find_package(RapidJSON QUIET)
    if (NOT RapidJSON_FOUND)
        message(FATAL_ERROR [=[
        RapidJSON library was not found, which is required for building. Consider installing
        like 'sudo apt-get install rapidjson-dev'. Alternatively, consider setting the option
        WARPAFFINE_BUILD_PREFER_EXTERNALPACKAGE_RAPIDJSON to OFF in order to download and build RapidJSON
        automatically as part of the build process.
        ]=])
    endif()
else()
    # since "RapidJSON" is a header-only library, we just have to download it and point to the include directory
     FetchContent_Declare(
            RapidJSON
            GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
            GIT_TAG        "v1.1.0"
            GIT_SHALLOW    TRUE
            PREFIX "${CMAKE_BINARY_DIR}/vendor/rapidjson"
    )

    if (NOT rapidjson_POPULATED)
      FetchContent_Populate(RapidJSON)
      set(RAPIDJSON_INCLUDE_DIRS ${rapidjson_SOURCE_DIR}/include)
    endif()
endif()

include("${CMAKE_SOURCE_DIR}/modules/CheckForLibAtomicRequired.cmake")
CheckForAdditionalLibsRequiredForAtomic(ADDITIONAL_LIBS_REQUIRED_FOR_ATOMIC)

if (ADDITIONAL_LIBS_REQUIRED_FOR_ATOMIC)
  message(STATUS "It was determined that linking to those additional libraries is necessary for using 'atomic': ${ADDITIONAL_LIBS_REQUIRED_FOR_ATOMIC}.")
endif()

enable_testing()

add_subdirectory ("libwarpaffine")
add_subdirectory ("warpaffine")
add_subdirectory ("warpaffine_unittests")
