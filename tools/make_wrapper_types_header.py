# Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from cef_parser import *

def make_wrapper_types_header(header):
    result = get_copyright()

    result += '#ifndef CEF_LIBCEF_DLL_WRAPPER_TYPES_H_\n'+ \
              '#define CEF_LIBCEF_DLL_WRAPPER_TYPES_H_\n' + \
              '#pragma once\n\n' + \
              'enum CefWrapperType {\n' + \
              '  WT_BASE_REF_COUNTED = 1,\n' + \
              '  WT_BASE_SCOPED,\n'

    clsnames = sorted(header.get_class_names())
    for clsname in clsnames:
        result += '  '+get_wrapper_type_enum(clsname)+',\n'

    result += '};\n\n' + \
              '#endif  // CEF_LIBCEF_DLL_WRAPPER_TYPES_H_'

    return wrap_code(result)


def write_wrapper_types_header(header, file, backup):
    if path_exists(file):
        oldcontents = read_file(file)
    else:
        oldcontents = ''

    newcontents = make_wrapper_types_header(header)
    if newcontents != oldcontents:
        if backup and oldcontents != '':
            backup_file(file)
        write_file(file, newcontents)
        return True

    return False


# test the module
if __name__ == "__main__":
    import sys

    # verify that the correct number of command-line arguments are provided
    if len(sys.argv) < 2:
        sys.stderr.write('Usage: '+sys.argv[0]+' <include_dir>')
        sys.exit()

    # create the header object
    header = obj_header()
    excluded_files = ['cef_application_mac.h', 'cef_version.h']
    header.add_directory(sys.argv[1], excluded_files)

    # dump the result to stdout
    sys.stdout.write(make_wrapper_types_header(header))
