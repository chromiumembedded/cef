# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os

def get_revision(path = '.'):
    """ Retrieves the revision number from stdin. """
    try:
        stream = os.popen('svn info '+path);
        # read the revision number
        for line in stream:
            if line[0:9] == "Revision:":
                return line[10:-1];
        raise IOError("Revision line not found.")
    except IOError, (errno, strerror):
        sys.stderr.write('Failed to read revision from "svn info": ' + strerror)
        raise
