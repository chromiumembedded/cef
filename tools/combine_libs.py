#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(slightlyoff): move to using shared version of this script.

'''This script makes it easy to combine libs and object files to a new lib,
optionally removing some of the object files in the input libs by regular
expression matching.
For usage information, run the script with a --help argument.
'''
import optparse
import os
import re
import subprocess
import sys


def Shell(*args):
  '''Runs the program and args in args, returns the output from the program.'''
  process = subprocess.Popen(args,
                             stdin = None,
                             stdout = subprocess.PIPE,
                             stderr = subprocess.STDOUT)
  output = process.stdout.readlines()
  process.wait()
  retcode = process.returncode
  if retcode != 0:
    raise RuntimeError('%s exited with status %d' % (args[0], retcode))
  return output


def CollectRemovals(remove_re, inputs):
  '''Returns a list of all object files in inputs that match remove_re.'''
  removals = []
  for input in inputs:
    output = Shell('lib.exe', '/list', input)

    for line in output:
      line = line.rstrip()
      if remove_re.search(line):
        removals.append(line)

  return removals


def CombineLibraries(output, remove_re, inputs):
  '''Combines all the libraries and objects in inputs, while removing any
  object files that match remove_re.
  '''
  removals = []
  if remove_re:
    removals = CollectRemovals(remove_re, inputs)

  if len(removals) > 0:
    print 'Removals: ', removals

  args = ['lib.exe', '/out:%s' % output]
  args += ['/remove:%s' % obj for obj in removals]
  args += inputs
  Shell(*args)


USAGE = '''usage: %prog [options] <lib or obj>+

Combines input libraries or objects into an output library, while removing
any object file (in the input libraries) that matches a given regular
expression.
'''

def GetOptionParser():
  parser = optparse.OptionParser(USAGE)
  parser.add_option('-o', '--output', dest = 'output',
                    help = 'write to this output library')
  parser.add_option('-r', '--remove', dest = 'remove',
                    help = 'object files matching this regexp will be removed '
                            'from the output library')
  return parser


def Main():
  '''Main function for this script'''
  parser = GetOptionParser()
  (opt, args) = parser.parse_args()
  output = opt.output
  remove = opt.remove
  if not output:
    parser.error('You must specify an output file')

  if not args:
    parser.error('You must specify at least one object or library')

  output = output.strip()
  if remove:
    remove = remove.strip()

  if remove:
    try:
      remove_re = re.compile(opt.remove)
    except:
      parser.error('%s is not a valid regular expression' % opt.remove)
  else:
    remove_re = None

  if sys.platform != 'win32' and sys.platform != 'cygwin':
    parser.error('this script only works on Windows for now')

  # If this is set, we can't capture lib.exe's output.
  if 'VS_UNICODE_OUTPUT' in os.environ:
    del os.environ['VS_UNICODE_OUTPUT']

  CombineLibraries(output, remove_re, args)
  return 0


if __name__ == '__main__':
  sys.exit(Main())
