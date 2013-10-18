# Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import sys
import subprocess
import urllib
import xml.etree.ElementTree as ET

def check_url(url):
  """ Check the URL and raise an exception if invalid. """
  if ':' in url[:7]:
    parts = url.split(':', 1)
    if (parts[0] == 'http' or parts[0] == 'https' or parts[0] == 'svn') and \
        parts[1] == urllib.quote(parts[1]):
      return url
  sys.stderr.write('Invalid URL: '+url+"\n")
  raise Exception('Invalid URL: '+url)

def get_svn_info(path):
  """ Retrieves the URL and revision from svn info. """
  url = 'None'
  rev = 'None'
  if path[0:4] == 'http' or os.path.exists(path):
    try:
      if sys.platform == 'win32':
        # Force use of the SVN version bundled with depot_tools.
        svn = 'svn.bat'
      else:
        svn = 'svn'
      p = subprocess.Popen([svn, 'info', '--xml', path], \
          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      out, err = p.communicate()
      if err == '':
        tree = ET.ElementTree(ET.fromstring(out))
        entry = tree.getroot().find('entry')
        url = entry.find('url').text
        rev = entry.attrib['revision']
      else:
        raise Exception("Failed to execute svn info:\n"+err+"\n")
    except IOError, (errno, strerror):
      sys.stderr.write('Failed to read svn info: '+strerror+"\n")
      raise
    except:
      raise
  return {'url': url, 'revision': rev}

def get_revision(path = '.'):
  """ Retrieves the revision from svn info. """
  info = get_svn_info(path)
  if info['revision'] == 'None':
    raise Exception('Unable to retrieve SVN revision for "'+path+'"')
  return info['revision']

def get_changed_files(path = '.'):
  """ Retrieves the list of changed files from svn status. """
  files = []
  if os.path.exists(path):
    try:
      stream = os.popen('svn status '+path)
      for line in stream:
        status = line[0]
        # Return paths with add, modify and switch status.
        if status == 'A' or status == 'M' or status == 'S':
          files.append(line[8:].strip())
    except IOError, (errno, strerror):
      sys.stderr.write('Failed to read svn status: '+strerror+"\n")
      raise
  return files
