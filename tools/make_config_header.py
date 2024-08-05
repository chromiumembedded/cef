# Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_parser import get_copyright
from file_util import *
import sys


def make_config_header(gn_config):
  """ Creates the header file contents for the cef build configuration. """

  if not path_exists(gn_config):
    raise Exception('File ' + gn_config + ' does not exist.')

  defines = []

  lines = read_file(gn_config).split("\n")

  # The following #defines are used in cef/include/ headers and CEF client-side code.
  # CEF library-side code will get these #defines from include/base/cef_build.h so
  # any changes must also be reflected there.

  # All Linux builds use Ozone, and the X11 platform is enabled by default.
  # Check if the config is explicitly disabling it.
  if sys.platform.startswith('linux') and \
      not 'ozone_platform_x11=false' in lines:
    defines.append('#define CEF_X11 1')

  # If the V8 sandbox is not disabled explicitly and
  # the target_cpu is arm64 or x64 (see v8_enable_pointer_compression)
  # add CEF_V8_ENABLE_SANDBOX define used in cef_message_router.h
  if not 'v8_enable_sandbox=false' in lines and \
          ('target_cpu="arm64"' in lines or 'target_cpu="x64"' in lines):
    defines.append('#define CEF_V8_ENABLE_SANDBOX 1')

  result = get_copyright(full=True, translator=False) + \
"""//
// ---------------------------------------------------------------------------
//
// This file is generated by the make_config_header.py tool.
//

#ifndef CEF_INCLUDE_CEF_CONFIG_H_
#define CEF_INCLUDE_CEF_CONFIG_H_

$DEFINES$

#endif  // CEF_INCLUDE_CEF_CONFIG_H_
"""

  result = result.replace('$DEFINES$', "\n".join(defines))
  return result


def write_config_header(output, gn_config):
  output = os.path.abspath(output)
  result = make_config_header(gn_config)
  return write_file_if_changed(output, result)


def main(argv):
  if len(argv) < 3:
    print(("Usage:\n  %s <output_header_file> <input_args_gn_file>" % argv[0]))
    sys.exit(-1)
  write_config_header(argv[1], argv[2])


if '__main__' == __name__:
  main(sys.argv)
