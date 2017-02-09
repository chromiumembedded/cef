# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from cef_parser import *

def make_function_body_block(cls):
    impl = '  // '+cls.get_name()+' methods.\n';

    funcs = cls.get_virtual_funcs()
    for func in funcs:
        impl += '  '+func.get_cpp_proto()
        if cls.is_client_side():
            impl += ' override;\n'
        else:
            impl += ' OVERRIDE;\n'

    return impl

def make_function_body(header, cls):
    impl = make_function_body_block(cls)

    cur_cls = cls
    while True:
        parent_name = cur_cls.get_parent_name()
        if is_base_class(parent_name):
            break
        else:
            parent_cls = header.get_class(parent_name)
            if parent_cls is None:
                raise Exception('Class does not exist: '+parent_name)
            if len(impl) > 0:
                impl += '\n'
            impl += make_function_body_block(parent_cls)
        cur_cls = header.get_class(parent_name)

    return impl

def make_ctocpp_header(header, clsname):
    cls = header.get_class(clsname)
    if cls is None:
        raise Exception('Class does not exist: '+clsname)

    clientside = cls.is_client_side()

    directory = cls.get_file_directory()
    defname = ''
    if not directory is None:
        defname += directory + '_'
    defname += get_capi_name(clsname[3:], False)
    defname = defname.upper()

    capiname = cls.get_capi_name()

    result = get_copyright()

    result += '#ifndef CEF_LIBCEF_DLL_CTOCPP_'+defname+'_CTOCPP_H_\n'+ \
              '#define CEF_LIBCEF_DLL_CTOCPP_'+defname+'_CTOCPP_H_\n' + \
              '#pragma once\n'

    if clientside:
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

    # build the function body
    func_body = make_function_body(header, cls)

    # include standard headers
    if func_body.find('std::map') > 0 or func_body.find('std::multimap') > 0:
        result += '\n#include <map>'
    if func_body.find('std::vector') > 0:
        result += '\n#include <vector>'

    # include the headers for this class
    result += '\n#include "include/'+cls.get_file_name()+'"'+ \
              '\n#include "include/capi/'+cls.get_capi_file_name()+'"\n'

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
        template_file = 'ctocpp_scoped.h'
        template_class = 'CefCToCppScoped'
    else:
        template_file = 'ctocpp_ref_counted.h'
        template_class = 'CefCToCppRefCounted'

    result += '#include "libcef_dll/ctocpp/' + template_file + '"'
    result += '\n\n// Wrap a C structure with a C++ class.\n'

    if clientside:
        result += '// This class may be instantiated and accessed DLL-side only.\n'
    else:
        result += '// This class may be instantiated and accessed wrapper-side only.\n'

    result +=   'class '+clsname+'CToCpp\n'+ \
                '    : public ' + template_class + '<'+clsname+'CToCpp, '+clsname+', '+capiname+'> {\n'+ \
                ' public:\n'+ \
                '  '+clsname+'CToCpp();\n\n'

    result +=   func_body
    result +=   '};\n\n'

    result += '#endif  // CEF_LIBCEF_DLL_CTOCPP_'+defname+'_CTOCPP_H_'

    return wrap_code(result)


def write_ctocpp_header(header, clsname, dir, backup):
    # give the output file the same directory offset as the input file
    cls = header.get_class(clsname)
    dir = os.path.dirname(os.path.join(dir, cls.get_file_name()))
    file = os.path.join(dir, get_capi_name(clsname[3:], False)+'_ctocpp.h')

    if path_exists(file):
        oldcontents = read_file(file)
    else:
        oldcontents = ''

    newcontents = make_ctocpp_header(header, clsname)
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
    sys.stdout.write(make_ctocpp_header(header, sys.argv[2]))
