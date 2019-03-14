# Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from cef_api_hash import cef_api_hash
from cef_parser import *
from file_util import *


def make_api_hash_header(cpp_header_dir):
  # calculate api hashes
  api_hash_calculator = cef_api_hash(cpp_header_dir, verbose=False)
  api_hash = api_hash_calculator.calculate()

  result = get_copyright(full=True) + \
"""#ifndef CEF_INCLUDE_API_HASH_H_
#define CEF_INCLUDE_API_HASH_H_

#include "include/internal/cef_export.h"

// The API hash is created by analyzing CEF header files for C API type
// definitions. The hash value will change when header files are modified in a
// way that may cause binary incompatibility with other builds. The universal
// hash value will change if any platform is affected whereas the platform hash
// values will change only if that particular platform is affected.
#define CEF_API_HASH_UNIVERSAL "$UNIVERSAL$"
#if defined(OS_WIN)
#define CEF_API_HASH_PLATFORM "$WINDOWS$"
#elif defined(OS_MACOSX)
#define CEF_API_HASH_PLATFORM "$MACOSX$"
#elif defined(OS_LINUX)
#define CEF_API_HASH_PLATFORM "$LINUX$"
#endif

#ifdef __cplusplus
extern "C" {
#endif

///
// Returns CEF API hashes for the libcef library. The returned string is owned
// by the library and should not be freed. The |entry| parameter describes which
// hash value will be returned:
// 0 - CEF_API_HASH_PLATFORM
// 1 - CEF_API_HASH_UNIVERSAL
// 2 - CEF_COMMIT_HASH (from cef_version.h)
///
CEF_EXPORT const char* cef_api_hash(int entry);

#ifdef __cplusplus
}
#endif
#endif  // CEF_INCLUDE_API_HASH_H_
"""

  # Substitute hash values for placeholders.
  for platform, value in api_hash.iteritems():
    result = result.replace('$%s$' % platform.upper(), value)

  return result


def write_api_hash_header(header, cpp_header_dir):
  newcontents = make_api_hash_header(cpp_header_dir)
  return (header, newcontents)


# Test the module.
if __name__ == "__main__":
  import sys

  # Verify that the correct number of command-line arguments are provided.
  if len(sys.argv) < 2:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <cppheaderdir>')
    sys.exit()

  # Dump the result to stdout.
  sys.stdout.write(make_api_hash_header(sys.argv[1]))
