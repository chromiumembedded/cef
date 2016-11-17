# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
from file_util import *
import sys

# script directory.
script_dir = os.path.dirname(__file__)

# CEF root directory.
cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))

def get_files_for_variable(cmake_path, variables, variable):
    """ Returns the path values associated with |variable| and relative to the
        |cmake_path| directory. """
    if not variable in variables:
        raise Exception('Variable %s does not exist' % variable)

    # Cmake file directory.
    cmake_dirname = os.path.dirname(cmake_path) + '/'

    # Return path values relative to the cmake file directory.
    # Example 1:
    #   cmake file   = "/path/to/libcef_dll/CMakeLists.txt"
    #   include path = "/path/to/libcef_dll/wrapper/cef_browser_info_map.h"
    #   return path  = "wrapper/cef_browser_info_map.h"
    # Example 2:
    #   cmake file   = "/path/to/libcef_dll/CMakeLists.txt"
    #   include path = "/path/to/include/internal/cef_export.h"
    #   return path  = "../include/internal/cef_export.h"
    new_paths = []
    paths = variables[variable]
    for path in paths:
        abspath = os.path.join(cef_dir, path)
        newpath = normalize_path(os.path.relpath(abspath, cmake_dirname))
        new_paths.append(newpath)
    return new_paths

def format_cmake_set(name, values):
    result = 'set(%s\n' % name
    for value in values:
        result += '  %s\n' % value
    return result + '  )\n'

def format_cmake_group(cmake_path, name, files, platform_sep, append_macro):
    platforms = {}
    common = []

    # Folder will be the cmake parent directory name combined with the path to
    # first file in the files list.
    # Example 1:
    #   cmake file   = "/path/to/libcef_dll/CMakeLists.txt"
    #   include path = "wrapper/cef_browser_info_map.h"
    #   folder       = "libcef_dll\\\\wrapper"
    # Example 2:
    #   cmake file   = "/path/to/libcef_dll/CMakeLists.txt"
    #   include path = "../include/internal/cef_export.h"
    #   folder       = "include\\\\internal"
    folder = os.path.basename(os.path.dirname(cmake_path))
    folder = os.path.dirname(os.path.normpath(os.path.join(folder, files[0])))
    folder = normalize_path(folder).replace('/', '\\\\\\\\')

    # Group the files by platform.
    for file in files:
        parts = file.split(platform_sep)
        file = parts[0]
        if len(parts) > 1:
            # Add the file under the platform.
            platform = parts[1]
            if not platform in platforms:
                platforms[platform] = []
            platforms[platform].append(file)
        else:
            common.append(file)

    result = ''
    if len(common) > 0:
        result += format_cmake_set(name, common)

    if len(platforms) > 0:
        keys = sorted(platforms.keys())
        for key in keys:
            result += format_cmake_set(name + '_' + key, platforms[key])
        result += '%s(%s)\n' % (append_macro, name)

    result += 'source_group(%s FILES ${%s})\n\n' % (folder, name)
    return result

def format_cmake_library(name, group_names):
    result = 'add_library(%s\n' % name
    for group in group_names:
        result += '  ${%s}\n' % group
    return result + '  )\n\n'

