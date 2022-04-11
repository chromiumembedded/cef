# Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
import sys
from cef_parser import *
from clang_util import clang_format
from file_util import *
import hashlib
from make_api_hash_header import *
from make_capi_header import *
from make_cpptoc_header import *
from make_cpptoc_impl import *
from make_ctocpp_header import *
from make_ctocpp_impl import *
from make_gypi_file import *
from make_libcef_dll_dylib_impl import *
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
parser.add_option(
    '--root-dir',
    dest='rootdir',
    metavar='DIR',
    help='CEF root directory [required]')
parser.add_option(
    '--backup',
    action='store_true',
    dest='backup',
    default=False,
    help='create a backup of modified files')
parser.add_option(
    '--force',
    action='store_true',
    dest='force',
    default=False,
    help='force rewrite of the file')
parser.add_option(
    '-c',
    '--classes',
    dest='classes',
    action='append',
    help='only translate the specified classes')
parser.add_option(
    '-q',
    '--quiet',
    action='store_true',
    dest='quiet',
    default=False,
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
api_hash_header = os.path.join(cpp_header_dir, 'cef_api_hash.h')
libcef_dll_dir = os.path.join(root_dir, 'libcef_dll')
cpptoc_global_impl = os.path.join(libcef_dll_dir, 'libcef_dll.cc')
ctocpp_global_impl = os.path.join(libcef_dll_dir, 'wrapper',
                                  'libcef_dll_wrapper.cc')
wrapper_types_header = os.path.join(libcef_dll_dir, 'wrapper_types.h')
cpptoc_dir = os.path.join(libcef_dll_dir, 'cpptoc')
ctocpp_dir = os.path.join(libcef_dll_dir, 'ctocpp')
gypi_file = os.path.join(root_dir, 'cef_paths.gypi')
libcef_dll_dylib_impl = os.path.join(libcef_dll_dir, 'wrapper',
                                     'libcef_dll_dylib.cc')

# make sure the header directory exists
if not path_exists(cpp_header_dir):
  sys.stderr.write('Directory ' + cpp_header_dir + ' does not exist.')
  sys.exit()

# create the header object
if not options.quiet:
  sys.stdout.write('Parsing C++ headers from ' + cpp_header_dir + '...\n')
header = obj_header()

# add include files to be processed
header.set_root_directory(cpp_header_dir)
excluded_files = ['cef_api_hash.h', 'cef_application_mac.h', 'cef_version.h']
header.add_directory(cpp_header_dir, excluded_files)
header.add_directory(cpp_header_test_dir)
header.add_directory(cpp_header_views_dir)

# Track the number of files that were written.
writect = 0


def update_file(file, newcontents):
  """ Replaces the contents of |file| with |newcontents| if necessary. """
  oldcontents = ''
  oldhash = ''

  if newcontents[-1:] != "\n":
    # Add newline at end of file.
    newcontents += "\n"

  # clang-format is slow so we don't want to apply it if the pre-formatted
  # content hasn't changed. To check for changes we embed a hash of the pre-
  # formatted content in the resulting file.
  hash_start = "$hash="
  hash_end = "$"
  hash_token = "$$HASH$$"

  if not options.force and path_exists(file):
    oldcontents = read_file(file)

    # Extract the existing hash.
    start = oldcontents.find(hash_start)
    if start > 0:
      end = oldcontents.find(hash_end, start + len(hash_start))
      if end > 0:
        oldhash = oldcontents[start + len(hash_start):end]

  # Compute the new hash.
  newhash = hashlib.sha1(newcontents.encode('utf-8')).hexdigest()

  if oldhash == newhash:
    # Pre-formatted contents have not changed.
    return

  newcontents = newcontents.replace(hash_token, newhash, 1)

  # Apply clang-format for C/C++ files.
  if os.path.splitext(file)[1][1:] in ('c', 'cc', 'cpp', 'h'):
    result = clang_format(file, newcontents)
    if result != None:
      newcontents = result
    else:
      raise Exception("Call to clang-format failed")

  if options.backup and oldcontents != '':
    backup_file(file)

  filedir = os.path.split(file)[0]
  if not os.path.isdir(filedir):
    make_dir(filedir)

  write_file(file, newcontents)

  global writect
  writect += 1


# output the C API header
if not options.quiet:
  sys.stdout.write('In C API header directory ' + capi_header_dir + '...\n')
filenames = sorted(header.get_file_names())
for filename in filenames:
  if not options.quiet:
    sys.stdout.write('Generating ' + filename + ' C API header...\n')
  update_file(*write_capi_header(header, capi_header_dir, filename))

# output the wrapper types header
if not options.quiet:
  sys.stdout.write('Generating wrapper types header...\n')
update_file(*write_wrapper_types_header(header, wrapper_types_header))

# build the list of classes to parse
allclasses = header.get_class_names()
if not options.classes is None:
  for cls in options.classes:
    if not cls in allclasses:
      sys.stderr.write('ERROR: Unknown class: ' + cls)
      sys.exit()
  classes = options.classes
else:
  classes = allclasses

classes = sorted(classes)

# output CppToC global file
if not options.quiet:
  sys.stdout.write('Generating CppToC global implementation...\n')
update_file(*write_cpptoc_impl(header, None, cpptoc_global_impl))

# output CToCpp global file
if not options.quiet:
  sys.stdout.write('Generating CToCpp global implementation...\n')
update_file(*write_ctocpp_impl(header, None, ctocpp_global_impl))

# output CppToC class files
if not options.quiet:
  sys.stdout.write('In CppToC directory ' + cpptoc_dir + '...\n')
for cls in classes:
  if not options.quiet:
    sys.stdout.write('Generating ' + cls + 'CppToC class header...\n')
  update_file(*write_cpptoc_header(header, cls, cpptoc_dir))
  if not options.quiet:
    sys.stdout.write('Generating ' + cls + 'CppToC class implementation...\n')
  update_file(*write_cpptoc_impl(header, cls, cpptoc_dir))

# output CppToC class files
if not options.quiet:
  sys.stdout.write('In CToCpp directory ' + ctocpp_dir + '...\n')
for cls in classes:
  if not options.quiet:
    sys.stdout.write('Generating ' + cls + 'CToCpp class header...\n')
  update_file(*write_ctocpp_header(header, cls, ctocpp_dir))
  if not options.quiet:
    sys.stdout.write('Generating ' + cls + 'CToCpp class implementation...\n')
  update_file(*write_ctocpp_impl(header, cls, ctocpp_dir))

# output the gypi file
if not options.quiet:
  sys.stdout.write('Generating ' + gypi_file + ' file...\n')
update_file(*write_gypi_file(header, gypi_file))

# output the libcef dll dylib file
if not options.quiet:
  sys.stdout.write('Generating ' + libcef_dll_dylib_impl + ' file...\n')
update_file(*write_libcef_dll_dylib_impl(header, libcef_dll_dylib_impl))

# Update the API hash header file if necessary. This must be done last because
# it reads files that were potentially written by proceeding operations.
if not options.quiet:
  sys.stdout.write('Generating API hash header...\n')
if write_api_hash_header(api_hash_header, cpp_header_dir):
  writect += 1

if not options.quiet:
  sys.stdout.write('Done - Wrote ' + str(writect) + ' files.\n')
