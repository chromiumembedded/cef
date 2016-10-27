# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
#
# Resort order of object files in libcef.ninja file.
#
# See: https://bitbucket.org/chromiumembedded/cef/issues/1999
#
# Usage:
#   import issue_1999
#   issue_1999.apply(output_path)
#
import sys
import os

module_order = [
  "_sse",  "-sse",  "_ssse",  "-ssse",
  "_sse2", "-sse2", "_ssse2", "-ssse2",
  "_sse3", "-sse3", "_ssse3", "-ssse3",
  "_sse4", "-sse4", "_ssse4", "-ssse4",
  "_avx",  "-avx",  "_savx",  "-savx",
  "_avx1", "-avx1", "_savx1", "-savx1",
  "_avx2", "-avx2", "_savx2", "-savx2",
  ]

def get_obj_class(item):
  item = item.lower()
  for i in range(len(module_order) - 1, -1, -1):
    x = module_order[i]
    if x in item:
      return 1 + i
  return 0

def obj_compare(x, y):
  xc = get_obj_class(x)
  yc = get_obj_class(y)
  if xc < yc: return -1
  elif xc > yc: return 1
  else: return 0


def process_line(line):
  if line.startswith("build ./libcef.dll ./libcef.dll.lib: solink "):
    index = line.find("solink")
    if index >= 0:
      part1 = line[0:index + 6]
      part2 = line[index + 6:]
      stampsIndex = part2.find("||")
      stamps = part2[stampsIndex:]
      objects = part2[:stampsIndex]

      objects_list = objects.split()
      objects_list = sorted(objects_list, cmp = obj_compare)
      return part1 + " " + " ".join(objects_list) + " " + stamps
  return line


def process_file(path):
  print "Applying issue #1999 fix to " + path

  with open(path) as f:
    content = f.read().splitlines()

  result = []

  for line in content:
    result.append(process_line(line))

  with open(path, "w") as f:
    f.write("\n".join(result))
    f.write("\n")

def apply(confpath):
  process_file(os.path.join(confpath, "obj", "cef", "libcef.ninja"))
