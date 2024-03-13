# SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

include(FetchContent)


FetchContent_Declare(
  libCZI
  GIT_REPOSITORY https://github.com/ZEISS/libczi.git
  GIT_TAG        origin/main
)

if(NOT libCZI_POPULATED)
  message(STATUS "Fetching libCZI")
  FetchContent_Populate(libCZI)

  set(LIBCZI_BUILD_CZICMD OFF CACHE BOOL "" FORCE)
  set(LIBCZI_BUILD_DYNLIB OFF CACHE BOOL "" FORCE)
  set(LIBCZI_BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
  
  # since warpaffine requires the presence of Eigen3 (either a private copy or from system's package manager),
  #  we can safely assume that an external package is available here (so that libCZI does not have to download its
  #  own copy).
  set(LIBCZI_BUILD_PREFER_EXTERNALPACKAGE_EIGEN3 ON CACHE BOOL "" FORCE)

  add_subdirectory(${libczi_SOURCE_DIR} ${libczi_BINARY_DIR})
endif()