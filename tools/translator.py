# Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import sys
from cef_parser import *
from make_capi_header import *
from make_cpptoc_header import *
from make_cpptoc_impl import *
from make_ctocpp_header import *
from make_ctocpp_impl import *
from make_gypi_file import *
from make_views_stub_impl import *
from make_wrapper_types_header import *
from optparse import OptionParser


# cannot be loaded as a module
if __name__ != "__main__":
    sys.stderr.write('This file cannot be loaded as a module!')
    sys.exit()


# parse command-line options
disc = """
This utility generates files for the CEF C++ to C API translation layer.
"""

parser = OptionParser(description=disc)
parser.add_option('--root-dir', dest='rootdir', metavar='DIR',
                  help='CEF root directory [required]')
parser.add_option('--backup',
                  action='store_true', dest='backup', default=False,
                  help='create a backup of modified files')
parser.add_option('-c', '--classes', dest='classes', action='append',
                  help='only translate the specified classes')
parser.add_option('-q', '--quiet',
                  action='store_true', dest='quiet', default=False,
                  help='do not output detailed status information')
(options, args) = parser.parse_args()

# the rootdir option is required
if options.rootdir is None:
    parser.print_help(sys.stdout)
    sys.exit()

# determine the paths
root_dir = os.path.abspath(options.rootdir)
cpp_header_dir = os.path.join(root_dir, 'include')
cpp_header_test_dir = os.path.join(cpp_header_dir, 'test')
cpp_header_views_dir = os.path.join(cpp_header_dir, 'views')
capi_header_dir = os.path.join(cpp_header_dir, 'capi')
libcef_dll_dir = os.path.join(root_dir, 'libcef_dll')
cpptoc_global_impl = os.path.join(libcef_dll_dir, 'libcef_dll.cc')
ctocpp_global_impl = os.path.join(libcef_dll_dir, 'wrapper', 'libcef_dll_wrapper.cc')
wrapper_types_header = os.path.join(libcef_dll_dir, 'wrapper_types.h')
cpptoc_dir = os.path.join(libcef_dll_dir, 'cpptoc')
ctocpp_dir = os.path.join(libcef_dll_dir, 'ctocpp')
gypi_file = os.path.join(root_dir, 'cef_paths.gypi')
views_stub_impl = os.path.join(libcef_dll_dir, 'views_stub.cc')

# make sure the header directory exists
if not path_exists(cpp_header_dir):
    sys.stderr.write('Directory '+cpp_header_dir+' does not exist.')
    sys.exit()

# create the header object
if not options.quiet:
    sys.stdout.write('Parsing C++ headers from '+cpp_header_dir+'...\n')
header = obj_header()

# add include files to be processed
header.set_root_directory(cpp_header_dir)
excluded_files = ['cef_application_mac.h', 'cef_version.h']
header.add_directory(cpp_header_dir, excluded_files)
header.add_directory(cpp_header_test_dir)
header.add_directory(cpp_header_views_dir)

writect = 0

#output the C API header
if not options.quiet:
    sys.stdout.write('In C API header directory '+capi_header_dir+'...\n')
filenames = sorted(header.get_file_names())
for filename in filenames:
    if not options.quiet:
        sys.stdout.write('Generating '+filename+' C API header...\n')
    writect += write_capi_header(header, capi_header_dir, filename,
                                 options.backup)

# output the wrapper types header
if not options.quiet:
    sys.stdout.write('Generating wrapper types header...\n')
writect += write_wrapper_types_header(header,
                                      wrapper_types_header,
                                      options.backup)

# build the list of classes to parse
allclasses = header.get_class_names()
if not options.classes is None:
    for cls in options.classes:
        if not cls in allclasses:
            sys.stderr.write('ERROR: Unknown class: '+cls)
            sys.exit()
    classes = options.classes
else:
    classes = allclasses

classes = sorted(classes)

# output CppToC global file
if not options.quiet:
    sys.stdout.write('Generating CppToC global implementation...\n')
writect += write_cpptoc_impl(header, None, cpptoc_global_impl, options.backup)

# output CToCpp global file
if not options.quiet:
    sys.stdout.write('Generating CToCpp global implementation...\n')
writect += write_ctocpp_impl(header, None, ctocpp_global_impl, options.backup)

# output CppToC class files
if not options.quiet:
    sys.stdout.write('In CppToC directory '+cpptoc_dir+'...\n')
for cls in classes:
    if not options.quiet:
        sys.stdout.write('Generating '+cls+'CppToC class header...\n')
    writect += write_cpptoc_header(header, cls, cpptoc_dir, options.backup)
    if not options.quiet:
        sys.stdout.write('Generating '+cls+'CppToC class implementation...\n')
    writect += write_cpptoc_impl(header, cls, cpptoc_dir, options.backup)

# output CppToC class files
if not options.quiet:
    sys.stdout.write('In CToCpp directory '+ctocpp_dir+'...\n')
for cls in classes:
    if not options.quiet:
        sys.stdout.write('Generating '+cls+'CToCpp class header...\n')
    writect += write_ctocpp_header(header, cls, ctocpp_dir, options.backup)
    if not options.quiet:
        sys.stdout.write('Generating '+cls+'CToCpp class implementation...\n')
    writect += write_ctocpp_impl(header, cls, ctocpp_dir, options.backup)

# output the gypi file
if not options.quiet:
    sys.stdout.write('Generating '+gypi_file+' file...\n')
writect += write_gypi_file(header, gypi_file, options.backup)

# output the views stub file
if not options.quiet:
    sys.stdout.write('Generating '+views_stub_impl+' file...\n')
writect += write_views_stub_impl(header, views_stub_impl, options.backup)

if not options.quiet:
    sys.stdout.write('Done - Wrote '+str(writect)+' files.\n')
