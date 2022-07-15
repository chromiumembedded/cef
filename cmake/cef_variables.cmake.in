# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# Must be loaded via FindCEF.cmake.
if(NOT DEFINED _CEF_ROOT_EXPLICIT)
  message(FATAL_ERROR "Use find_package(CEF) to load this file.")
endif()


#
# Shared configuration.
#

# Determine the platform.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  set(OS_MAC 1)
  set(OS_MACOSX 1)  # For backwards compatibility.
  set(OS_POSIX 1)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  set(OS_LINUX 1)
  set(OS_POSIX 1)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(OS_WINDOWS 1)
endif()

# Determine the project architecture.
if(NOT DEFINED PROJECT_ARCH)
  if(OS_WINDOWS AND "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "arm64")
    set(PROJECT_ARCH "arm64")
  elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(PROJECT_ARCH "x86_64")
  else()
    set(PROJECT_ARCH "x86")
  endif()

  if(OS_MAC)
    # PROJECT_ARCH should be specified on Mac OS X.
    message(WARNING "No PROJECT_ARCH value specified, using ${PROJECT_ARCH}")
  endif()
endif()

if(${CMAKE_GENERATOR} STREQUAL "Ninja")
  set(GEN_NINJA 1)
elseif(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
  set(GEN_MAKEFILES 1)
endif()

# Determine the build type.
if(NOT CMAKE_BUILD_TYPE AND (GEN_NINJA OR GEN_MAKEFILES))
  # CMAKE_BUILD_TYPE should be specified when using Ninja or Unix Makefiles.
  set(CMAKE_BUILD_TYPE Release)
  message(WARNING "No CMAKE_BUILD_TYPE value selected, using ${CMAKE_BUILD_TYPE}")
endif()


# Path to the include directory.
set(CEF_INCLUDE_PATH "${_CEF_ROOT}")

# Path to the libcef_dll_wrapper target.
set(CEF_LIBCEF_DLL_WRAPPER_PATH "${_CEF_ROOT}/libcef_dll")


# Shared compiler/linker flags.
list(APPEND CEF_COMPILER_DEFINES
  # Allow C++ programs to use stdint.h macros specified in the C99 standard that aren't 
  # in the C++ standard (e.g. UINT8_MAX, INT64_MIN, etc)
  __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS
  )


# Configure use of the sandbox.
option(USE_SANDBOX "Enable or disable use of the sandbox." ON)


#
# Linux configuration.
#

if(OS_LINUX)
  # Platform-specific compiler/linker flags.
  set(CEF_LIBTYPE SHARED)
  list(APPEND CEF_COMPILER_FLAGS
    -fno-strict-aliasing            # Avoid assumptions regarding non-aliasing of objects of different types
    -fPIC                           # Generate position-independent code for shared libraries
    -fstack-protector               # Protect some vulnerable functions from stack-smashing (security feature)
    -funwind-tables                 # Support stack unwinding for backtrace()
    -fvisibility=hidden             # Give hidden visibility to declarations that are not explicitly marked as visible
    --param=ssp-buffer-size=4       # Set the minimum buffer size protected by SSP (security feature, related to stack-protector)
    -pipe                           # Use pipes rather than temporary files for communication between build stages
    -pthread                        # Use the pthread library
    -Wall                           # Enable all warnings
    -Werror                         # Treat warnings as errors
    -Wno-missing-field-initializers # Don't warn about missing field initializers
    -Wno-unused-parameter           # Don't warn about unused parameters
    -Wno-error=comment              # Don't warn about code in comments
    -Wno-comment                    # Don't warn about code in comments
    -Wno-deprecated-declarations    # Don't warn about using deprecated methods
    )
  list(APPEND CEF_C_COMPILER_FLAGS
    -std=c99                        # Use the C99 language standard
    )
  list(APPEND CEF_CXX_COMPILER_FLAGS
    -fno-exceptions                 # Disable exceptions
    -fno-rtti                       # Disable real-time type information
    -fno-threadsafe-statics         # Don't generate thread-safe statics
    -fvisibility-inlines-hidden     # Give hidden visibility to inlined class member functions
    -std=c++17                      # Use the C++17 language standard
    -Wsign-compare                  # Warn about mixed signed/unsigned type comparisons
    )
  list(APPEND CEF_COMPILER_FLAGS_DEBUG
    -O0                             # Disable optimizations
    -g                              # Generate debug information
    )
  list(APPEND CEF_COMPILER_FLAGS_RELEASE
    -O2                             # Optimize for maximum speed
    -fdata-sections                 # Enable linker optimizations to improve locality of reference for data sections
    -ffunction-sections             # Enable linker optimizations to improve locality of reference for function sections
    -fno-ident                      # Ignore the #ident directive
    -U_FORTIFY_SOURCE               # Undefine _FORTIFY_SOURCE in case it was previously defined
    -D_FORTIFY_SOURCE=2             # Add memory and string function protection (security feature, related to stack-protector)
    )
  list(APPEND CEF_LINKER_FLAGS
    -fPIC                           # Generate position-independent code for shared libraries
    -pthread                        # Use the pthread library
    -Wl,--disable-new-dtags         # Don't generate new-style dynamic tags in ELF
    -Wl,--fatal-warnings            # Treat warnings as errors
    -Wl,-rpath,.                    # Set rpath so that libraries can be placed next to the executable
    -Wl,-z,noexecstack              # Mark the stack as non-executable (security feature)
    -Wl,-z,now                      # Resolve symbols on program start instead of on first use (security feature)
    -Wl,-z,relro                    # Mark relocation sections as read-only (security feature)
    )
  list(APPEND CEF_LINKER_FLAGS_RELEASE
    -Wl,-O1                         # Enable linker optimizations
    -Wl,--as-needed                 # Only link libraries that export symbols used by the binary
    -Wl,--gc-sections               # Remove unused code resulting from -fdata-sections and -function-sections
    )
  list(APPEND CEF_COMPILER_DEFINES
    _FILE_OFFSET_BITS=64            # Allow the Large File Support (LFS) interface to replace the old interface
    )
  list(APPEND CEF_COMPILER_DEFINES_RELEASE
    NDEBUG                          # Not a debug build
    )

  include(CheckCCompilerFlag)
  include(CheckCXXCompilerFlag)

  CHECK_CXX_COMPILER_FLAG(-Wno-undefined-var-template COMPILER_SUPPORTS_NO_UNDEFINED_VAR_TEMPLATE)
  if(COMPILER_SUPPORTS_NO_UNDEFINED_VAR_TEMPLATE)
    list(APPEND CEF_CXX_COMPILER_FLAGS
      -Wno-undefined-var-template   # Don't warn about potentially uninstantiated static members
      )
  endif()

  CHECK_C_COMPILER_FLAG(-Wno-unused-local-typedefs COMPILER_SUPPORTS_NO_UNUSED_LOCAL_TYPEDEFS)
  if(COMPILER_SUPPORTS_NO_UNUSED_LOCAL_TYPEDEFS)
    list(APPEND CEF_C_COMPILER_FLAGS
      -Wno-unused-local-typedefs  # Don't warn about unused local typedefs
      )
  endif()

  CHECK_CXX_COMPILER_FLAG(-Wno-literal-suffix COMPILER_SUPPORTS_NO_LITERAL_SUFFIX)
  if(COMPILER_SUPPORTS_NO_LITERAL_SUFFIX)
    list(APPEND CEF_CXX_COMPILER_FLAGS
      -Wno-literal-suffix         # Don't warn about invalid suffixes on literals
      )
  endif()

  CHECK_CXX_COMPILER_FLAG(-Wno-narrowing COMPILER_SUPPORTS_NO_NARROWING)
  if(COMPILER_SUPPORTS_NO_NARROWING)
    list(APPEND CEF_CXX_COMPILER_FLAGS
      -Wno-narrowing              # Don't warn about type narrowing
      )
  endif()

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    list(APPEND CEF_CXX_COMPILER_FLAGS
      -Wno-attributes             # The cfi-icall attribute is not supported by the GNU C++ compiler
      )
  endif()

  if(PROJECT_ARCH STREQUAL "x86_64")
    # 64-bit architecture.
    list(APPEND CEF_COMPILER_FLAGS
      -m64
      -march=x86-64
      )
    list(APPEND CEF_LINKER_FLAGS
      -m64
      )
  elseif(PROJECT_ARCH STREQUAL "x86")
    # 32-bit architecture.
    list(APPEND CEF_COMPILER_FLAGS
      -msse2
      -mfpmath=sse
      -mmmx
      -m32
      )
    list(APPEND CEF_LINKER_FLAGS
      -m32
      )
  endif()

  # Standard libraries.
  set(CEF_STANDARD_LIBS
    X11
    )

  # CEF directory paths.
  set(CEF_RESOURCE_DIR        "${_CEF_ROOT}/Resources")
  set(CEF_BINARY_DIR          "${_CEF_ROOT}/${CMAKE_BUILD_TYPE}")
  set(CEF_BINARY_DIR_DEBUG    "${_CEF_ROOT}/Debug")
  set(CEF_BINARY_DIR_RELEASE  "${_CEF_ROOT}/Release")

  # CEF library paths.
  set(CEF_LIB_DEBUG   "${CEF_BINARY_DIR_DEBUG}/libcef.so")
  set(CEF_LIB_RELEASE "${CEF_BINARY_DIR_RELEASE}/libcef.so")

  # List of CEF binary files.
  set(CEF_BINARY_FILES
    chrome-sandbox
    libcef.so
    libEGL.so
    libGLESv2.so
    libvk_swiftshader.so
    libvulkan.so.1
    snapshot_blob.bin
    v8_context_snapshot.bin
    vk_swiftshader_icd.json
    )

  # List of CEF resource files.
  set(CEF_RESOURCE_FILES
    chrome_100_percent.pak
    chrome_200_percent.pak
    resources.pak
    icudtl.dat
    locales
    )

  if(USE_SANDBOX)
    list(APPEND CEF_COMPILER_DEFINES
      CEF_USE_SANDBOX   # Used by apps to test if the sandbox is enabled
      )
  endif()
endif()


#
# Mac OS X configuration.
#

if(OS_MAC)
  # Platform-specific compiler/linker flags.
  # See also Xcode target properties in cef_macros.cmake.
  set(CEF_LIBTYPE SHARED)
  list(APPEND CEF_COMPILER_FLAGS
    -fno-strict-aliasing            # Avoid assumptions regarding non-aliasing of objects of different types
    -fstack-protector               # Protect some vulnerable functions from stack-smashing (security feature)
    -funwind-tables                 # Support stack unwinding for backtrace()
    -fvisibility=hidden             # Give hidden visibility to declarations that are not explicitly marked as visible
    -Wall                           # Enable all warnings
    -Werror                         # Treat warnings as errors
    -Wextra                         # Enable additional warnings
    -Wendif-labels                  # Warn whenever an #else or an #endif is followed by text
    -Wnewline-eof                   # Warn about no newline at end of file
    -Wno-missing-field-initializers # Don't warn about missing field initializers
    -Wno-unused-parameter           # Don't warn about unused parameters
    )
  list(APPEND CEF_C_COMPILER_FLAGS
    -std=c99                        # Use the C99 language standard
    )
  list(APPEND CEF_CXX_COMPILER_FLAGS
    -fno-exceptions                 # Disable exceptions
    -fno-rtti                       # Disable real-time type information
    -fno-threadsafe-statics         # Don't generate thread-safe statics
    -fobjc-call-cxx-cdtors          # Call the constructor/destructor of C++ instance variables in ObjC objects
    -fvisibility-inlines-hidden     # Give hidden visibility to inlined class member functions
    -std=c++17                      # Use the C++17 language standard
    -Wno-narrowing                  # Don't warn about type narrowing
    -Wsign-compare                  # Warn about mixed signed/unsigned type comparisons
    )
  list(APPEND CEF_COMPILER_FLAGS_DEBUG
    -O0                             # Disable optimizations
    -g                              # Generate debug information
    )
  list(APPEND CEF_COMPILER_FLAGS_RELEASE
    -O3                             # Optimize for maximum speed plus a few extras
    )
  list(APPEND CEF_LINKER_FLAGS
    -Wl,-search_paths_first         # Search for static or shared library versions in the same pass
    -Wl,-ObjC                       # Support creation of ObjC static libraries
    -Wl,-pie                        # Generate position-independent code suitable for executables only
    )
  list(APPEND CEF_LINKER_FLAGS_RELEASE
    -Wl,-dead_strip                 # Strip dead code
    )

  include(CheckCXXCompilerFlag)

  CHECK_CXX_COMPILER_FLAG(-Wno-undefined-var-template COMPILER_SUPPORTS_NO_UNDEFINED_VAR_TEMPLATE)
  if(COMPILER_SUPPORTS_NO_UNDEFINED_VAR_TEMPLATE)
    list(APPEND CEF_CXX_COMPILER_FLAGS
      -Wno-undefined-var-template   # Don't warn about potentially uninstantiated static members
      )
  endif()

  # Standard libraries.
  set(CEF_STANDARD_LIBS
    -lpthread
    "-framework Cocoa"
    "-framework AppKit"
    )

  # Find the newest available base SDK.
  execute_process(COMMAND xcode-select --print-path OUTPUT_VARIABLE XCODE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
  foreach(OS_VERSION 10.15 10.14 10.13)
    set(SDK "${XCODE_PATH}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${OS_VERSION}.sdk")
    if(NOT "${CMAKE_OSX_SYSROOT}" AND EXISTS "${SDK}" AND IS_DIRECTORY "${SDK}")
      set(CMAKE_OSX_SYSROOT ${SDK})
    endif()
  endforeach()

  # Target SDK.
  set(CEF_TARGET_SDK               "10.13")
  list(APPEND CEF_COMPILER_FLAGS
    -mmacosx-version-min=${CEF_TARGET_SDK}
  )
  set(CMAKE_OSX_DEPLOYMENT_TARGET  ${CEF_TARGET_SDK})

  # Target architecture.
  if(PROJECT_ARCH STREQUAL "x86_64")
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
  elseif(PROJECT_ARCH STREQUAL "arm64")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
  else()
    set(CMAKE_OSX_ARCHITECTURES "i386")
  endif()

  # Prevent Xcode 11 from doing automatic codesigning.
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")

  # CEF directory paths.
  set(CEF_BINARY_DIR          "${_CEF_ROOT}/$<CONFIGURATION>")
  set(CEF_BINARY_DIR_DEBUG    "${_CEF_ROOT}/Debug")
  set(CEF_BINARY_DIR_RELEASE  "${_CEF_ROOT}/Release")

  if(USE_SANDBOX)
    list(APPEND CEF_COMPILER_DEFINES
      CEF_USE_SANDBOX   # Used by apps to test if the sandbox is enabled
      )

    # CEF sandbox library paths.
    set(CEF_SANDBOX_LIB_DEBUG "${CEF_BINARY_DIR_DEBUG}/cef_sandbox.a")
    set(CEF_SANDBOX_LIB_RELEASE "${CEF_BINARY_DIR_RELEASE}/cef_sandbox.a")
  endif()

  # CEF Helper app suffixes.
  # Format is "<name suffix>:<target suffix>:<plist suffix>".
  set(CEF_HELPER_APP_SUFFIXES
    "::"
    " (GPU):_gpu:.gpu"
    " (Plugin):_plugin:.plugin"
    " (Renderer):_renderer:.renderer"
    )
endif()


#
# Windows configuration.
#

if(OS_WINDOWS)
  if (GEN_NINJA)
    # When using the Ninja generator clear the CMake defaults to avoid excessive
    # console warnings (see issue #2120).
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
  endif()

  if(USE_SANDBOX)
    # Check if the current MSVC version is compatible with the cef_sandbox.lib
    # static library. We require VS2015 or newer.
    if(MSVC_VERSION LESS 1900)
      message(WARNING "CEF sandbox is not compatible with the current MSVC version (${MSVC_VERSION})")
      set(USE_SANDBOX OFF)
    endif()
  endif()

  # Consumers who run into LNK4099 warnings can pass /Z7 instead (see issue #385).
  set(CEF_DEBUG_INFO_FLAG "/Zi" CACHE STRING "Optional flag specifying specific /Z flag to use")

  # Consumers using different runtime types may want to pass different flags
  set(CEF_RUNTIME_LIBRARY_FLAG "/MT" CACHE STRING "Optional flag specifying which runtime to use")
  if (CEF_RUNTIME_LIBRARY_FLAG)
    list(APPEND CEF_COMPILER_FLAGS_DEBUG ${CEF_RUNTIME_LIBRARY_FLAG}d)
    list(APPEND CEF_COMPILER_FLAGS_RELEASE ${CEF_RUNTIME_LIBRARY_FLAG})
  endif()

  # Platform-specific compiler/linker flags.
  set(CEF_LIBTYPE STATIC)
  list(APPEND CEF_COMPILER_FLAGS
    /MP           # Multiprocess compilation
    /Gy           # Enable function-level linking
    /GR-          # Disable run-time type information
    /W4           # Warning level 4
    /WX           # Treat warnings as errors
    /wd4100       # Ignore "unreferenced formal parameter" warning
    /wd4127       # Ignore "conditional expression is constant" warning
    /wd4244       # Ignore "conversion possible loss of data" warning
    /wd4324       # Ignore "structure was padded due to alignment specifier" warning
    /wd4481       # Ignore "nonstandard extension used: override" warning
    /wd4512       # Ignore "assignment operator could not be generated" warning
    /wd4701       # Ignore "potentially uninitialized local variable" warning
    /wd4702       # Ignore "unreachable code" warning
    /wd4996       # Ignore "function or variable may be unsafe" warning
    ${CEF_DEBUG_INFO_FLAG}
    )
  list(APPEND CEF_COMPILER_FLAGS_DEBUG
    /RTC1         # Disable optimizations
    /Od           # Enable basic run-time checks
    )
  list(APPEND CEF_COMPILER_FLAGS_RELEASE
    /O2           # Optimize for maximum speed
    /Ob2          # Inline any suitable function
    /GF           # Enable string pooling
    )
  list(APPEND CEF_CXX_COMPILER_FLAGS
    /std:c++17    # Use the C++17 language standard
    )
  list(APPEND CEF_LINKER_FLAGS_DEBUG
    /DEBUG        # Generate debug information
    )
  list(APPEND CEF_EXE_LINKER_FLAGS
    /MANIFEST:NO        # No default manifest (see ADD_WINDOWS_MANIFEST macro usage)
    /LARGEADDRESSAWARE  # Allow 32-bit processes to access 3GB of RAM
    )
  list(APPEND CEF_COMPILER_DEFINES
    WIN32 _WIN32 _WINDOWS             # Windows platform
    UNICODE _UNICODE                  # Unicode build
    WINVER=0x0601 _WIN32_WINNT=0x601  # Targeting Windows 7
    NOMINMAX                          # Use the standard's templated min/max
    WIN32_LEAN_AND_MEAN               # Exclude less common API declarations
    _HAS_EXCEPTIONS=0                 # Disable exceptions
    )
  list(APPEND CEF_COMPILER_DEFINES_RELEASE
    NDEBUG _NDEBUG                    # Not a debug build
    )

  # Standard libraries.
  set(CEF_STANDARD_LIBS
    comctl32.lib
    gdi32.lib
    rpcrt4.lib
    shlwapi.lib
    ws2_32.lib
    )

  # CEF directory paths.
  set(CEF_RESOURCE_DIR        "${_CEF_ROOT}/Resources")
  set(CEF_BINARY_DIR          "${_CEF_ROOT}/$<CONFIGURATION>")
  set(CEF_BINARY_DIR_DEBUG    "${_CEF_ROOT}/Debug")
  set(CEF_BINARY_DIR_RELEASE  "${_CEF_ROOT}/Release")

  # CEF library paths.
  set(CEF_LIB_DEBUG   "${CEF_BINARY_DIR_DEBUG}/libcef.lib")
  set(CEF_LIB_RELEASE "${CEF_BINARY_DIR_RELEASE}/libcef.lib")

  # List of CEF binary files.
  set(CEF_BINARY_FILES
    chrome_elf.dll
    libcef.dll
    libEGL.dll
    libGLESv2.dll
    snapshot_blob.bin
    v8_context_snapshot.bin
    vk_swiftshader.dll
    vk_swiftshader_icd.json
    vulkan-1.dll
    )

  if(NOT PROJECT_ARCH STREQUAL "arm64")
    list(APPEND CEF_BINARY_FILES
      d3dcompiler_47.dll
      )
  endif()

  # List of CEF resource files.
  set(CEF_RESOURCE_FILES
    chrome_100_percent.pak
    chrome_200_percent.pak
    resources.pak
    icudtl.dat
    locales
    )

  if(USE_SANDBOX)
    list(APPEND CEF_COMPILER_DEFINES
      PSAPI_VERSION=1   # Required by cef_sandbox.lib
      CEF_USE_SANDBOX   # Used by apps to test if the sandbox is enabled
      )
    list(APPEND CEF_COMPILER_DEFINES_DEBUG
      _HAS_ITERATOR_DEBUGGING=0   # Disable iterator debugging
      )

    # Libraries required by cef_sandbox.lib.
    set(CEF_SANDBOX_STANDARD_LIBS
      Advapi32.lib
      dbghelp.lib
      Delayimp.lib
      OleAut32.lib
      PowrProf.lib
      Propsys.lib
      psapi.lib
      SetupAPI.lib
      Shell32.lib
      version.lib
      wbemuuid.lib
      winmm.lib
      )

    # CEF sandbox library paths.
    set(CEF_SANDBOX_LIB_DEBUG "${CEF_BINARY_DIR_DEBUG}/cef_sandbox.lib")
    set(CEF_SANDBOX_LIB_RELEASE "${CEF_BINARY_DIR_RELEASE}/cef_sandbox.lib")
  endif()

  # Configure use of ATL.
  option(USE_ATL "Enable or disable use of ATL." ON)
  if(USE_ATL)
    # Locate the atlmfc directory if it exists. It may be at any depth inside
    # the VC directory. The cl.exe path returned by CMAKE_CXX_COMPILER may also
    # be at different depths depending on the toolchain version
    # (e.g. "VC/bin/cl.exe", "VC/bin/amd64_x86/cl.exe",
    # "VC/Tools/MSVC/14.10.25017/bin/HostX86/x86/cl.exe", etc).
    set(HAS_ATLMFC 0)
    get_filename_component(VC_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    get_filename_component(VC_DIR_NAME ${VC_DIR} NAME)
    while(NOT ${VC_DIR_NAME} STREQUAL "VC")
      get_filename_component(VC_DIR ${VC_DIR} DIRECTORY)
      if(IS_DIRECTORY "${VC_DIR}/atlmfc")
        set(HAS_ATLMFC 1)
        break()
      endif()
      get_filename_component(VC_DIR_NAME ${VC_DIR} NAME)
    endwhile()

    # Determine if the Visual Studio install supports ATL.
    if(NOT HAS_ATLMFC)
      message(WARNING "ATL is not supported by your VC installation.")
      set(USE_ATL OFF)
    endif()
  endif()

  if(USE_ATL)
    list(APPEND CEF_COMPILER_DEFINES
      CEF_USE_ATL   # Used by apps to test if ATL support is enabled
      )
  endif()
endif()
