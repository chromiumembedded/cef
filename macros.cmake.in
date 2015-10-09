# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

#
# Shared macros.
#

# Append platform specific sources to a list of sources.
macro(APPEND_PLATFORM_SOURCES name_of_list)
  if(OS_LINUX AND ${name_of_list}_LINUX)
    list(APPEND ${name_of_list} ${${name_of_list}_LINUX})
  endif()
  if(OS_POSIX AND ${name_of_list}_POSIX)
    list(APPEND ${name_of_list} ${${name_of_list}_POSIX})
  endif()
  if(OS_WINDOWS AND ${name_of_list}_WINDOWS)
    list(APPEND ${name_of_list} ${${name_of_list}_WINDOWS})
  endif()
  if(OS_MACOSX AND ${name_of_list}_MACOSX)
    list(APPEND ${name_of_list} ${${name_of_list}_MACOSX})
  endif()
endmacro()

# Add a logical target that can be used to link the specified libraries into an
# executable target.
macro(ADD_LOGICAL_TARGET target debug_lib release_lib)
  add_library(${target} ${CEF_LIBTYPE} IMPORTED)
  set_target_properties(${target} PROPERTIES
    IMPORTED_LOCATION "${release_lib}"
    IMPORTED_LOCATION_DEBUG "${debug_lib}"
    IMPORTED_LOCATION_RELEASE "${release_lib}"
    )
endmacro()

# Determine the target output directory based on platform and generator.
macro(SET_CEF_TARGET_OUT_DIR)
  if(${CMAKE_GENERATOR} STREQUAL "Ninja")
    # Ninja does not create a subdirectory named after the configuration.
    set(CEF_TARGET_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  elseif(OS_LINUX)
    set(CEF_TARGET_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
  else()
    set(CEF_TARGET_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>")
  endif()
endmacro()

# Copy a list of files from one directory to another. Relative files paths are maintained.
macro(COPY_FILES target file_list source_dir target_dir)
  foreach(FILENAME ${file_list})
    set(source_file ${source_dir}/${FILENAME})
    set(target_file ${target_dir}/${FILENAME})
    if(IS_DIRECTORY ${source_file})
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_file}" "${target_file}"
        VERBATIM
        )
    else()
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${source_file}" "${target_file}"
        VERBATIM
        )
    endif()
  endforeach()
endmacro()

# Rename a directory replacing the target if it already exists.
macro(RENAME_DIRECTORY target source_dir target_dir)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    # Remove the target directory if it already exists.
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${target_dir}"
    # Rename the source directory to target directory.
    COMMAND ${CMAKE_COMMAND} -E rename "${source_dir}" "${target_dir}"
    VERBATIM
    )
endmacro()


#
# Linux macros.
#

if(OS_LINUX)

# Use pkg-config to find Linux libraries and update compiler/linker variables.
macro(FIND_LINUX_LIBRARIES libraries)
  # Read pkg-config info into variables.
  execute_process(COMMAND pkg-config --cflags ${libraries} OUTPUT_VARIABLE FLL_CFLAGS)
  execute_process(COMMAND pkg-config --libs-only-L --libs-only-other ${libraries} OUTPUT_VARIABLE FLL_LDFLAGS)
  execute_process(COMMAND pkg-config --libs-only-l ${libraries} OUTPUT_VARIABLE FLL_LIBS)

  # Strip leading and trailing whitepspace.
  STRING(STRIP "${FLL_CFLAGS}"  FLL_CFLAGS)
  STRING(STRIP "${FLL_LDFLAGS}" FLL_LDFLAGS)
  STRING(STRIP "${FLL_LIBS}"    FLL_LIBS)

  # Update the variables.
  set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} ${FLL_CFLAGS}")
  set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} ${FLL_CFLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} ${FLL_LDFLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FLL_LDFLAGS}")
  set(CEF_STANDARD_LIBS         "${CEF_STANDARD_LIBS} ${FLL_LIBS}")
endmacro()

# Set SUID permissions on the specified executable.
macro(SET_LINUX_SUID_PERMISSIONS target executable)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "*** Run the following command manually to set SUID permissions ***"
    COMMAND ${CMAKE_COMMAND} -E echo "EXE=\"${executable}\" && sudo -- chown root:root $EXE && sudo -- chmod 4755 $EXE"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    VERBATIM
    )
endmacro()

endif(OS_LINUX)


#
# Mac OS X macros.
#

if(OS_MACOSX)

