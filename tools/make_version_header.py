# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from date_util import *
from file_util import *
from optparse import OptionParser
from cef_api_hash import cef_api_hash
import git_util as git
import sys

# cannot be loaded as a module
if __name__ != "__main__":
    sys.stderr.write('This file cannot be loaded as a module!')
    sys.exit()


# parse command-line options
disc = """
This utility creates the version header file.
"""

parser = OptionParser(description=disc)
parser.add_option('--header', dest='header', metavar='FILE',
                  help='output version header file [required]')
parser.add_option('--cef_version', dest='cef_version', metavar='FILE',
                  help='input CEF version config file [required]')
parser.add_option('--chrome_version', dest='chrome_version', metavar='FILE',
                  help='input Chrome version config file [required]')
parser.add_option('--cpp_header_dir', dest='cpp_header_dir', metavar='DIR',
                  help='input directory for C++ header files [required]')
parser.add_option('-q', '--quiet',
                  action='store_true', dest='quiet', default=False,
                  help='do not output detailed status information')
(options, args) = parser.parse_args()

# the header option is required
if options.header is None or options.cef_version is None or options.chrome_version is None or options.cpp_header_dir is None:
    parser.print_help(sys.stdout)
    sys.exit()

def write_version_header(header, chrome_version, cef_version, cpp_header_dir):
    """ Creates the header file for the current revision and Chrome version information
       if the information has changed or if the file doesn't already exist. """

    if not path_exists(chrome_version):
      raise Exception('Chrome version file '+chrome_version+' does not exist.')
    if not path_exists(cef_version):
      raise Exception('CEF version file '+cef_version+' does not exist.')

    args = {}
    read_version_file(chrome_version, args)
    read_version_file(cef_version, args)

    if path_exists(header):
        oldcontents = read_file(header)
    else:
        oldcontents = ''

    year = get_year()

    if not git.is_checkout('.'):
       raise Exception('Not a valid checkout')

    commit_number = git.get_commit_number()
    commit_hash = git.get_hash()
    version = '%s.%s.%s.g%s' % (args['CEF_MAJOR'], args['BUILD'], commit_number, commit_hash[:7])

    # calculate api hashes
    api_hash_calculator = cef_api_hash(cpp_header_dir, verbose = False)
    api_hashes = api_hash_calculator.calculate()

    newcontents = '// Copyright (c) '+year+' Marshall A. Greenblatt. All rights reserved.\n'+\
                  '//\n'+\
                  '// Redistribution and use in source and binary forms, with or without\n'+\
                  '// modification, are permitted provided that the following conditions are\n'+\
                  '// met:\n'+\
                  '//\n'+\
                  '//    * Redistributions of source code must retain the above copyright\n'+\
                  '// notice, this list of conditions and the following disclaimer.\n'+\
                  '//    * Redistributions in binary form must reproduce the above\n'+\
                  '// copyright notice, this list of conditions and the following disclaimer\n'+\
                  '// in the documentation and/or other materials provided with the\n'+\
                  '// distribution.\n'+\
                  '//    * Neither the name of Google Inc. nor the name Chromium Embedded\n'+\
                  '// Framework nor the names of its contributors may be used to endorse\n'+\
                  '// or promote products derived from this software without specific prior\n'+\
                  '// written permission.\n'+\
                  '//\n'+\
                  '// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n'+\
                  '// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n'+\
                  '// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n'+\
                  '// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n'+\
                  '// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n'+\
                  '// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n'+\
                  '// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n'+\
                  '// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n'+\
                  '// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n'+\
                  '// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n'+\
                  '// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n'+\
                  '//\n'+\
                  '// ---------------------------------------------------------------------------\n'+\
                  '//\n'+\
                  '// This file is generated by the make_version_header.py tool.\n'+\
                  '//\n\n'+\
                  '#ifndef CEF_INCLUDE_CEF_VERSION_H_\n'+\
                  '#define CEF_INCLUDE_CEF_VERSION_H_\n\n'+\
                  '#define CEF_VERSION "' + version + '"\n'+\
                  '#define CEF_VERSION_MAJOR ' + args['CEF_MAJOR'] + '\n'+\
                  '#define CEF_COMMIT_NUMBER ' + commit_number + '\n'+\
                  '#define CEF_COMMIT_HASH "' + commit_hash + '"\n'+\
                  '#define COPYRIGHT_YEAR ' + year + '\n\n'+\
                  '#define CHROME_VERSION_MAJOR ' + args['MAJOR'] + '\n'+\
                  '#define CHROME_VERSION_MINOR ' + args['MINOR'] + '\n'+\
                  '#define CHROME_VERSION_BUILD ' + args['BUILD'] + '\n'+\
                  '#define CHROME_VERSION_PATCH ' + args['PATCH'] + '\n\n'+\
                  '#define DO_MAKE_STRING(p) #p\n'+\
                  '#define MAKE_STRING(p) DO_MAKE_STRING(p)\n\n'+\
                  '#ifndef APSTUDIO_HIDDEN_SYMBOLS\n\n'\
                  '#include "include/internal/cef_export.h"\n\n'+\
                  '#ifdef __cplusplus\n'+\
                  'extern "C" {\n'+\
                  '#endif\n\n'+\
                  '// The API hash is created by analyzing CEF header files for C API type\n'+\
                  '// definitions. The hash value will change when header files are modified\n'+\
                  '// in a way that may cause binary incompatibility with other builds. The\n'+\
                  '// universal hash value will change if any platform is affected whereas the\n'+\
                  '// platform hash values will change only if that particular platform is\n'+\
                  '// affected.\n'+\
                  '#define CEF_API_HASH_UNIVERSAL "' + api_hashes['universal'] + '"\n'+\
                  '#if defined(OS_WIN)\n'+\
                  '#define CEF_API_HASH_PLATFORM "' + api_hashes['windows'] + '"\n'+\
                  '#elif defined(OS_MACOSX)\n'+\
                  '#define CEF_API_HASH_PLATFORM "' + api_hashes['macosx'] + '"\n'+\
                  '#elif defined(OS_LINUX)\n'+\
                  '#define CEF_API_HASH_PLATFORM "' + api_hashes['linux'] + '"\n'+\
                  '#endif\n\n'+\
                  '// Returns CEF version information for the libcef library. The |entry|\n'+\
                  '// parameter describes which version component will be returned:\n'+\
                  '// 0 - CEF_VERSION_MAJOR\n'+\
                  '// 1 - CEF_COMMIT_NUMBER\n'+\
                  '// 2 - CHROME_VERSION_MAJOR\n'+\
                  '// 3 - CHROME_VERSION_MINOR\n'+\
                  '// 4 - CHROME_VERSION_BUILD\n'+\
                  '// 5 - CHROME_VERSION_PATCH\n'+\
                  '///\n'+\
                  'CEF_EXPORT int cef_version_info(int entry);\n\n'+\
                  '///\n'+\
                  '// Returns CEF API hashes for the libcef library. The returned string is owned\n'+\
                  '// by the library and should not be freed. The |entry| parameter describes which\n'+\
                  '// hash value will be returned:\n'+\
                  '// 0 - CEF_API_HASH_PLATFORM\n'+\
                  '// 1 - CEF_API_HASH_UNIVERSAL\n'+\
                  '// 2 - CEF_COMMIT_HASH\n'+\
                  '///\n'+\
                  'CEF_EXPORT const char* cef_api_hash(int entry);\n\n'+\
                  '#ifdef __cplusplus\n'+\
                  '}\n'+\
                  '#endif\n\n'+\
                  '#endif  // APSTUDIO_HIDDEN_SYMBOLS\n\n'+\
                  '#endif  // CEF_INCLUDE_CEF_VERSION_H_\n'
    if newcontents != oldcontents:
        write_file(header, newcontents)
        return True

    return False

written = write_version_header(options.header, options.chrome_version, options.cef_version, options.cpp_header_dir)
if not options.quiet:
  if written:
    sys.stdout.write('File '+options.header+' updated.\n')
  else:
    sys.stdout.write('File '+options.header+' is already up to date.\n')
