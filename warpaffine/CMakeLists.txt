# SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

set(WARPAFFINE_SRCFILES 
"inc_libwarpaffine.h"
"main.cpp" 
"warpaffine-longpaths.manifest" # the manifest is needed to allow long paths on windows, see https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=cmd
                                # CMake will merge the manifest with a linker generated one (and ignore it on non-Windows platforms)
"warpaffine.rc"                 # the rc file (containing version information, only relevant for Windows))
)

add_executable(warpaffine ${WARPAFFINE_SRCFILES}  $<TARGET_OBJECTS:libwarpaffine>)

target_link_libraries(warpaffine PRIVATE ${ADDITIONAL_LIBS_REQUIRED_FOR_ATOMIC} TBB::tbb libCZIStatic CLI11::CLI11)

IF(WIN32) 
    set(IPP_LIBRARYNAME_SUFFIX "mt")
ENDIF()

if (IPP_FOUND)
  target_link_libraries(warpaffine PRIVATE
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippi${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippcc${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippcore${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippvm${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ipps${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()


if(UNIX)
  # seems to be problem with glibc I'd reckon -> https://stackoverflow.com/questions/51584960/stdcall-once-throws-stdsystem-error-unknown-error-1
  target_link_libraries(warpaffine  PUBLIC pthread)
endif(UNIX)

if(WIN32)
	set(WarpAffine_UNIX_ENVIRONMENT 0)
	set(WarpAffine_WIN32_ENVIRONMENT 1)
endif()
if(UNIX)
  set(WarpAffine_UNIX_ENVIRONMENT 1)
	set(WarpAffine_WIN32_ENVIRONMENT 0)
endif()
if (NOT IPP_FOUND)
  set(WarpAffine_INTELPERFORMANCEPRIMITIVES_AVAILABLE 0)
else()
  set(WarpAffine_INTELPERFORMANCEPRIMITIVES_AVAILABLE 1)
endif()


configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/WarpAffine_Config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/WarpAffine_Config.h"
  )

set(warpaffine_PROJECT_DESCRIPTION ${CMAKE_PROJECT_DESCRIPTION})  # the string given for the project description in the main CMakeLists.txt

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/resource_data.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/resource_data.h"
  )

# On Windows, we want to copy the tbb-runtime to the destination folder. There is no
# supported way to link it statically, so this DLL needs to be provided with the executable.
IF(WIN32) 
  if (CMAKE_BUILD_TYPE MATCHES "Debug")
      set(_TBB_DLL_TO_SEARCH_FOR "tbb12_debug.dll")
  else()
      set(_TBB_DLL_TO_SEARCH_FOR "tbb12.dll")
  endif()
  
  # we try to find the tbb-dll either as part of an "oneAPI" installation or from a vcpkg-provided version
  find_file(TBB_DLL_TO_COPY
             ${_TBB_DLL_TO_SEARCH_FOR}
             HINTS "${TBB_DIR}/../../../redist/${_tbb_subdir}/" "${TBB_DIR}/../../bin")
  message(STATUS "-> TBB_DLL_TO_COPY: '${TBB_DLL_TO_COPY}'")

  if (TBB_DLL_TO_COPY)  # if we found the tbb12-dll, we copy it to the binary folder
    add_custom_command(
          TARGET warpaffine POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
                  "${TBB_DLL_TO_COPY}"
                  "${CMAKE_CURRENT_BINARY_DIR}")
  endif()
endif()

target_include_directories(warpaffine PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# checking platform -> https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/Checking-Platform
#                   -> https://stackoverflow.com/questions/9160335/os-specific-instructions-in-cmake-how-to

install(TARGETS warpaffine RUNTIME  DESTINATION bin)
