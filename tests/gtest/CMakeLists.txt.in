# Copyright 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

set(CEF_TARGET "cef_gtest")

set(GTEST_SRCS
  src/gtest-all.cc
  teamcity/include/teamcity_gtest.h
  teamcity/src/teamcity_gtest.cpp
  teamcity/src/teamcity_messages.cpp
  teamcity/src/teamcity_messages.h
  )
source_group(cef_gtest FILES ${GTEST_SRCS})

add_library(${CEF_TARGET} ${GTEST_SRCS})

# Start with CEF default properties.
SET_LIBRARY_TARGET_PROPERTIES(${CEF_TARGET})

# The gtest-all.cc file uses #include "gtest/gtest.h"
target_include_directories(${CEF_TARGET} PRIVATE "include")

# In order to allow regex matches in gtest to be shared between Windows
# and other systems we tell gtest to always use it's internal engine.
target_compile_definitions(${CEF_TARGET} PRIVATE -DGTEST_HAS_POSIX_RE=0 -DGTEST_LANG_CXX11=1)

# All dependent targets are unit tests.
target_compile_definitions(${CEF_TARGET} PUBLIC -DUNIT_TEST)

if(OS_WINDOWS)
  # Disable unused variable warning.
  target_compile_options(${CEF_TARGET} PRIVATE "/wd4800")
endif()
