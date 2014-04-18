# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from exec_util import exec_cmd
import os
import urllib
import xml.etree.ElementTree as ET

def is_checkout(path):
  """ Returns true if the path represents an svn checkout. """
  return os.path.exists(os.path.join(path, '.svn'))

def check_url(url):
  """ Check the URL and raise an exception if invalid. """
  if ':' in url[:7]:
    parts = url.split(':', 1)
    if (parts[0] == 'http' or parts[0] == 'https' or parts[0] == 'svn') and \
        parts[1] == urllib.quote(parts[1]):
      return url
  raise Exception('Invalid URL: %s' % (url))

def get_svn_info(path = '.'):
  """ Retrieves the URL and revision from svn info. """
  url = 'None'
  rev = 'None'
  cmd = "svn info --xml %s" % (path)
  is_http = path[0:4] == 'http'
  if is_http or os.path.exists(path):
    result = exec_cmd(cmd, path if not is_http else '.')
    if result['err'] == '':
      tree = ET.ElementTree(ET.fromstring(result['out']))
      entry = tree.getroot().find('entry')
      url = entry.find('url').text
      rev = entry.attrib['revision']
    else:
      raise Exception("Failed to execute svn info: %s" % (result['err']))
  return {'url': url, 'revision': rev}

def get_revision(path = '.'):
  """ Retrieves the revision from svn info. """
  info = get_svn_info(path)
  if info['revision'] == 'None':
    raise Exception('Unable to retrieve SVN revision for %s' % (path))
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
      raise
  return files
