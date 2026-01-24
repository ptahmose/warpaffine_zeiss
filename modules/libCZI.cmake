# SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

include(FetchContent)

# Configure libCZI before declaring it
set(LIBCZI_BUILD_CZICMD OFF CACHE BOOL "" FORCE)
set(LIBCZI_BUILD_DYNLIB OFF CACHE BOOL "" FORCE)
set(LIBCZI_BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)

# since warpaffine requires the presence of Eigen3 (either a private copy or from system's package manager),
# we can safely assume that an external package is available here (so that libCZI does not have to download its
# own copy).
set(LIBCZI_BUILD_PREFER_EXTERNALPACKAGE_EIGEN3 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  libCZI
  GIT_REPOSITORY https://github.com/ZEISS/libczi.git
  GIT_TAG        474670909c741a51c71cf1a5bcace681d5aa4062	# origin/main as of 1/23/2026
)

message(STATUS "Fetching libCZI")
FetchContent_MakeAvailable(libCZI)
