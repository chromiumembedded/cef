# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from cef_parser import *

def make_cpptoc_header(header, clsname):
    cls = header.get_class(clsname)
    if cls is None:
        raise Exception('Class does not exist: '+clsname)

    dllside = cls.is_library_side()

    directory = cls.get_file_directory()
    defname = ''
    if not directory is None:
        defname += directory + '_'
    defname += get_capi_name(clsname[3:], False)
    defname = defname.upper()

    capiname = cls.get_capi_name()

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

    # include the headers for this class
    result += '\n#include "include/'+cls.get_file_name()+'"\n' \
              '#include "include/capi/'+cls.get_capi_file_name()+'"\n'

    # include headers for any forward declared classes that are not in the same file
    declares = cls.get_forward_declares()
    for declare in declares:
        dcls = header.get_class(declare)
        if dcls.get_file_name() != cls.get_file_name():
              result += '#include "include/'+dcls.get_file_name()+'"\n' \
                        '#include "include/capi/'+dcls.get_capi_file_name()+'"\n'

    base_class_name = header.get_base_class_name(clsname)
    base_scoped = True if base_class_name == 'CefBaseScoped' else False
    if base_scoped:
        template_file = 'cpptoc_scoped.h'
        template_class = 'CefCppToCScoped'
    else:
        template_file = 'cpptoc_ref_counted.h'
        template_class = 'CefCppToCRefCounted'

    result += '#include "libcef_dll/cpptoc/' + template_file + '"'
    result += '\n\n// Wrap a C++ class with a C structure.\n'

    if dllside:
        result += '// This class may be instantiated and accessed DLL-side only.\n'
    else:
        result += '// This class may be instantiated and accessed wrapper-side only.\n'

    result +=  'class '+clsname+'CppToC\n'+ \
               '    : public ' + template_class + '<'+clsname+'CppToC, '+clsname+', '+capiname+'> {\n'+ \
               ' public:\n'+ \
               '  '+clsname+'CppToC();\n'+ \
               '};\n\n'

    result += '#endif  // CEF_LIBCEF_DLL_CPPTOC_'+defname+'_CPPTOC_H_'

    return wrap_code(result)


def write_cpptoc_header(header, clsname, dir, backup):
    # give the output file the same directory offset as the input file
    cls = header.get_class(clsname)
    dir = os.path.dirname(os.path.join(dir, cls.get_file_name()))
    file = os.path.join(dir, get_capi_name(clsname[3:], False)+'_cpptoc.h')

    if path_exists(file):
        oldcontents = read_file(file)
    else:
        oldcontents = ''

    newcontents = make_cpptoc_header(header, clsname)
    if newcontents != oldcontents:
        if backup and oldcontents != '':
            backup_file(file)
        file_dir = os.path.split(file)[0]
        if not os.path.isdir(file_dir):
            make_dir(file_dir)
        write_file(file, newcontents)
        return True

    return False


# test the module
if __name__ == "__main__":
    import sys

    # verify that the correct number of command-line arguments are provided
    if len(sys.argv) < 3:
        sys.stderr.write('Usage: '+sys.argv[0]+' <infile> <classname>')
        sys.exit()

    # create the header object
    header = obj_header()
    header.add_file(sys.argv[1])

    # dump the result to stdout
    sys.stdout.write(make_cpptoc_header(header, sys.argv[2]))
