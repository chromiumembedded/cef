# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_parser import *


def make_capi_global_funcs(funcs, defined_names, translate_map, indent):
  result = ''
  for func in funcs:
    comment = func.get_comment()
    pre, post = get_version_surround(func)
    result += '\n' + pre
    if len(comment) > 0:
      result += format_comment(comment, indent, translate_map)
    if func.get_retval().get_type().is_result_string():
      result += indent + '// The resulting string must be freed by calling cef_string_userfree_free().\n'
    result += indent + 'CEF_EXPORT ' + func.get_capi_proto(
        defined_names) + ';\n' + post
  return result


def make_capi_member_funcs(funcs, defined_names, translate_map, indent):
  result = ''
  for func in funcs:
    comment = func.get_comment()
    pre, post = get_version_surround(func)
    result += '\n' + pre
    if len(comment) > 0:
      result += format_comment(comment, indent, translate_map)
    if func.get_retval().get_type().is_result_string():
      result += indent + '// The resulting string must be freed by calling cef_string_userfree_free().\n'
    parts = func.get_capi_parts()
    result += indent + parts['retval'] + ' (CEF_CALLBACK *' + parts['name'] + \
              ')(' + ', '.join(parts['args']) + ');\n'
    if len(post) > 0 and func.has_version_removed():
      if func.has_version_added():
        result += '#elif ' + get_version_check({
            'added': func.get_attrib('removed')
        })
      else:
        result += '#else'
      result += '\n' + indent + 'uintptr_t ' + parts['name'] + '_removed;\n'
    result += post
  return result


def make_capi_header(header, filename):
  # structure names that have already been defined
  defined_names = header.get_defined_structs()

  # map of strings that will be changed in C++ comments
  translate_map = header.get_capi_translations()

  # header string
  result = get_copyright(full=True, translator=False) + \
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

#if defined(BUILDING_CEF_SHARED)
#error This file cannot be included DLL-side
#endif

"""

  # Protect against incorrect use of test headers.
  if filename.startswith('test/'):
    result += \
"""#if !defined(WRAPPING_CEF_SHARED) && !defined(UNIT_TEST)
#error This file can be included for unit tests only
#endif

"""

  classes = header.get_classes(filename)

  # identify all includes and forward declarations
  translated_includes = set([])
  internal_includes = set([])
  all_declares = {}
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
      capi_name = declare_cls.get_capi_name()
      if not capi_name in all_declares:
        all_declares[capi_name] = declare_cls.get_version_check() \
                                  if declare_cls.has_version() else None

  # output translated includes
  if len(translated_includes) > 0:
    sorted_includes = sorted(translated_includes)
    for include in sorted_includes:
      result += '#include "include/capi/' + include + '_capi.h"\n'
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
    sorted_declares = sorted(all_declares.keys())
    for declare in sorted_declares:
      cls_version_check = all_declares[declare]
      if not cls_version_check is None:
        result += '#if ' + cls_version_check + '\n'
      result += 'struct _' + declare + ';\n'
      if not cls_version_check is None:
        result += '#endif\n'

  # output classes
  for cls in classes:
    pre, post = get_version_surround(cls, long=True)
    if len(pre) > 0:
      result += '\n' + pre

    comment = cls.get_comment()
    add_comment = []
    if comment[-1] != '':
      add_comment.append('')
    add_comment.append('NOTE: This struct is allocated %s-side.' % \
                       ('client' if cls.is_client_side() else 'DLL'))
    add_comment.append('')

    # virtual functions are inside the structure
    classname = cls.get_capi_name()
    result += '\n' + format_comment(comment + add_comment, '', translate_map)
    result += 'typedef struct _'+classname+' {\n'+\
              '  ///\n'+\
              '  /// Base structure.\n'+\
              '  ///\n'+\
              '  '+cls.get_parent_capi_name()+' base;\n'
    funcs = cls.get_virtual_funcs(version_order=True)
    result += make_capi_member_funcs(funcs, defined_names, translate_map, '  ')
    result += '} ' + classname + ';\n\n'

    defined_names.append(cls.get_capi_name())

    # static functions become global
    funcs = cls.get_static_funcs()
    if len(funcs) > 0:
      result += make_capi_global_funcs(funcs, defined_names, translate_map,
                                       '') + '\n'

    result += post

  # output global functions
  funcs = header.get_funcs(filename)
  if len(funcs) > 0:
    result += make_capi_global_funcs(funcs, defined_names, translate_map, '')

  # footer string
  result += \
"""
#ifdef __cplusplus
}
#endif

#endif  // $GUARD$
"""

  # add the guard string
  guard = 'CEF_INCLUDE_CAPI_' + \
      filename.replace('/', '_').replace('.', '_capi_').upper() + '_'
  result = result.replace('$GUARD$', guard)

  return result


def write_capi_header(header, header_dir, filename):
  file = get_capi_file_name(os.path.join(header_dir, filename))
  newcontents = make_capi_header(header, filename)
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
  sys.stdout.write(make_capi_header(header, filename))