# Set Xcode target properties.
function(SET_XCODE_TARGET_PROPERTIES target)
  set_target_properties(${target} PROPERTIES
    XCODE_ATTRIBUTE_ALWAYS_SEARCH_USER_PATHS                    NO
    XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD                 "gnu++11"   # -std=gnu++11
    XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME                     NO          # -fno-objc-link-runtime
    XCODE_ATTRIBUTE_CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS  YES         # -Wobjc-missing-property-synthesis
    XCODE_ATTRIBUTE_COPY_PHASE_STRIP                            NO
    XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING[variant=Release]        YES         # -Wl,-dead_strip
    XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD                     "c99"       # -std=c99
    XCODE_ATTRIBUTE_GCC_CW_ASM_SYNTAX                           NO          # No -fasm-blocks
    XCODE_ATTRIBUTE_GCC_DYNAMIC_NO_PIC                          NO
    XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS                   NO          # -fno-exceptions
    XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI                         NO          # -fno-rtti
    XCODE_ATTRIBUTE_GCC_ENABLE_PASCAL_STRINGS                   NO          # No -mpascal-strings
    XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN              YES         # -fvisibility-inlines-hidden
    XCODE_ATTRIBUTE_GCC_OBJC_CALL_CXX_CDTORS                    YES         # -fobjc-call-cxx-cdtors
    XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN                  YES         # -fvisibility=hidden
    XCODE_ATTRIBUTE_GCC_THREADSAFE_STATICS                      NO          # -fno-threadsafe-statics
    XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS                YES         # -Werror
    XCODE_ATTRIBUTE_GCC_VERSION                                 "com.apple.compilers.llvm.clang.1_0"
    XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_NEWLINE              YES         # -Wnewline-eof
    XCODE_ATTRIBUTE_USE_HEADERMAP                               NO
    OSX_ARCHITECTURES_DEBUG                                     "${CMAKE_OSX_ARCHITECTURES}"
    OSX_ARCHITECTURES_RELEASE                                   "${CMAKE_OSX_ARCHITECTURES}"
    )
endfunction()

# Override default add_library function.
function(add_library name)
  _add_library(${name} ${ARGN})
  SET_XCODE_TARGET_PROPERTIES(${name})
endfunction()

# Override default add_executable function.
function(add_executable name)
  _add_executable(${name} ${ARGN})
  SET_XCODE_TARGET_PROPERTIES(${name})
endfunction()

# Fix the framework link in the helper executable.
macro(FIX_MACOSX_HELPER_FRAMEWORK_LINK target app_path)
  add_custom_command(TARGET ${target}
    POST_BUILD
    COMMAND install_name_tool -change "@executable_path/Chromium Embedded Framework"
            "@executable_path/../../../../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
            "${app_path}/Contents/MacOS/${target}"
    VERBATIM
    )
endmacro()

# Fix the framework link in the main executable.
macro(FIX_MACOSX_MAIN_FRAMEWORK_LINK target app_path)
  add_custom_command(TARGET ${target}
    POST_BUILD
    COMMAND install_name_tool -change "@executable_path/Chromium Embedded Framework"
            "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
            "${app_path}/Contents/MacOS/${target}"
    VERBATIM
    )
endmacro()

# Manually process and copy over resource files.
macro(COPY_MACOSX_RESOURCES resource_list prefix_list target source_dir app_path)
  foreach(FILENAME ${resource_list})
    # Remove one or more prefixes from the source paths.
    set(TARGET_FILENAME "${FILENAME}")
    foreach(PREFIX ${prefix_list})
      string(REGEX REPLACE "^.*${PREFIX}" "" TARGET_FILENAME ${TARGET_FILENAME})
    endforeach()

    # Determine the absolute source and target paths.
    set(TARGET_PATH "${app_path}/Contents/Resources/${TARGET_FILENAME}")
    if(IS_ABSOLUTE ${FILENAME})
      set(SOURCE_PATH ${FILENAME})
    else()
      set(SOURCE_PATH "${source_dir}/${FILENAME}")
    endif()

    if(${FILENAME} MATCHES ".xib$")
      # Change the target file extension.
      string(REGEX REPLACE ".xib$" ".nib" TARGET_PATH ${TARGET_PATH})

      get_filename_component(TARGET_DIRECTORY ${TARGET_PATH} PATH)
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        # Create the target directory.
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TARGET_DIRECTORY}"
        # Compile the XIB file to a NIB.
        COMMAND /usr/bin/ibtool --output-format binary1 --compile "${TARGET_PATH}" "${SOURCE_PATH}"
        VERBATIM
        )
    elseif(NOT ${TARGET_FILENAME} STREQUAL "Info.plist")
      # Copy the file as-is.
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${SOURCE_PATH}" "${TARGET_PATH}"
        VERBATIM
        )
    endif()
  endforeach()
endmacro()

endif(OS_MACOSX)


#
# Windows macros.
#

if(OS_WINDOWS)

# Add custom manifest files to an executable target.
macro(ADD_WINDOWS_MANIFEST manifest_path target)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "mt.exe" -nologo
            -manifest \"${manifest_path}/${target}.exe.manifest\" \"${manifest_path}/compatibility.manifest\"
            -outputresource:"${CEF_TARGET_OUT_DIR}/${target}.exe"\;\#1
    COMMENT "Adding manifest..." 
    )
endmacro()

endif(OS_WINDOWS)
