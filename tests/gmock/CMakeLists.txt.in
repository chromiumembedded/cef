# Copyright 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

set(CEF_TARGET "cef_gmock")

set(GMOCK_SRCS
  src/gmock-all.cc
  )
source_group(cef_gmock FILES ${GMOCK_SRCS})

add_library(${CEF_TARGET} ${GMOCK_SRCS})

# Start with CEF default properties.
SET_LIBRARY_TARGET_PROPERTIES(${CEF_TARGET})

# The gmock-all.cc file uses #include "gmock/gmock.h"
target_include_directories(${CEF_TARGET} PRIVATE "include")

# gmock depends on gtest
target_link_libraries(${CEF_TARGET} PRIVATE cef_gtest)

# gmock needs to find gtest headers
target_include_directories(${CEF_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../gtest/include")

if(OS_WINDOWS)
  # Disable unused variable warning.
  target_compile_options(${CEF_TARGET} PRIVATE "/wd4800")
endif()
