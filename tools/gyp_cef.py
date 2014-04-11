# Copyright (c) 2013 The Chromium Embedded Framework Authors.
# Portions copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is (possibly, depending on python version) imported by gyp_cef
# when it creates sub-processes through the multiprocessing library.

# Importing in Python 2.6 (fixed in 2.7) on Windows doesn't search for
# imports that don't end in .py (and aren't directories with an
# __init__.py). This wrapper makes "import gyp_cef" work with those old
# versions and makes it possible to execute gyp_cef.py directly on Windows
# where the extension is useful.

import os

path = os.path.abspath(os.path.split(__file__)[0])
execfile(os.path.join(path, 'gyp_cef'))
