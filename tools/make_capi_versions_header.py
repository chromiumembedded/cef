# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_parser import *
import functools


def _version_finder(header, name):
  cls = header.get_capi_class(name)
  if not cls is None:
    return cls.get_capi_name(first_version=True)
  return name


def make_capi_global_funcs(version, version_finder, funcs, defined_names):
  result = ''
  for func in funcs:
    result += 'CEF_EXPORT ' + func.get_capi_proto(
        defined_names, version=version, version_finder=version_finder) + ';\n'
  return result


def make_capi_member_funcs(version, version_finder, funcs, defined_names,
                           translate_map, indent):
  result = ''
  for func in funcs:
    parts = func.get_capi_parts(version=version, version_finder=version_finder)
    if func.removed_at_version(version):
      result += indent + 'uintptr_t ' + parts['name'] + '_removed;\n'
    else:
      result += indent + parts['retval'] + ' (CEF_CALLBACK *' + parts['name'] + \
                ')(' + ', '.join(parts['args']) + ');\n'
  return result


def make_capi_versions_header(header, filename):
  # structure names that have already been defined
  defined_names = header.get_defined_structs()

  # map of strings that will be changed in C++ comments
  translate_map = header.get_capi_translations()

  # header string
  result = get_copyright(full=False, translator=False) + \
"""//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool and should not edited
// by hand. See the translator.README.txt file in the tools directory for
// more information.
//
// $hash=$$HASH$$$
//

#ifndef $GUARD$
#define $GUARD$
#pragma once

#if !defined(BUILDING_CEF_SHARED)
#error This file can be included DLL-side only
#endif

"""

  classes = header.get_classes(filename)

  # identify all includes and forward declarations
  translated_includes = set()
  internal_includes = set()
  all_declares = set()
  for cls in classes:
    includes = cls.get_includes()
    for include in includes:
      if include.startswith('base/'):
        # base/ headers are C++. They should not be included by
        # translated CEF API headers.
        raise Exception('Disallowed include of %s.h from %s' % (include,
                                                                filename))
      elif include.startswith('internal/') or include == 'cef_api_hash':
        # internal/ headers may be C or C++. Include them as-is.
        internal_includes.add(include)
      else:
        translated_includes.add(include)
    declares = cls.get_forward_declares()
    for declare in declares:
      declare_cls = header.get_class(declare)
      if declare_cls is None:
        raise Exception('Unknown class: %s' % declare)
      for version in declare_cls.get_all_versions():
        all_declares.add(declare_cls.get_capi_name(version=version))

  # output translated includes
  if len(translated_includes) > 0:
    sorted_includes = sorted(translated_includes)
    for include in sorted_includes:
      suffix = '_versions' if not include in ('cef_base',) else ''
      result += '#include "include/capi/' + include + '_capi' + suffix + '.h"\n'
  else:
    result += '#include "include/capi/cef_base_capi.h"\n'

  # output internal includes
  if len(internal_includes) > 0:
    sorted_includes = sorted(internal_includes)
    for include in sorted_includes:
      result += '#include "include/' + include + '.h"\n'

  result += \
"""
#ifdef __cplusplus
extern "C" {
#endif

"""

  # output forward declarations
  if bool(all_declares):
    for declare in sorted(all_declares):
      result += 'struct _' + declare + ';\n'
    result += '\n'

  version_finder = functools.partial(_version_finder, header)

  # output classes
  for cls in classes:
    for version in cls.get_all_versions():
      # virtual functions are inside the structure
      classname = cls.get_capi_name(version=version)
      result += 'typedef struct _'+classname+' {\n'+\
                '  '+cls.get_parent_capi_name(version=version)+' base;\n'
      funcs = cls.get_virtual_funcs(version_order=True, version=version)
      result += make_capi_member_funcs(version, version_finder, funcs,
                                       defined_names, translate_map, '  ')
      result += '} ' + classname + ';\n\n'

      defined_names.append(classname)

    # static functions become global
    funcs = cls.get_static_funcs()
    if len(funcs) > 0:
      result += make_capi_global_funcs(version, version_finder, funcs,
                                       defined_names) + '\n'

  # output global functions
  funcs = header.get_funcs(filename)
  if len(funcs) > 0:
    result += make_capi_global_funcs(None, version_finder, funcs,
                                     defined_names) + '\n'

  # footer string
  result += \
"""#ifdef __cplusplus
}
#endif

#endif  // $GUARD$
"""

  # add the guard string
  guard = 'CEF_INCLUDE_CAPI_' + \
      filename.replace('/', '_').replace('.', '_capi_versions_').upper() + '_'
  result = result.replace('$GUARD$', guard)

  return result


def write_capi_versions_header(header, header_dir, filename):
  file = get_capi_file_name(os.path.join(header_dir, filename), versions=True)
  newcontents = make_capi_versions_header(header, filename)
  return (file, newcontents)


# test the module
if __name__ == "__main__":
  import sys

  # verify that the correct number of command-line arguments are provided
  if len(sys.argv) < 2:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <infile>\n')
    sys.exit()

  # create the header object
  header = obj_header()
  header.add_file(sys.argv[1])

  # dump the result to stdout
  filename = os.path.split(sys.argv[1])[1]
  sys.stdout.write(make_capi_versions_header(header, filename))
