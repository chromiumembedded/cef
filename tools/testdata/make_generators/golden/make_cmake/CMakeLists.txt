cmake_minimum_required(VERSION 3.10)
set(FIXTURE_INCLUDE_SRCS
  include/cef_fixture.h
  )
source_group(fixture_root\\\\include FILES ${FIXTURE_INCLUDE_SRCS})

set(FIXTURE_LIBCEF_DLL_SRCS
  libcef_dll/fixture.cc
  )
set(FIXTURE_LIBCEF_DLL_SRCS_WINDOWS
  libcef_dll/fixture_win.cc
  )
APPEND_PLATFORM_SOURCES(FIXTURE_LIBCEF_DLL_SRCS)
source_group(fixture_root\\\\libcef_dll FILES ${FIXTURE_LIBCEF_DLL_SRCS})

add_library(fixture_library
  ${FIXTURE_INCLUDE_SRCS}
  ${FIXTURE_LIBCEF_DLL_SRCS}
  )
