#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import httplib
from optparse import OptionParser
import os
import re
import shlex
import subprocess
import sys
import urllib
import urlparse

# Cannot be loaded as a module.
if __name__ != "__main__":
  sys.stderr.write('This file cannot be loaded as a module!')
  sys.exit()

def run(command_line, working_dir):
  """ Run the specified command line. """
  if not options.quiet:
    print '-------- Running "%s" in "%s"...' % (command_line, working_dir)
  if not options.dryrun:
    args = shlex.split(command_line.replace('\\', '\\\\'))
    return subprocess.check_call(args, cwd=working_dir, env=os.environ,
                                 shell=(sys.platform == 'win32'))

def fail(message):
  """ Exit the script due to an execution failure. """
  print message
  sys.exit(1)

def url_request(url, method, headers, body, expected_response):
  """ Execute an arbitrary request. """
  parsed_url = urlparse.urlparse(url)
  if parsed_url.scheme == "http":
    connection = httplib.HTTPConnection(parsed_url.netloc)
  elif parsed_url.scheme == "https":
    connection = httplib.HTTPSConnection(parsed_url.netloc)
  else:
    print 'Unsupported URL format for %s' % url
    return None

  connection.putrequest(method, url)

  if not headers is None:
    for key, val in headers.iteritems():
      connection.putheader(key, val)
  if not body is None:
    connection.putheader('Content-Length', len(body))
  connection.endheaders()

  if not body is None:
    connection.send(body)

  response = connection.getresponse()
  if response.status == expected_response:
    return response.read()
  else:
    print 'URL %s returned unexpected response code %d' % \
        (url, response.status)
    return None

def url_propfind(url, depth, body):
  """ Execute a PROPFIND request. """
  return url_request(url, 'PROPFIND',
      {'Depth': depth, 'Content-Type': 'text/xml'}, body, 207)

def url_get(url):
  """ Execute a GET request. """
  return url_request(url, 'GET', None, None, 200)

def extract_string(str, start, end):
  """ Returns the string between start and end. """
  s = str.find(start)
  if s < 0:
    return None
  slen = len(start)
  e = str.find(end, s + slen)
  if e < 0:
    return None
  return str[s + slen:e]

def extract_int(str, start, end):
  """ Returns the integer between start and end. """
  val = extract_string(str, start, end)
  if not val is None and re.match('^[0-9]{1,}$', val):
    return int(val)
  return None

def read_file(name, normalize = True):
  """ Read a file. """
  try:
    f = open(name, 'r')
    # read the data
    data = f.read()
    if normalize:
      # normalize line endings
      data = data.replace("\r\n", "\n")
    return data
  except IOError, (errno, strerror):
    print 'Failed to read file %s: %s' % (name, strerror)
  else:
    f.close()
  return None

def write_file(name, data):
  """ Write a file. """
  try:
    f = open(name, 'w')
    f.write(data)
    return True
  except IOError, (errno, strerror):
   print 'Failed to write file %s: %s' % (name, strerror)
  else:
    f.close()
  return True

def read_cache_file(name, args):
  """ Read and parse a cache file (key=value pairs, one per line). """
  content = read_file(name)
  if content is None:
    return False
  lines = content.split("\n")
  for line in lines:
    parts = line.split('=', 1)
    if len(parts) == 2:
      args[parts[0]] = parts[1]
  return True

def write_cache_file(name, args):
  """ Write a cache file (key=value pairs, one per line). """
  data = ''
  for key, val in args.iteritems():
    data = data + key + '=' + str(val) + "\n"
  return write_file(name, data)


# Parse command-line options.
disc = """This utility creates and synchronizes git-svn clones of CEF SVN
repositories."""

parser = OptionParser(description=disc)
parser.add_option('--storage-dir', dest='storagedir', metavar='DIR',
                  help='local directory where data will be stored')
parser.add_option('--branch', dest='branch',
                  help='CEF branch to clone ' +
                       '(trunk/cef3, branches/1453/cef3, etc)')
parser.add_option('--git-repo', dest='gitrepo',
                  help='remote repo where the git data will be pushed ' +
                       '(user@domain:path/to/repo.git)')
parser.add_option('--force',
                  action='store_true', dest='force', default=False,
                  help="force the run even if the revision hasn't changed")
parser.add_option('--dry-run',
                  action='store_true', dest='dryrun', default=False,
                  help="output commands without executing them")
parser.add_option('-q', '--quiet',
                  action='store_true', dest='quiet', default=False,
                  help='do not output detailed status information')
(options, args) = parser.parse_args()

# Required options.
if options.storagedir is None or options.branch is None or \
   options.gitrepo is None:
  parser.print_help(sys.stderr)
  sys.exit(1)

# Validate the git repo format. Should be user@domain:path/to/repo.git
if not re.match(
    '^[a-zA-Z0-9_\-]{1,}@[a-zA-Z0-9\-\.]{1,}:[a-zA-Z0-9\-_/]{1,}\.git$',
    options.gitrepo):
  fail('Invalid git repo format: %s' % options.gitrepo)

svn_url = 'https://chromiumembedded.googlecode.com/svn/' + options.branch

# Verify that the requested branch is valid CEF root directory.
value = url_get(svn_url + '/CHROMIUM_BUILD_COMPATIBILITY.txt')
if value is None:
  fail('Invalid branch "%s"' % options.branch)

# Retrieve the most recent revision for the branch.
revision = None
request = '<?xml version="1.0" encoding="utf-8"?><propfind xmlns="DAV:">' + \
          '<prop><version-name xmlns="DAV:"/></prop></propfind>'
value = url_propfind(svn_url, 0, request)
if not value is None:
  revision  = extract_int(value, '<lp1:version-name>', '</lp1:version-name>')
if revision is None:
  fail('Failed to discover revision for branch "%s"' % options.branch)

branch_path_comp = options.branch.replace('/', '-')

# Create the branch storage directory if it doesn't already exist.
branch_dir = os.path.join(options.storagedir, branch_path_comp)
if not os.path.exists(branch_dir):
  os.makedirs(branch_dir)

# Default cache configuration.
cache_config = {
  'last_revision': 0,
}

# Create the authors.txt file if it doesn't already exist
authors_file_path = os.path.join(options.storagedir, 'authors.txt')
if not os.path.exists(authors_file_path):
  content = 'magreenblatt@gmail.com = ' + \
            'Marshall Greenblatt <magreenblatt@gmail.com>'
  if not write_file(authors_file_path, content):
    fail('Failed to create authors.txt file: %s' % authors_file_path)

# Read the cache file if it exists.
cache_file_path = os.path.join(branch_dir, 'cache.txt')
if os.path.exists(cache_file_path):
  if not read_cache_file(cache_file_path, cache_config):
    print 'Failed to read cache.txt file %s' % cache_file_path

# Check if the revision has changed.
if not options.force and int(cache_config['last_revision']) == revision:
  if not options.quiet:
    print 'Already at revision %d' % revision
  sys.exit()

repo_dir = os.path.join(branch_dir, branch_path_comp)
if not os.path.exists(repo_dir):
  # Create the git repository.
  run('git svn clone -A %s %s %s' % (authors_file_path, svn_url, repo_dir),
      branch_dir)
  run('git remote add origin %s' % options.gitrepo, repo_dir)
else:
  # Rebase the git repository.
  run('git svn rebase --fetch-all -A %s' % authors_file_path, repo_dir)

run('git push origin --all', repo_dir)

# Write the cache file.
cache_config['last_revision'] = revision
if not write_cache_file(cache_file_path, cache_config):
  print 'Failed to write cache file %s' % cache_file_path