def process_cmake_template_segment(segment, segment_ct, cmake_path, variables):
    prefix = None
    library = None
    set = None
    includes = []
    suffix = '_SRCS'    # Appended to each group name before the platform name.
    platform_sep = ':'  # Used to separate value from platform name.
    append_macro = 'APPEND_PLATFORM_SOURCES'  # CMake macro name.

    # Extract values from |segment|. Example |segment| contents:
    #  'prefix': 'cefsimple',
    #  'includes': [
    #    'cefsimple_sources_common',
    #    'cefsimple_sources_win:WINDOWS',
    #    'cefsimple_sources_mac:MACOSX',
    #    'cefsimple_sources_linux:LINUX',
    #  ],
    values = eval('{' + segment + '}', {'__builtins__': None}, None)
    if 'prefix' in values:
        prefix = values['prefix']
    else:
        raise Exception('Missing prefix value in segment %d' % segment_ct)

    if 'library' in values:
        library = values['library']

    if 'set' in values:
        set = values['set']

    if 'append_macro' in values:
        append_macro = values['append_macro']

    if 'includes' in values and len(values['includes']) > 0:
        for include in values['includes']:
            parts = include.strip().split(platform_sep)
            files = get_files_for_variable(cmake_path, variables, parts[0])
            if len(parts) == 2:
                # Append the platform to each file path.
                files = [file + platform_sep + parts[1] for file in files]
            includes.extend(files)
    else:
        raise Exception('Missing includes value in segment %d' % segment_ct)

    # Sort the file paths alphabetically.
    includes.sort()

    # Group files by path.
    # For example, '../include/base/foo.h' and '../include/base/bar.h' will be
    # grouped as 'PREFIX_INCLUDE_BASE'.
    groups = {}
    for include in includes:
        paths = include.split('/')
        label = prefix
        for path in paths[0:-1]:
            if path == '..':
                continue
            label += '_' + path
        label = label.replace('.', '_').upper()
        if not label in groups:
            groups[label] = []
        groups[label].append(include)

    # Create the output results.
    result = ''

    keys = sorted(groups.keys())
    for key in keys:
        # Add a group of files that share the same path.
        result += format_cmake_group(cmake_path, key + suffix, groups[key], \
                                     platform_sep, append_macro)

    if not library is None:
        # Add the library declaration if requested.
        result += format_cmake_library(library, [key + suffix for key in keys])

    if not set is None:
        # Add the set declaration if requested.
        result += format_cmake_set(set, \
                                   ['${' + key + suffix + '}' for key in keys])

    return result.strip()

def process_cmake_template(input, output, variables, quiet = False):
    """ Reads the |input| template, parses variable substitution sections and
        writes |output|. """
    if not quiet:
        sys.stdout.write('Processing "%s" to "%s"...\n' % (input, output))

    if not os.path.exists(input):
        raise Exception('File %s does not exist' % input)

    cmake_path = normalize_path(os.path.abspath(input))
    template = read_file(cmake_path)

    delim_start = '{{'
    delim_end = '}}'

    # Process the template file, replacing segments delimited by |delim_start|
    # and |delim_end|.
    result = ''
    end = 0
    segment_ct = 0
    while True:
        start = template.find(delim_start, end)
        if start == -1:
            break
        result += template[end:start]
        end = template.find(delim_end, start + len(delim_start))
        if end == -1:
            break
        segment = template[start + len(delim_start):end]
        segment_ct = segment_ct + 1
        result += process_cmake_template_segment(segment, segment_ct, \
                                                 cmake_path, variables)
        end += len(delim_end)
    result += template[end:]

    # Only write the output file if the contents have changed.
    changed = True
    if os.path.exists(output):
        existing = read_file(output)
        changed = result != existing
    if changed:
        write_file(output, result)

def read_gypi_variables(source):
    """ Read the |source| gypi file and extract the variables section. """
    path = os.path.join(cef_dir, source + '.gypi')
    if not os.path.exists(path):
        raise Exception('File %s does not exist' % path)
    contents = eval_file(path)
    if not 'variables' in contents:
        raise Exception('File %s does not have a variables section' % path)
    return contents['variables']

# File entry point.
if __name__ == "__main__":
    # Verify that the correct number of command-line arguments are provided.
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: '+sys.argv[0]+' <infile> <outfile>')
        sys.exit()

    # Read the gypi files and combine into a single dictionary.
    variables1 = read_gypi_variables('cef_paths')
    variables2 = read_gypi_variables('cef_paths2')
    variables = dict(variables1.items() + variables2.items())

    # Process the cmake template.
    process_cmake_template(sys.argv[1], sys.argv[2], variables)
