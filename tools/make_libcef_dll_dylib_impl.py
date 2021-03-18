# Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_parser import *
from file_util import *
import os

# Other headers that export C API functions.
OTHER_HEADERS = [
    'cef_api_hash.h',
    'cef_version.h',
    'internal/cef_logging_internal.h',
    'internal/cef_string_list.h',
    'internal/cef_string_map.h',
    'internal/cef_string_multimap.h',
    'internal/cef_string_types.h',
    'internal/cef_thread_internal.h',
    'internal/cef_time.h',
    'internal/cef_trace_event_internal.h',
]


def make_libcef_dll_dylib_impl_parts(name, retval, args):
  # Split arguments into types and names.
  arg_types = ''
  arg_names = ''
  for arg in args:
    if len(arg_types) > 0:
      arg_types += ', '
      arg_names += ', '
    pos = arg.rfind(' ')
    arg_types += arg[0:pos]
    arg_names += arg[pos + 1:]

  typedef = 'typedef %s (*%s_ptr)(%s);\n' % (retval, name, arg_types)

  declare = '%s_ptr %s;\n' % (name, name)

  init = '  INIT_ENTRY(%s);' % name

  impl = """NO_SANITIZE("cfi-icall") %s %s(%s) {
  %sg_libcef_pointers.%s(%s);
}

""" % (retval, name, ', '.join(args), 'return '
       if retval != 'void' else '', name, arg_names)

  return (typedef, declare, init, impl)


def make_libcef_dll_dylib_impl_func(func):
  name = func.get_capi_name()
  parts = func.get_capi_parts([])
  retval = parts['retval']
  args = parts['args']
  return make_libcef_dll_dylib_impl_parts(name, retval, args)


def make_libcef_dll_dylib_impl(header):
  filenames = []
  includes = []
  ptr_typedef = ''
  ptr_declare = ''
  ptr_init = ''
  ptr_impl = ''

  # Include required headers for global functions.
  for func in header.get_funcs():
    typedef, declare, init, impl = make_libcef_dll_dylib_impl_func(func)
    ptr_typedef += typedef
    ptr_declare += declare
    ptr_init += init
    ptr_impl += impl

    filename = func.get_file_name()
    if not filename in filenames:
      includes.append('#include "include/capi/%s"' % func.get_capi_file_name())
      filenames.append(filename)

  # Include required headers for static class functions.
  allclasses = header.get_classes()
  for cls in allclasses:
    funcs = cls.get_static_funcs()
    for func in funcs:
      typedef, declare, init, impl = make_libcef_dll_dylib_impl_func(func)
      ptr_typedef += typedef
      ptr_declare += declare
      ptr_init += init
      ptr_impl += impl

    if len(funcs) > 0:
      filename = cls.get_file_name()
      if not filename in filenames:
        includes.append('#include "include/capi/%s"' % cls.get_capi_file_name())
        filenames.append(filename)

  # Parse other headers.
  root_directory = header.get_root_directory()
  for other in OTHER_HEADERS:
    path = os.path.join(root_directory, other)
    content = read_file(path)
    funcs = get_function_impls(content, 'CEF_EXPORT', False)
    for func in funcs:
      typedef, declare, init, impl = make_libcef_dll_dylib_impl_parts(
          func['name'], func['retval'], func['args'])
      ptr_typedef += typedef
      ptr_declare += declare
      ptr_init += init
      ptr_impl += impl

    includes.append('#include "include/%s"' % other)

  # Build the final output.
  result = get_copyright() + """

#include <stdio.h>
#include <system_error>

""" + "\n".join(sorted(includes)) + """
#include "include/wrapper/cef_library_loader.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// GLOBAL WRAPPER FUNCTIONS - Do not edit by hand.

namespace {

#if defined(OS_WIN)
inline std::string get_last_error_message() {
  DWORD error_code = ::GetLastError();
  return std::system_category().message(error_code);
}
#endif

void* g_libcef_handle = nullptr;

void* libcef_get_ptr(const CEF_PATH_CHAR_T* path, const char* name) {

#if defined(OS_WIN)
  void* ptr = ::GetProcAddress((HMODULE)g_libcef_handle, name);
  if (!ptr) {
    fprintf(stderr, "GetProcAddress %ls: %s\\n",
            path, get_last_error_message().c_str());
  }
#else
  void* ptr = dlsym(g_libcef_handle, name);
  if (!ptr) {
    fprintf(stderr, "dlsym %s: %s\\n", path, dlerror());
  }
#endif

  return ptr;
}

""" + ptr_typedef + """

struct libcef_pointers {
""" + ptr_declare + """
} g_libcef_pointers = {0};

#define INIT_ENTRY(name) \
  g_libcef_pointers.name = (name##_ptr)libcef_get_ptr(path, #name); \
  if (!g_libcef_pointers.name) { \
    return 0; \
  }

int libcef_init_pointers(const CEF_PATH_CHAR_T* path) {
""" + ptr_init + """
  return 1;
}

}  // namespace

int cef_load_library(const CEF_PATH_CHAR_T* path) {
  if (g_libcef_handle)
    return 0;

#if defined(OS_WIN)
  g_libcef_handle = ::LoadLibraryExW(path, nullptr,
                                     LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                     LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!g_libcef_handle) {
    fprintf(stderr, "LoadLibraryExW %ls: %s\\n",
            path, get_last_error_message().c_str());
    return 0;
  }
#else
  g_libcef_handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!g_libcef_handle) {
    fprintf(stderr, "dlopen %s: %s\\n", path, dlerror());
    return 0;
  }
#endif

  if (!libcef_init_pointers(path)) {
    cef_unload_library();
    return 0;
  }

  return 1;
}

int cef_unload_library() {
  int result = 0;
  if (g_libcef_handle) {

#if defined(OS_WIN)
    result = FreeLibrary((HMODULE)g_libcef_handle);
    if (!result) {
      fprintf(stderr, "FreeLibrary failed: %s\\n",
              get_last_error_message().c_str());
    }
#else
    result = !dlclose(g_libcef_handle);
    if (!result) {
      fprintf(stderr, "dlclose: %s\\n", dlerror());
    }
#endif

    g_libcef_handle = nullptr;
  }
  return result;
}

""" + ptr_impl
  return result


def write_libcef_dll_dylib_impl(header, file):
  newcontents = make_libcef_dll_dylib_impl(header)
  return (file, newcontents)


# Test the module.
if __name__ == "__main__":
  import sys

  # Verify that the correct number of command-line arguments are provided.
  if len(sys.argv) < 2:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <cpp_header_dir>\n')
    sys.exit()

  cpp_header_dir = sys.argv[1]

  # Create the header object. Should match the logic in translator.py.
  header = obj_header()
  header.set_root_directory(cpp_header_dir)
  excluded_files = ['cef_api_hash.h', 'cef_application_mac.h', 'cef_version.h']
  header.add_directory(cpp_header_dir, excluded_files)
  header.add_directory(os.path.join(cpp_header_dir, 'test'))
  header.add_directory(os.path.join(cpp_header_dir, 'views'))

  # Dump the result to stdout.
  sys.stdout.write(make_libcef_dll_dylib_impl(header))
