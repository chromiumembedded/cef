# Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from file_util import *
import os
import re
import shutil
import string
import sys
import textwrap
import time
import itertools
import hashlib


class cef_api_hash:
    """ CEF API hash calculator """

    def __init__(self, headerdir, debugdir = None, verbose = False):
        if headerdir is None or len(headerdir) == 0:
            raise AssertionError("headerdir is not specified")

        self.__headerdir = headerdir;
        self.__debugdir = debugdir;
        self.__verbose = verbose;
        self.__debug_enabled = not (self.__debugdir is None) and len(self.__debugdir) > 0;

        self.platforms = [ "windows", "macosx", "linux" ];

        self.platform_files = {
            "windows": [
                "internal/cef_types_win.h",
                ],
            "macosx": [
                "internal/cef_types_mac.h",
                ],
            "linux": [
                "internal/cef_types_linux.h",
                ]
            };

        self.included_files = [
            ];

        self.excluded_files = [
            "cef_version.h",
            "internal/cef_tuple.h",
            "internal/cef_types_wrappers.h",
            "internal/cef_string_wrappers.h",
            "internal/cef_win.h",
            "internal/cef_mac.h",
            "internal/cef_linux.h",
            ];

    def calculate(self):
        filenames = [filename for filename in self.__get_filenames() if not filename in self.excluded_files]

        objects = []
        for filename in filenames:
            if self.__verbose:
                print "Processing " + filename + "..."
            content = read_file(os.path.join(self.__headerdir, filename), True)
            platforms = list([p for p in self.platforms if self.__is_platform_filename(filename, p)])

            # Parse cef_string.h happens in special case: grab only defined CEF_STRING_TYPE_xxx declaration
            content_objects = None
            if filename == "internal/cef_string.h":
                content_objects = self.__parse_string_type(content)
            else:
                content_objects = self.__parse_objects(content)

            for o in content_objects:
                o["text"] = self.__prepare_text(o["text"])
                o["platforms"] = platforms
                o["filename"] = filename
                objects.append(o)

        # objects will be sorted including filename, to make stable universal hashes
        objects = sorted(objects, key = lambda o: o["name"] + "@" + o["filename"])

        if self.__debug_enabled:
            namelen = max([len(o["name"]) for o in objects])
            filenamelen = max([len(o["filename"]) for o in objects])
            dumpsig = [];
            for o in objects:
                dumpsig.append(format(o["name"], str(namelen) + "s") + "|" + format(o["filename"], "" + str(filenamelen) + "s") + "|" + o["text"]);
            self.__write_debug_file("objects.txt", dumpsig)

        revisions = { };

        for platform in itertools.chain(["universal"], self.platforms):
            sig = self.__get_final_sig(objects, platform)
            if self.__debug_enabled:
                self.__write_debug_file(platform + ".sig", sig)
            rev = hashlib.sha1(sig).digest();
            revstr = ''.join(format(ord(i),'0>2x') for i in rev)
            revisions[platform] = revstr

        return revisions

    def __parse_objects(self, content):
        """ Returns array of objects in content file. """
        objects = []
        content = re.sub("//.*\n", "", content)

        # function declarations
        for m in re.finditer("\nCEF_EXPORT\s+?.*?\s+?(\w+)\s*?\(.*?\)\s*?;", content, flags = re.DOTALL):
            object = {
                "name": m.group(1),
                "text": m.group(0).strip()
            }
            objects.append(object)

        # structs
        for m in re.finditer("\ntypedef\s+?struct\s+?(\w+)\s+?\{.*?\}\s+?(\w+)\s*?;", content, flags = re.DOTALL):
            object = {
                "name": m.group(2),
                "text": m.group(0).strip()
            }
            objects.append(object)

        # enums
        for m in re.finditer("\nenum\s+?(\w+)\s+?\{.*?\}\s*?;", content, flags = re.DOTALL):
            object = {
                "name": m.group(1),
                "text": m.group(0).strip()
            }
            objects.append(object)

        # typedefs
        for m in re.finditer("\ntypedef\s+?.*?\s+(\w+);", content, flags = 0):
            object = {
                "name": m.group(1),
                "text": m.group(0).strip()
            }
            objects.append(object)

        return objects

    def __parse_string_type(self, content):
        """ Grab defined CEF_STRING_TYPE_xxx """
        objects = []
        for m in re.finditer("\n\s*?#\s*?define\s+?(CEF_STRING_TYPE_\w+)\s+?.*?\n", content, flags = 0):
            object = {
                "name": m.group(1),
                "text": m.group(0),
            }
            objects.append(object)
        return objects

    def __prepare_text(self, text):
        text = text.strip()
        text = re.sub("\s+", " ", text);
        text = re.sub("\(\s+", "(", text);
        return text

    def __get_final_sig(self, objects, platform):
        sig = []

        for o in objects:
            if platform == "universal" or platform in o["platforms"]:
                sig.append(o["text"])

        return "\n".join(sig)

    def __get_filenames(self):
        """ Returns file names to be processed, relative to headerdir """
        headers = [os.path.join(self.__headerdir, filename) for filename in self.included_files];
        headers = itertools.chain(headers, get_files(os.path.join(self.__headerdir, "capi", "*.h")))
        headers = itertools.chain(headers, get_files(os.path.join(self.__headerdir, "internal", "*.h")))

        for v in self.platform_files.values():
            headers = itertools.chain(headers, [os.path.join(self.__headerdir, f) for f in v])

        normalized = [os.path.relpath(filename, self.__headerdir) for filename in headers];
        normalized = [f.replace('\\', '/').lower() for f in normalized];

        return list(set(normalized));

    def __is_platform_filename(self, filename, platform):
        if platform == "universal":
            return True
        if not platform in self.platform_files:
            return False
        listed = False
        for p in self.platforms:
            if filename in self.platform_files[p]:
                if p == platform:
                    return True
                else:
                    listed = True
        return not listed

    def __write_debug_file(self, filename, content):
        make_dir(self.__debugdir);
        outfile = os.path.join(self.__debugdir, filename);
        dir = os.path.dirname(outfile);
        make_dir(dir);
        if not isinstance(content, basestring):
            content = "\n".join(content)
        write_file(outfile, content)


if __name__ == "__main__":
    from optparse import OptionParser
    import time

    disc = """
    This utility calculates CEF API hash.
    """

    parser = OptionParser(description=disc)
    parser.add_option('--cpp-header-dir', dest='cppheaderdir', metavar='DIR',
                      help='input directory for C++ header files [required]')
    parser.add_option('--debug-dir', dest='debugdir', metavar='DIR',
                      help='intermediate directory for easy debugging')
    parser.add_option('-v', '--verbose',
                      action='store_true', dest='verbose', default=False,
                      help='output detailed status information')
    (options, args) = parser.parse_args()

    # the cppheader option is required
    if options.cppheaderdir is None:
        parser.print_help(sys.stdout)
        sys.exit()

    # calculate
    c_start_time = time.time()

    calc = cef_api_hash(options.cppheaderdir, options.debugdir, options.verbose);
    revisions = calc.calculate();

    c_completed_in = time.time() - c_start_time

    print "{"
    for k in sorted(revisions.keys()):
        print format("\"" + k + "\"", ">12s") + ": \"" + revisions[k] + "\""
    print "}"
    # print
    # print 'Completed in: ' + str(c_completed_in)
    # print

    # print "Press any key to continue...";
    # sys.stdin.readline();
