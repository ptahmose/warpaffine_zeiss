# SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

set(LIBWARPAFFINE_SRCFILES 
"inc_libCZI.h"
"czi_helpers.cpp"
"czi_helpers.h" 
"deskew_helpers.cpp"
"deskew_helpers.h" 
"document_info.h"
"main.cpp" 
"cmdlineoptions.h" 
"cmdlineoptions.cpp" 
"cmdlineoptions_enums.h" 
"operationtype.h" 
"utilities.h" 
"utilities_windows.h" 
"utilities.cpp" 
"appcontext.h" 
"consoleio.h" 
"appcontext.cpp" 
"consoleio.cpp" 
"dowarp.h" 
"dowarp.cpp" 
"brick.h" 
"mmstream/mmstream.cpp" 
"mmstream/mmstream.h" 
"BrickAllocator.h" 
"BrickAllocator.cpp" 
"calcresulthash.h"
"calcresulthash.cpp"
"mmstream/IStreamEx.h" 
"mmstream/StreamEx.h"
"mmstream/StreamEx.cpp"
"warpaffine/IWarpAffine.h" 
"warpaffine/WarpAffine_IPP.h" 
"warpaffine/WarpAffine_IPP.cpp"
"warpaffine/WarpAffine.cpp"
"warpaffine/WarpAffineNull.h" 
"warpaffine/WarpAffineNull.cpp" 
"taskarena/ITaskArena.h" 
"taskarena/taskarena_tbb.cpp" 
"taskarena/taskarena.cpp" 
"taskarena/taskarena_tbb.h" 
"printstatistics.h" 
"printstatistics.cpp"
"brickreader/linearreading_orderhelper.h"
"brickreader/linearreading_orderhelper.cpp"
"brickreader/IBrickReader.h"
"brickreader/BrickReader.cpp"
"brickreader/czi_brick_reader.h"
"brickreader/czi_brick_reader.cpp"
"brickreader/czi_brick_reader2.h"
"brickreader/czi_brick_reader2.cpp"
"brickreader/czi_linear_brick_reader.h"
"brickreader/czi_linear_brick_reader.cpp"
"brickreader/brick_enumerator.h"
"brickreader/brick_enumerator.cpp"
"sliceswriter/ISlicesWriter.h"
"sliceswriter/NullSlicesWriter.cpp"
"sliceswriter/NullSlicesWriter.h"
"sliceswriter/SlicesWriter.cpp" 
"sliceswriter/SlicesWriterTbb.cpp"
"sliceswriter/SlicesWriterTbb.h"
"brickreader/brick_bucket_manager.cpp"
"brickreader/brick_bucket_manager.h" 
"brickreader/brick_coordinate.h" 
"brickreader/czi_brick_reader_base.h" 
"brickreader/czi_brick_reader_base.cpp" 
"geotypes.h" 
"configure.h"
"configure.cpp" 
"warpaffine/reference_warp.h" 
"warpaffine/reference_warp.cpp" 
"warpaffine/WarpAffine_Reference.h" 
"warpaffine/WarpAffine_Reference.cpp")

add_library(libwarpaffine ${LIBWARPAFFINE_SRCFILES})

target_compile_definitions(libwarpaffine PRIVATE _LIBCZISTATICLIB RAPIDJSON_HAS_STDSTRING=1)

target_link_libraries(libwarpaffine PRIVATE TBB::tbb libCZIStatic CLI11::CLI11 Eigen3::Eigen tinyxml2)

target_include_directories(libwarpaffine PRIVATE ${IPP_INCLUDE_DIRS}  ${TBB_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIRS} ${LIBCZI_INCLUDE_DIR})

if(WIN32)
	set(LibWarpAffine_UNIX_ENVIRONMENT 0)
	set(LibWarpAffine_WIN32_ENVIRONMENT 1)
endif()
if(UNIX)
  set(LibWarpAffine_UNIX_ENVIRONMENT 1)
	set(LibWarpAffine_WIN32_ENVIRONMENT 0)
endif()
if (NOT IPP_FOUND)
  set(LibWarpAffine_INTELPERFORMANCEPRIMITIVES_AVAILABLE 0)
else()
  set(LibWarpAffine_INTELPERFORMANCEPRIMITIVES_AVAILABLE 1)
endif()
if (TBB_VERSION)
  set(libWarpAffine_TBB_VERSION ${TBB_VERSION})
else()
  set(libWarpAffine_TBB_VERSION unknown)
endif()


configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/LibWarpAffine_Config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/LibWarpAffine_Config.h"
  )

target_include_directories(libwarpaffine PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
