# Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import shutil
import sys
import time

def read_file(name, normalize = True):
    """ Function for reading a file. """
    try:
        f = open(name, 'r')
        # read the data
        data = f.read()
        if normalize:
            # normalize line endings
            data = data.replace("\r\n", "\n")
        return data
    except IOError, (errno, strerror):
        sys.stderr.write('Failed to read file '+filename+': '+strerror)
        raise
    else:
        f.close()
 
def write_file(name, data):
    """ Function for writing a file. """
    try:
        f = open(name, 'w')
        # write the data
        f.write(data)
    except IOError, (errno, strerror):
       sys.stderr.write('Failed to write file '+name+': '+strerror)
       raise
    else:
        f.close()
        
def file_exists(name):
    """ Returns true if the file currently exists. """
    return os.path.exists(name)

def backup_file(name):
    """ Renames the file to a name that includes the current time stamp. """
    shutil.move(name, name+'.'+time.strftime('%Y-%m-%d-%H-%M-%S'))
