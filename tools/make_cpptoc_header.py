# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_parser import *


def make_cpptoc_header(header, clsname):
  cls = header.get_class(clsname)
  if cls is None:
    raise Exception('Class does not exist: ' + clsname)

  dllside = cls.is_library_side()

  directory = cls.get_file_directory()
  defname = ''
  if not directory is None:
    defname += directory + '_'
  defname += get_capi_name(clsname[3:], False)
  defname = defname.upper()

  result = get_copyright()

  result += '#ifndef CEF_LIBCEF_DLL_CPPTOC_'+defname+'_CPPTOC_H_\n'+ \
            '#define CEF_LIBCEF_DLL_CPPTOC_'+defname+'_CPPTOC_H_\n' + \
            '#pragma once\n'

  if dllside:
    result += """
#if !defined(BUILDING_CEF_SHARED)
#error This file can be included DLL-side only
#endif
"""
  else:
    result += """
#if !defined(WRAPPING_CEF_SHARED)
#error This file can be included wrapper-side only
#endif
"""

  with_versions = dllside

  # include the headers for this class
  result += '\n#include "include/'+cls.get_file_name()+'"\n' \
            '#include "include/capi/'+cls.get_capi_file_name(versions=with_versions)+'"\n'

  # include headers for any forward declared classes that are not in the same file
  declares = cls.get_forward_declares()
  for declare in declares:
    dcls = header.get_class(declare)
    if dcls.get_file_name() != cls.get_file_name():
      result += '#include "include/'+dcls.get_file_name()+'"\n' \
                '#include "include/capi/'+dcls.get_capi_file_name(versions=with_versions)+'"\n'

  base_class_name = header.get_base_class_name(clsname)
  base_scoped = True if base_class_name == 'CefBaseScoped' else False
  if base_scoped:
    template_file = 'cpptoc_scoped.h'
    template_class = 'CefCppToCScoped'
  else:
    template_file = 'cpptoc_ref_counted.h'
    template_class = 'CefCppToCRefCounted'

  result += '#include "libcef_dll/cpptoc/' + template_file + '"'

  if with_versions:
    pre = post = ''
  else:
    pre, post = get_version_surround(cls, long=True)
    if len(pre) > 0:
      result += '\n\n' + pre.strip()

  result += '\n\n'

  versions = cls.get_all_versions() if with_versions else (None,)

  for version in versions:
    result += '// Wrap a C++ class with a C structure%s.\n' % \
              ('' if version is None else ' at API version %d' % version)
    if dllside:
      result += '// This class may be instantiated and accessed DLL-side only.\n'
    else:
      result += '// This class may be instantiated and accessed wrapper-side only.\n'

    capiname = cls.get_capi_name(version=version)
    if version is None:
      typename = clsname + 'CppToC'
    else:
      typename = clsname + '_%d_CppToC' % version

    result +=  'class '+typename+'\n'+ \
               '    : public ' + template_class + '<'+typename+', '+clsname+', '+capiname+'> {\n'+ \
               ' public:\n'+ \
               '  '+typename+'();\n'+ \
               '  virtual ~'+typename+'();\n'+ \
               '};\n\n'

  typename = clsname + 'CppToC'
  if len(versions) > 1:
    result += '// Helpers to return objects at the globally configured API version.\n'
    structname = cls.get_capi_name(version=versions[0])
    if base_scoped:
      result += structname + '* ' + typename + '_WrapOwn(CefOwnPtr<' + clsname + '> c);\n' + \
                'std::pair<CefOwnPtr<CefBaseScoped>, '+ structname + '*> ' + typename + '_WrapRaw(CefRawPtr<' + clsname + '> c);\n' + \
                'CefOwnPtr<' + clsname + '> ' + typename + '_UnwrapOwn('+ structname + '* s);\n' + \
                'CefRawPtr<' + clsname + '> ' + typename + '_UnwrapRaw('+ structname + '* s);\n' + \
                'CefBaseScoped* ' + typename + '_GetWrapper('+ structname + '* s);\n\n'
    else:
      result += structname + '* ' + typename + '_Wrap(CefRefPtr<' + clsname + '> c);\n' + \
                'CefRefPtr<' + clsname + '> ' + typename + '_Unwrap('+ structname + '* s);\n\n'
  else:
    if versions[0] is None:
      targetname = clsname + 'CppToC'
      structname = cls.get_capi_name()
    else:
      targetname = clsname + '_%d_CppToC' % versions[0]
      structname = cls.get_capi_name(version=versions[0])
    if base_scoped:
      result += 'constexpr auto ' + typename + '_WrapOwn = ' + targetname + '::WrapOwn;\n' + \
                'constexpr auto ' + typename + '_WrapRaw = ' + targetname + '::WrapRaw;\n' + \
                'constexpr auto ' + typename + '_UnwrapOwn = ' + targetname + '::UnwrapOwn;\n' + \
                'constexpr auto ' + typename + '_UnwrapRaw = ' + targetname + '::UnwrapRaw;\n' + \
                'constexpr auto ' + typename + '_GetWrapper = ' + targetname + '::GetWrapper;\n\n'
    else:
      result += 'constexpr auto ' + typename + '_Wrap = ' + targetname + '::Wrap;\n' + \
                'constexpr auto ' + typename + '_Unwrap = ' + targetname + '::Unwrap;\n\n'

  if base_scoped:
    result += 'inline ' + structname + '* ' + typename + '_WrapRawAndRelease(CefRawPtr<' + clsname + '> c) {\n' + \
              '  auto [ownerPtr, structPtr] = ' + typename + '_WrapRaw(c);\n' + \
              '  ownerPtr.release();\n' + \
              '  return structPtr;\n' + \
              '}\n\n'

  if len(post) > 0:
    result += post + '\n'

  result += '#endif  // CEF_LIBCEF_DLL_CPPTOC_' + defname + '_CPPTOC_H_'

  return result


def write_cpptoc_header(header, clsname, dir):
  # give the output file the same directory offset as the input file
  cls = header.get_class(clsname)
  dir = os.path.dirname(os.path.join(dir, cls.get_file_name()))
  file = os.path.join(dir, get_capi_name(clsname[3:], False) + '_cpptoc.h')

  newcontents = make_cpptoc_header(header, clsname)
  return (file, newcontents)


# test the module
if __name__ == "__main__":
  import sys

  # verify that the correct number of command-line arguments are provided
  if len(sys.argv) < 3:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <infile> <classname>')
    sys.exit()

  # create the header object
  header = obj_header()
  header.add_file(sys.argv[1])

  # dump the result to stdout
  sys.stdout.write(make_cpptoc_header(header, sys.argv[2]))
