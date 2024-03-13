# SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: MIT

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY "https://github.com/google/googletest"
  GIT_TAG        v1.12.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
  warpaffine_unittests
  $<TARGET_OBJECTS:libwarpaffine>
 "brick_enumerator_tests.cpp"
 "czi_helpers_tests.cpp" 
 "mem_output_stream.h" 
 "mem_output_stream.cpp"  
 "warpaffine_tests.cpp" 
 "utilities_tests.cpp"
 "utilities.h"
 "utilities.cpp")

target_include_directories(warpaffine_unittests PRIVATE ${IPP_INCLUDE_DIRS} ${EIGEN3_INCLUDE_DIR} ${LIBCZI_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(warpaffine_unittests PRIVATE _LIBCZISTATICLIB)
set_target_properties(warpaffine_unittests PROPERTIES CXX_STANDARD 17)
IF(WIN32) 
    set(IPP_LIBRARYNAME_SUFFIX "mt")
ENDIF()
target_link_libraries(warpaffine_unittests 
        GTest::gtest_main
        ${ADDITIONAL_LIBS_REQUIRED_FOR_ATOMIC} TBB::tbb libCZIStatic CLI11::CLI11
        Eigen3::Eigen)

if (IPP_FOUND)
  target_link_libraries(warpaffine_unittests 
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippi${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippcc${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippcore${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ippvm${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${IPP_LIB_SEARCHPATH}/${CMAKE_STATIC_LIBRARY_PREFIX}ipps${IPP_LIBRARYNAME_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

IF(WIN32)
	set(WarpAffineUnitTests_UNIX_ENVIRONMENT 0)
	set(WarpAffineUnitTests_WIN32_ENVIRONMENT 1)
ENDIF()
IF(UNIX)
  set(WarpAffineUnitTests_UNIX_ENVIRONMENT 1)
	set(WarpAffineUnitTests_WIN32_ENVIRONMENT 0)
ENDIF()
if (NOT IPP_FOUND)
  set(WarpAffineUnitTests_INTELPERFORMANCEPRIMITIVES_AVAILABLE 0)
else()
  set(WarpAffineUnitTests_INTELPERFORMANCEPRIMITIVES_AVAILABLE 1)
endif()


configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/warpafine_unittests_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/warpafine_unittests_config.h"
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
          TARGET warpaffine_unittests POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
                  "${TBB_DLL_TO_COPY}"
                  "${CMAKE_CURRENT_BINARY_DIR}")
  endif()
endif()

include(GoogleTest)
gtest_discover_tests(warpaffine_unittests)