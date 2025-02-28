# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
import bisect
from date_util import *
from file_util import *
import os
import re
import shutil
import string
import sys
import textwrap
import time
from version_util import version_as_numeric, version_as_variable

_NOTIFY_CONTEXT = None
_NOTIFY_CONTEXT_LAST = None


def set_notify_context(context):
  global _NOTIFY_CONTEXT
  _NOTIFY_CONTEXT = context


def notify(msg):
  """ Display a message. """
  global _NOTIFY_CONTEXT_LAST

  if not _NOTIFY_CONTEXT is None and _NOTIFY_CONTEXT != _NOTIFY_CONTEXT_LAST:
    print('In %s:' % _NOTIFY_CONTEXT)
    _NOTIFY_CONTEXT_LAST = _NOTIFY_CONTEXT

  print('  NOTE: ' + msg)


def wrap_text(text, indent='', maxchars=80, listitem=False):
  """ Wrap the text to the specified number of characters. If
    necessary a line will be broken and wrapped after a word.
    """
  if listitem:
    initial_indent = indent + '- '
    subsequent_indent = indent + '  '
  else:
    initial_indent = indent
    subsequent_indent = indent
  lines = textwrap.wrap(
      text,
      maxchars,
      initial_indent=initial_indent,
      subsequent_indent=subsequent_indent)
  return '\n'.join(lines) + '\n'


def is_base_class(clsname):
  """ Returns true if |clsname| is a known base (root) class in the object
        hierarchy.
    """
  return clsname == 'CefBaseRefCounted' or clsname == 'CefBaseScoped'


def get_capi_file_name(cppname, versions=False):
  """ Convert a C++ header file name to a C API header file name. """
  return cppname[:-2] + ('_capi_versions.h' if versions else '_capi.h')


def get_capi_name(cppname, isclassname, prefix=None, version=None):
  """ Convert a C++ CamelCaps name to a C API underscore name. """
  result = ''
  lastchr = ''
  for chr in cppname:
    # add an underscore if the current character is an upper case letter
    # and the last character was a lower case letter or number.
    if len(result) > 0 and chr.isalpha() \
        and chr.upper() == chr \
        and lastchr.isalnum() and lastchr.lower() == lastchr:
      result += '_'
    result += chr.lower()
    lastchr = chr

  if isclassname and not version is None:
    result += '_%d' % version

  if isclassname:
    result += '_t'

  if not prefix is None:
    if prefix[0:3] == 'cef':
      # if the prefix name is duplicated in the function name
      # remove that portion of the function name
      subprefix = prefix[3:]
      pos = result.find(subprefix)
      if pos >= 0:
        result = result[0:pos] + result[pos + len(subprefix):]
    result = prefix + '_' + result

  return result


def get_wrapper_type_enum(cppname):
  """ Returns the wrapper type enumeration value for the specified C++ class
        name. """
  return 'WT_' + get_capi_name(cppname, False)[4:].upper()


def get_prev_line(body, pos):
  """ Retrieve the start and end positions and value for the line immediately
    before the line containing the specified position.
    """
  end = body.rfind('\n', 0, pos)
  start = body.rfind('\n', 0, end) + 1
  line = body[start:end]
  return {'start': start, 'end': end, 'line': line}


def get_comment(body, name):
  """ Retrieve the comment for a class or function. """
  result = []

  pos = body.find(name)
  in_block_comment = False
  while pos > 0:
    data = get_prev_line(body, pos)
    line = data['line'].strip()
    pos = data['start']
    if len(line) == 0:
      break
    # single line /*--cef()--*/
    elif line[0:2] == '/*' and line[-2:] == '*/':
      continue
    # start of multi line /*--cef()--*/
    elif in_block_comment and line[0:2] == '/*':
      in_block_comment = False
      continue
    # end of multi line /*--cef()--*/
    elif not in_block_comment and line[-2:] == '*/':
      in_block_comment = True
      continue
    elif in_block_comment:
      continue
    elif line[0:3] == '///':
      # keep the comment line including any leading spaces
      result.append(line[3:])
    else:
      break

  result.reverse()
  return result


def validate_comment(file, name, comment):
  """ Validate the comment array returned by get_comment(). """
  # Verify that the comment contains beginning and ending '///' as required by
  # Doxygen (the leading '///' from each line will already have been removed by
  # the get_comment() logic).
  if len(comment) < 3 or len(comment[0]) != 0 or len(comment[-1]) != 0:
    raise Exception('Missing or incorrect comment in %s for: %s' % \
        (file, name))


def format_comment(comment, indent, translate_map=None, maxchars=80):
  """ Return the comments array as a formatted string. """
  if not translate_map is None:
    # Replace longest keys first in translation.
    translate_keys = sorted(
        translate_map.keys(), key=lambda item: (-len(item), item))

  result = ''
  wrapme = ''
  hasemptyline = False
  listitem = False
  for line in comment:
    # if the line starts with a leading space, remove that space
    if not line is None and len(line) > 0 and line[0] == ' ':
      line = line[1:]
      didremovespace = True
    else:
      didremovespace = False

    if line is None or len(line) == 0 or (line[0] == ' ' and not listitem) \
        or line[0] == '-':
      # the previous paragraph or list item, if any, has ended
      if len(wrapme) > 0:
        if not translate_map is None:
          # apply the translation
          for key in translate_keys:
            wrapme = wrapme.replace(key, translate_map[key])
        # output the previous paragraph
        result += wrap_text(wrapme, indent + '/// ', maxchars, listitem)
        wrapme = ''
        listitem = False

    if not line is None:
      if len(line) > 0 and line[0] == '-':
        # list item
        listitem = True
        wrapme = line[1:].strip()
      if len(line) > 0 and line[0] == ' ' and listitem:
        # list item continues
        wrapme += line[1:]
      if len(line) == 0 or (line[0] == ' ' and not listitem):
        # blank lines or anything that's further indented should be
        # output as-is
        result += indent + '///'
        if len(line) > 0:
          if didremovespace:
            result += ' ' + line
          else:
            result += line
        result += '\n'
        listitem = False
      else:
        if not listitem:
          # add to the current paragraph
          wrapme += line + ' '
    else:
      # output an empty line
      hasemptyline = True
      result += '\n'

  if len(wrapme) > 0:
    if not translate_map is None:
      # apply the translation
      for key in translate_map.keys():
        wrapme = wrapme.replace(key, translate_map[key])
    # output the previous paragraph
    result += wrap_text(wrapme, indent + '/// ', maxchars, listitem)

  if hasemptyline:
    # an empty line means a break between comments, so the comment is
    # probably a section heading and should have an extra line before it
    result = '\n' + result
  return result


def format_translation_changes(old, new):
  """ Return a comment stating what is different between the old and new
    function prototype parts.
    """
  changed = False
  result = ''

  # normalize C API attributes
  oldargs = [x.replace('struct _', '') for x in old['args']]
  oldretval = old['retval'].replace('struct _', '')
  newargs = [x.replace('struct _', '') for x in new['args']]
  newretval = new['retval'].replace('struct _', '')

  # check if the prototype has changed
  oldset = set(oldargs)
  newset = set(newargs)
  if len(oldset.symmetric_difference(newset)) > 0:
    changed = True
    result += '\n  // WARNING - CHANGED ATTRIBUTES'

    # in the implementation set only
    oldonly = oldset.difference(newset)
    for arg in oldonly:
      result += '\n  //   REMOVED: ' + arg

    # in the current set only
    newonly = newset.difference(oldset)
    for arg in newonly:
      result += '\n  //   ADDED:   ' + arg

  # check if the return value has changed
  if oldretval != newretval:
    changed = True
    result += '\n  // WARNING - CHANGED RETURN VALUE'+ \
              '\n  //   WAS: '+old['retval']+ \
              '\n  //   NOW: '+new['retval']

  if changed:
    result += '\n  #pragma message("Warning: " __FILE__ ": '+new['name']+ \
              ' prototype has changed")\n'

  return result


def format_translation_includes(header,
                                body,
                                with_versions=False,
                                other_includes=None):
  """ Return the necessary list of includes based on the contents of the
    body.
    """
  result = ''

  # <algorithm> required for VS2013.
  if body.find('std::min') > 0 or body.find('std::max') > 0:
    result += '#include <algorithm>\n'

  paths = set()

  if body.find('cef_api_hash(') > 0 or body.find('cef_api_version(') > 0:
    paths.add('include/cef_api_hash.h')

  if body.find('template_util::has_valid_size(') > 0:
    paths.add('libcef_dll/template_util.h')

  search = ((True, True, r'([A-Za-z0-9_]{1,})_[0-9]{1,}_CppToC'),
            (True, False, r'([A-Za-z0-9_]{1,})CppToC'),
            (False, True, r'([A-Za-z0-9_]{1,})_[0-9]{1,}_CToCpp'),
            (False, False, r'([A-Za-z0-9_]{1,})CToCpp'))
  for cpptoc, versioned, regex in search:
    # identify what classes are being used
    p = re.compile(regex)
    items = set(p.findall(body))
    for item in items:
      if item == 'Cef':
        continue
      if not versioned and item[-1] == '_':
        # skip versioned names that are picked up by the unversioned regex
        continue
      directory = ''
      if not is_base_class(item):
        cls = header.get_class(item)
        if cls is None:
          raise Exception('Class does not exist: ' + item)
        dir = cls.get_file_directory()
        if not dir is None:
          directory = dir + '/'
      type = 'cpptoc' if cpptoc else 'ctocpp'
      paths.add('libcef_dll/' + type + '/'+directory+ \
                get_capi_name(item[3:], False)+'_' + type + '.h')

  if body.find('shutdown_checker') > 0:
    paths.add('libcef_dll/shutdown_checker.h')

  if body.find('transfer_') > 0:
    paths.add('libcef_dll/transfer_util.h')

  if not other_includes is None:
    paths.update(other_includes)

  if len(paths) > 0:
    if len(result) > 0:
      result += '\n'

    paths = sorted(list(paths))
    result += '\n'.join(['#include "%s"' % p for p in paths]) + '\n'

  return result


def format_notreached(library_side, msg, default_retval='', indent='  '):
  if library_side:
    return 'NOTREACHED() << __func__ << ' + msg + ';'
  return 'CHECK(false) << __func__ << ' + msg + ';\n' + \
         indent + 'return%s;' % ((' '  + default_retval) if len(default_retval) > 0 else '')


def _has_version_added(attribs):
  return 'added' in attribs


def _has_version_removed(attribs):
  return 'removed' in attribs


def _has_version(attribs):
  return _has_version_added(attribs) or _has_version_removed(attribs)


def get_version_check(attribs):
  assert _has_version(attribs)

  added = attribs.get('added', None)
  if not added is None:
    added = version_as_variable(added)
  removed = attribs.get('removed', None)
  if not removed is None:
    removed = version_as_variable(removed)

  if not added is None and not removed is None:
    return 'CEF_API_RANGE(%s, %s)' % (added, removed)
  elif not added is None:
    return 'CEF_API_ADDED(%s)' % added
  return 'CEF_API_REMOVED(%s)' % removed


def _get_version_attrib(attribs, key):
  value = attribs.get(key, None)
  if not value is None:
    return version_as_numeric(value)

  # Unversioned is always the first value.
  return 0


def _get_version_added(attribs):
  """ Returns a numeric 'added' value used for sorting purposes. """
  return _get_version_attrib(attribs, 'added')


def _get_version_removed(attribs):
  """ Returns a numeric 'removed' value used for sorting purposes. """
  return _get_version_attrib(attribs, 'removed')


def get_version_surround(obj, long=False):
  """ Returns (pre,post) strings for a version check. """
  version_check = obj.get_version_check() if obj.has_version() else None

  # Don't duplicate the surrounding class version check for a virtual method.
  if not version_check is None and \
    isinstance(obj, obj_function) and isinstance(obj.parent, obj_class) and \
    obj.parent.has_version() and obj.parent.get_version_check() == version_check:
    version_check = None

  if not version_check is None:
    return ('#if %s\n' % version_check, ('#endif  // %s\n' % version_check)
            if long else '#endif\n')
  return ('', '')


def get_clsname(cls, version):
  name = cls.get_name()

  if not version is None:
    # Select the appropriate version for this class.
    closest_version = cls.get_closest_version(version)
    if closest_version is None:
      raise Exception('Cannot find version <= %d for %s' % (version, name))
    return '%s_%d_' % (name, closest_version)

  return name


def _version_order_funcs(funcs, max_version=None):
  """ Applies version-based ordering to a list of funcs. """
  versions = {0: []}

  for func in funcs:
    if func.has_version():
      added = func.get_version_added()
      if not added in versions:
        versions[added] = [func]
      else:
        versions[added].append(func)
    else:
      # Unversioned funcs.
      versions[0].append(func)

  result = []
  for version in sorted(versions.keys()):
    if not max_version is None and version > max_version:
      break
    result.extend(versions[version])
  return result


def _get_all_versions(funcs):
  # Using a set to ensure uniqueness.
  versions = set({0})

  for func in funcs:
    if func.has_version():
      versions.add(func.get_version_added())
      versions.add(func.get_version_removed())

  return versions


def _find_closest_not_greater(lst, target):
  assert isinstance(lst, list), lst
  assert isinstance(target, int), target
  idx = bisect.bisect_right(lst, target) - 1
  if idx < 0:
    return None
  return lst[idx]


def str_to_dict(str):
  """ Convert a string to a dictionary. If the same key has multiple values
        the values will be stored in a list. """
  dict = {}
  parts = str.split(',')
  for part in parts:
    part = part.strip()
    if len(part) == 0:
      continue
    sparts = part.split('=')
    if len(sparts) > 2:
      raise Exception('Invalid dictionary pair format: ' + part)
    name = sparts[0].strip()
    if len(sparts) == 2:
      val = sparts[1].strip()
    else:
      val = True
    if name in dict:
      # a value with this name already exists
      curval = dict[name]
      if not isinstance(curval, list):
        # convert the string value to a list
        dict[name] = [curval]
      dict[name].append(val)
    else:
      dict[name] = val
  return dict


def dict_to_str(dict):
  """ Convert a dictionary to a string. """
  str = []
  for name in dict.keys():
    if not isinstance(dict[name], list):
      if dict[name] is True:
        # currently a bool value
        str.append(name)
      else:
        # currently a string value
        str.append(name + '=' + dict[name])
    else:
      # currently a list value
      for val in dict[name]:
        str.append(name + '=' + val)
  return ','.join(str)


# Attribute keys allowed in CEF metadata comments.
COMMON_ATTRIB_KEYS = ('added', 'removed')
CLASS_ATTRIB_KEYS = COMMON_ATTRIB_KEYS + ('no_debugct_check', 'source')
FUNCTION_ATTRIB_KEYS = COMMON_ATTRIB_KEYS + ('api_hash_check', 'capi_name',
                                             'count_func', 'default_retval',
                                             'index_param', 'optional_param')

# regex for matching comment-formatted attributes
_cre_attrib = r'/\*--cef\(([A-Za-z0-9_ ,=:\n]{0,})\)--\*/'
# regex for matching class and function names
_cre_cfname = r'([A-Za-z0-9_]{1,})'
# regex for matching class and function names including path separators
_cre_cfnameorpath = r'([A-Za-z0-9_\/]{1,})'
# regex for matching typedef value and name combination
_cre_typedef = r'([A-Za-z0-9_<>:,\*\&\s]{1,})'
# regex for matching function return value and name combination
_cre_func = r'([A-Za-z][A-Za-z0-9_<>:,\*\&\s]{1,})'
# regex for matching virtual function modifiers + arbitrary whitespace
_cre_vfmod = r'([\sA-Za-z0-9_]{0,})'
# regex for matching arbitrary whitespace
_cre_space = r'[\s]{1,}'
# regex for matching optional virtual keyword
_cre_virtual = r'(?:[\s]{1,}virtual){0,1}'

# Simple translation types. Format is:
#   'cpp_type' : ['capi_type', 'capi_default_value']
_simpletypes = {
    'void': ['void', ''],
    'void*': ['void*', 'NULL'],
    'int': ['int', '0'],
    'int16_t': ['int16_t', '0'],
    'uint16_t': ['uint16_t', '0'],
    'int32_t': ['int32_t', '0'],
    'uint32_t': ['uint32_t', '0'],
    'int64_t': ['int64_t', '0'],
    'uint64_t': ['uint64_t', '0'],
    'double': ['double', '0'],
    'float': ['float', '0'],
    'float*': ['float*', 'NULL'],
    'long': ['long', '0'],
    'unsigned long': ['unsigned long', '0'],
    'long long': ['long long', '0'],
    'size_t': ['size_t', '0'],
    'bool': ['int', '0'],
    'char': ['char', '0'],
    'char* const': ['char* const', 'NULL'],
    'cef_color_t': ['cef_color_t', '0'],
    'cef_json_parser_error_t': ['cef_json_parser_error_t', 'JSON_NO_ERROR'],
    'CefAcceleratedPaintInfo': [
        'cef_accelerated_paint_info_t', 'CefAcceleratedPaintInfo()'
    ],
    'CefAudioParameters': ['cef_audio_parameters_t', 'CefAudioParameters()'],
    'CefBaseTime': ['cef_basetime_t', 'CefBaseTime()'],
    'CefBoxLayoutSettings': [
        'cef_box_layout_settings_t', 'CefBoxLayoutSettings()'
    ],
    'CefCompositionUnderline': [
        'cef_composition_underline_t', 'CefCompositionUnderline()'
    ],
    'CefCursorHandle': ['cef_cursor_handle_t', 'kNullCursorHandle'],
    'CefCursorInfo': ['cef_cursor_info_t', 'CefCursorInfo()'],
    'CefDraggableRegion': ['cef_draggable_region_t', 'CefDraggableRegion()'],
    'CefEventHandle': ['cef_event_handle_t', 'kNullEventHandle'],
    'CefInsets': ['cef_insets_t', 'CefInsets()'],
    'CefKeyEvent': ['cef_key_event_t', 'CefKeyEvent()'],
    'CefMainArgs': ['cef_main_args_t', 'CefMainArgs()'],
    'CefMouseEvent': ['cef_mouse_event_t', 'CefMouseEvent()'],
    'CefPoint': ['cef_point_t', 'CefPoint()'],
    'CefPopupFeatures': ['cef_popup_features_t', 'CefPopupFeatures()'],
    'CefRange': ['cef_range_t', 'CefRange()'],
    'CefRect': ['cef_rect_t', 'CefRect()'],
    'CefScreenInfo': ['cef_screen_info_t', 'CefScreenInfo()'],
    'CefSize': ['cef_size_t', 'CefSize()'],
    'CefTouchEvent': ['cef_touch_event_t', 'CefTouchEvent()'],
    'CefTouchHandleState': [
        'cef_touch_handle_state_t', 'CefTouchHandleState()'
    ],
    'CefThreadId': ['cef_thread_id_t', 'TID_UI'],
    'CefTime': ['cef_time_t', 'CefTime()'],
    'CefWindowHandle': ['cef_window_handle_t', 'kNullWindowHandle'],
}


def get_function_impls(content, ident, has_impl=True):
  """ Retrieve the function parts from the specified contents as a set of
    return value, name, arguments and body. Ident must occur somewhere in
    the value.
    """
  # Remove prefix from methods in CToCpp files.
  content = content.replace('NO_SANITIZE("cfi-icall") ', '')
  content = content.replace('NO_SANITIZE("cfi-icall")\n', '')

  # extract the functions
  find_regex = r'\n' + _cre_func + r'\((.*?)\)([A-Za-z0-9_\s]{0,})'
  if has_impl:
    find_regex += r'\{(.*?)\n\}'
  else:
    find_regex += r'(;)'
  p = re.compile(find_regex, re.MULTILINE | re.DOTALL)
  list = p.findall(content)

  # build the function map with the function name as the key
  result = []
  for retval, argval, vfmod, body in list:
    if retval.find(ident) < 0:
      # the identifier was not found
      continue

    # remove the identifier
    retval = retval.replace(ident, '')
    retval = retval.strip()

    # Normalize the delimiter.
    retval = retval.replace('\n', ' ')

    # retrieve the function name
    parts = retval.split(' ')
    name = parts[-1]
    del parts[-1]
    retval = ' '.join(parts)

    # parse the arguments
    args = []
    if argval != 'void':
      for v in argval.split(','):
        v = v.strip()
        if len(v) > 0:
          args.append(v)

    result.append({
        'retval': retval.strip(),
        'name': name,
        'args': args,
        'vfmod': vfmod.strip(),
        'body': body if has_impl else '',
    })

  return result


def get_next_function_impl(existing, name):
  result = None
  for item in existing:
    if item['name'] == name:
      result = item
      existing.remove(item)
      break
  return result


def get_copyright(full=False, translator=True):
  if full:
    result = \
"""// Copyright (c) $YEAR$ Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
  else:
    result = \
"""// Copyright (c) $YEAR$ The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
"""

  if translator:
    result += \
"""//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//
// $hash=$$HASH$$$
//

"""

  # add the copyright year
  return result.replace('$YEAR$', get_year())


class obj_header:
  """ Class representing a C++ header file. """

  def __init__(self):
    self.filenames = []
    self.typedefs = []
    self.funcs = []
    self.classes = []
    self.root_directory = None

  def set_root_directory(self, root_directory):
    """ Set the root directory. """
    self.root_directory = root_directory

  def get_root_directory(self):
    """ Get the root directory. """
    return self.root_directory

  def add_directory(self, directory, excluded_files=[]):
    """ Add all header files from the specified directory. """
    files = get_files(os.path.join(directory, '*.h'))
    for file in files:
      if len(excluded_files) == 0 or \
          not os.path.split(file)[1] in excluded_files:
        self.add_file(file)

  def add_file(self, filepath):
    """ Add a header file. """

    if self.root_directory is None:
      filename = os.path.split(filepath)[1]
    else:
      filename = os.path.relpath(filepath, self.root_directory)
      filename = filename.replace('\\', '/')

    try:
      # read the input file into memory
      self.add_data(filename, read_file(filepath))
    except Exception:
      print('Exception while parsing %s' % filepath)
      raise

  def add_data(self, filename, data):
    """ Add header file contents. """

    added = False

    # remove space from between template definition end brackets
    data = data.replace("> >", ">>")

    # extract global typedefs
    p = re.compile(r'\ntypedef' + _cre_space + _cre_typedef + r';',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      # build the global typedef objects
      for value in list:
        pos = value.rfind(' ')
        if pos < 0:
          raise Exception('Invalid typedef: ' + value)
        alias = value[pos + 1:].strip()
        value = value[:pos].strip()
        self.typedefs.append(obj_typedef(self, filename, value, alias))

    # extract global functions
    p = re.compile(r'\n' + _cre_attrib + r'\n' + _cre_func + r'\((.*?)\)',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      added = True

      # build the global function objects
      for attrib, retval, argval in list:
        comment = get_comment(data, retval + '(' + argval + ');')
        validate_comment(filename, retval, comment)
        self.funcs.append(
            obj_function(self, filename, attrib, retval, argval, comment))

    # extract includes
    p = re.compile(r'\n#include \"include/' + _cre_cfnameorpath + r'.h')
    includes = p.findall(data)

    # extract forward declarations
    p = re.compile(r'\nclass' + _cre_space + _cre_cfname + r';')
    forward_declares = p.findall(data)

    # extract empty classes
    p = re.compile(r'\n' + _cre_attrib + r'\nclass' + _cre_space + _cre_cfname +
                   _cre_space + r':' + _cre_space + r'public' + _cre_virtual +
                   _cre_space + _cre_cfname + _cre_space + r'{};',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      added = True

      # build the class objects
      for attrib, name, parent_name in list:
        # Style may place the ':' on the next line.
        comment = get_comment(data, name + ' :')
        if len(comment) == 0:
          comment = get_comment(data, name + "\n")
        validate_comment(filename, name, comment)
        self.classes.append(
            obj_class(self, filename, attrib, name, parent_name, "", comment,
                      includes, forward_declares))

      # Remove empty classes from |data| so we don't mess up the non-empty
      # class search that follows.
      data = p.sub('', data)

    # extract classes
    p = re.compile(r'\n' + _cre_attrib + r'\nclass' + _cre_space + _cre_cfname +
                   _cre_space + r':' + _cre_space + r'public' + _cre_virtual +
                   _cre_space + _cre_cfname + _cre_space + r'{(.*?)\n};',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      added = True

      # build the class objects
      for attrib, name, parent_name, body in list:
        # Style may place the ':' on the next line.
        comment = get_comment(data, name + ' :')
        if len(comment) == 0:
          comment = get_comment(data, name + "\n")
        validate_comment(filename, name, comment)
        self.classes.append(
            obj_class(self, filename, attrib, name, parent_name, body, comment,
                      includes, forward_declares))

    if added:
      # a global function or class was read from the header file
      self.filenames.append(filename)

  def __repr__(self):
    result = ''

    if len(self.typedefs) > 0:
      strlist = []
      for cls in self.typedefs:
        strlist.append(str(cls))
      result += "\n".join(strlist) + "\n\n"

    if len(self.funcs) > 0:
      strlist = []
      for cls in self.funcs:
        strlist.append(str(cls))
      result += "\n".join(strlist) + "\n\n"

    if len(self.classes) > 0:
      strlist = []
      for cls in self.classes:
        strlist.append(str(cls))
      result += "\n".join(strlist)

    return result

  def get_file_names(self):
    """ Return the array of header file names. """
    return self.filenames

  def get_typedefs(self):
    """ Return the array of typedef objects. """
    return self.typedefs

  def get_funcs(self, filename=None):
    """ Return the array of function objects. """
    if filename is None:
      return self.funcs
    else:
      # only return the functions in the specified file
      res = []
      for func in self.funcs:
        if func.get_file_name() == filename:
          res.append(func)
      return res

  def get_classes(self, filename=None):
    """ Return the array of class objects. """
    if filename is None:
      return self.classes
    else:
      # only return the classes in the specified file
      res = []
      for cls in self.classes:
        if cls.get_file_name() == filename:
          res.append(cls)
      return res

  def get_class(self, classname):
    """ Return the specified class or None if not found. """
    for cls in self.classes:
      if cls.get_name() == classname:
        return cls
    return None

  def get_capi_class(self, classname):
    """ Return the specified class or None if not found. """
    for cls in self.classes:
      if cls.get_capi_name() == classname:
        return cls
    return None

  def get_class_names(self):
    """ Returns the names of all classes in this object. """
    result = []
    for cls in self.classes:
      result.append(cls.get_name())
    return result

  def get_base_class_name(self, classname):
    """ Returns the base (root) class name for |classname|. """
    cur_cls = self.get_class(classname)
    while True:
      parent_name = cur_cls.get_parent_name()
      if is_base_class(parent_name):
        return parent_name
      else:
        parent_cls = self.get_class(parent_name)
        if parent_cls is None:
          break
      cur_cls = self.get_class(parent_name)
    return None

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    for cls in self.typedefs:
      cls.get_types(list)

    for cls in self.classes:
      cls.get_types(list)

  def get_alias_translation(self, alias):
    """ Return a translation of alias to value based on typedef
            statements. """
    for cls in self.typedefs:
      if cls.alias == alias:
        return cls.value
    return None

  def get_analysis(self, value, named=True):
    """ Return an analysis of the value based the header file context. """
    return obj_analysis([self], value, named)

  def get_defined_structs(self):
    """ Return a list of already defined structure names. """
    return [
        'cef_print_info_t', 'cef_window_info_t', 'cef_base_ref_counted_t',
        'cef_base_scoped_t'
    ]

  def get_capi_translations(self):
    """ Return a dictionary that maps C++ terminology to C API terminology.
        """
    # strings that will be changed in C++ comments
    map = {
        'class': 'structure',
        'Class': 'Structure',
        'interface': 'structure',
        'Interface': 'Structure',
        'true': 'true (1)',
        'false': 'false (0)',
        'empty': 'NULL',
        'method': 'function'
    }

    # add mappings for all classes and functions
    funcs = self.get_funcs()
    for func in funcs:
      map[func.get_name() + '()'] = func.get_capi_name() + '()'

    classes = self.get_classes()
    for cls in classes:
      map[cls.get_name()] = cls.get_capi_name()

      funcs = cls.get_virtual_funcs()
      for func in funcs:
        map[func.get_name() + '()'] = func.get_capi_name() + '()'

      funcs = cls.get_static_funcs()
      for func in funcs:
        map[func.get_name() + '()'] = func.get_capi_name() + '()'

    return map


class obj_class:
  """ Class representing a C++ class. """

  def __init__(self, parent, filename, attrib, name, parent_name, body, comment,
               includes, forward_declares):
    if not isinstance(parent, obj_header):
      raise Exception('Invalid parent object type')

    self.parent = parent
    self.filename = filename
    self.attribs = str_to_dict(attrib)
    self.name = name
    self.parent_name = parent_name
    self.comment = comment
    self.includes = includes
    self.forward_declares = forward_declares

    self._validate_attribs()

    # extract typedefs
    p = re.compile(
        r'\n' + _cre_space + r'typedef' + _cre_space + _cre_typedef + r';',
        re.MULTILINE | re.DOTALL)
    list = p.findall(body)

    # build the typedef objects
    self.typedefs = []
    for value in list:
      pos = value.rfind(' ')
      if pos < 0:
        raise Exception('Invalid typedef: ' + value)
      alias = value[pos + 1:].strip()
      value = value[:pos].strip()
      self.typedefs.append(obj_typedef(self, filename, value, alias))

    # extract static functions
    p = re.compile(r'\n' + _cre_space + _cre_attrib + r'\n' + _cre_space +
                   r'static' + _cre_space + _cre_func + r'\((.*?)\)',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(body)

    # build the static function objects
    self.staticfuncs = []
    for attrib, retval, argval in list:
      comment = get_comment(body, retval + '(' + argval + ')')
      validate_comment(filename, retval, comment)
      self.staticfuncs.append(
          obj_function_static(self, attrib, retval, argval, comment))

    # extract virtual functions
    p = re.compile(
        r'\n' + _cre_space + _cre_attrib + r'\n' + _cre_space + r'virtual' +
        _cre_space + _cre_func + r'\((.*?)\)' + _cre_vfmod,
        re.MULTILINE | re.DOTALL)
    list = p.findall(body)

    # build the virtual function objects
    self.virtualfuncs = []
    self.has_versioned_funcs = False
    for attrib, retval, argval, vfmod in list:
      comment = get_comment(body, retval + '(' + argval + ')')
      validate_comment(filename, retval, comment)
      if not self.has_versioned_funcs and _has_version(attrib):
        self.has_versioned_funcs = True
      self.virtualfuncs.append(
          obj_function_virtual(self, attrib, retval, argval, comment,
                               vfmod.strip()))

    self.virtualfuncs_ordered = None
    self.allversions = None

  def __repr__(self):
    result = '/* ' + dict_to_str(
        self.attribs) + ' */ class ' + self.name + "\n{"

    if len(self.typedefs) > 0:
      result += "\n\t"
      strlist = []
      for cls in self.typedefs:
        strlist.append(str(cls))
      result += "\n\t".join(strlist)

    if len(self.staticfuncs) > 0:
      result += "\n\t"
      strlist = []
      for cls in self.staticfuncs:
        strlist.append(str(cls))
      result += "\n\t".join(strlist)

    if len(self.virtualfuncs) > 0:
      result += "\n\t"
      strlist = []
      for cls in self.virtualfuncs:
        strlist.append(str(cls))
      result += "\n\t".join(strlist)

    result += "\n};\n"
    return result

  def _validate_attribs(self):
    for key in self.attribs.keys():
      if not key in CLASS_ATTRIB_KEYS:
        raise Exception('Invalid attribute key \"%s\" for class %s' %
                        (key, self.get_name()))

  def get_file_name(self):
    """ Return the C++ header file name. Includes the directory component,
            if any. """
    return self.filename

  def get_capi_file_name(self, versions=False):
    """ Return the CAPI header file name. Includes the directory component,
            if any. """
    return get_capi_file_name(self.filename, versions)

  def get_file_directory(self):
    """ Return the file directory component, if any. """
    pos = self.filename.rfind('/')
    if pos >= 0:
      return self.filename[:pos]
    return None

  def get_name(self, version=None):
    """ Return the class name. """
    if not version is None:
      # Select the appropriate version for this class.
      closest_version = self.get_closest_version(version)
      if closest_version is None:
        raise Exception('Cannot find version <= %d for %s' % (version,
                                                              self.name))
      return '%s_%d_' % (self.name, closest_version)
    return self.name

  def get_capi_name(self, version=None, first_version=False):
    """ Return the CAPI structure name for this class. """
    # Select the appropriate version for this class.
    if first_version:
      version = self.get_first_version()
    elif not version is None:
      closest_version = self.get_closest_version(version)
      if closest_version is None:
        raise Exception('Cannot find version <= %d for %s' % (version,
                                                              self.name))
      version = closest_version
    return get_capi_name(self.name, True, version=version)

  def get_parent_name(self):
    """ Return the parent class name. """
    return self.parent_name

  def get_parent_capi_name(self, version=None):
    """ Return the CAPI structure name for the parent class. """
    if not version is None:
      # Select the appropriate version for the parent class.
      if is_base_class(self.parent_name):
        version = None
      else:
        parent_cls = self.parent.get_class(self.parent_name)
        version = parent_cls.get_closest_version(version)
    return get_capi_name(self.parent_name, True, version=version)

  def has_parent(self, parent_name):
    """ Returns true if this class has the specified class anywhere in its
            inheritance hierarchy. """
    # Every class has a known base class as the top-most parent.
    if is_base_class(parent_name) or parent_name == self.parent_name:
      return True
    if is_base_class(self.parent_name):
      return False

    cur_cls = self.parent.get_class(self.parent_name)
    while True:
      cur_parent_name = cur_cls.get_parent_name()
      if is_base_class(cur_parent_name):
        break
      elif cur_parent_name == parent_name:
        return True
      cur_cls = self.parent.get_class(cur_parent_name)

    return False

  def get_comment(self):
    """ Return the class comment as an array of lines. """
    return self.comment

  def get_includes(self):
    """ Return the list of classes that are included from this class'
            header file. """
    return self.includes

  def get_forward_declares(self):
    """ Return the list of classes that are forward declared for this
            class. """
    return self.forward_declares

  def get_attribs(self):
    """ Return all attributes as a dictionary. """
    return self.attribs

  def has_attrib(self, name):
    """ Return true if the specified attribute exists. """
    return name in self.attribs

  def get_attrib(self, name):
    """ Return the first or only value for specified attribute. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is a list
        return self.attribs[name][0]
      else:
        # the value is a string
        return self.attribs[name]
    return None

  def get_attrib_list(self, name):
    """ Return all values for specified attribute as a list. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is already a list
        return self.attribs[name]
      else:
        # convert the value to a list
        return [self.attribs[name]]
    return None

  def get_typedefs(self):
    """ Return the array of typedef objects. """
    return self.typedefs

  def has_typedef_alias(self, alias):
    """ Returns true if the specified typedef alias is defined in the scope
            of this class declaration. """
    for typedef in self.typedefs:
      if typedef.get_alias() == alias:
        return True
    return False

  def get_static_funcs(self):
    """ Return the array of static function objects. """
    return self.staticfuncs

  def get_virtual_funcs(self, version_order=False, version=None):
    """ Return the array of virtual function objects. """
    if version_order and self.has_versioned_funcs:
      if version is None:
        # Cache the ordering result for future use.
        if self.virtualfuncs_ordered is None:
          self.virtualfuncs_ordered = _version_order_funcs(self.virtualfuncs)
        return self.virtualfuncs_ordered

      # Need to order each time to apply the max version.
      return _version_order_funcs(self.virtualfuncs, version)
    return self.virtualfuncs

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    for cls in self.typedefs:
      cls.get_types(list)

    for cls in self.staticfuncs:
      cls.get_types(list)

    for cls in self.virtualfuncs:
      cls.get_types(list)

  def get_alias_translation(self, alias):
    for cls in self.typedefs:
      if cls.alias == alias:
        return cls.value
    return None

  def get_analysis(self, value, named=True):
    """ Return an analysis of the value based on the class definition
        context.
        """
    return obj_analysis([self, self.parent], value, named)

  def is_library_side(self):
    """ Returns true if the class is implemented by the library. """
    return self.attribs['source'] == 'library'

  def is_client_side(self):
    """ Returns true if the class is implemented by the client. """
    return self.attribs['source'] == 'client'

  def has_version(self):
    """ Returns true if the class has an associated version. """
    return _has_version(self.attribs)

  def has_version_added(self):
    """ Returns true if the class has an associated 'added' version. """
    return _has_version_added(self.attribs)

  def get_version_added(self):
    """ Returns the associated 'added' version. """
    return _get_version_added(self.attribs)

  def has_version_removed(self):
    """ Returns true if the class has an associated 'removed' version. """
    return _has_version_removed(self.attribs)

  def get_version_removed(self):
    """ Returns the associated 'removed' version. """
    return _get_version_removed(self.attribs)

  def removed_at_version(self, version):
    """ Returns true if this class is removed at the specified version. """
    return self.has_version_removed() and self.get_version_removed() <= version

  def exists_at_version(self, version):
    """ Returns true if this class exists at the specified version. """
    if self.has_version_added() and self.get_version_added() > version:
      return False
    return not self.removed_at_version(version)

  def get_version_check(self):
    """ Returns the #if check for the associated version. """
    return get_version_check(self.attribs)

  def get_all_versions(self):
    """ Returns all distinct versions of this class. """
    if not self.allversions is None:
      return self.allversions

    # Using a set to ensure uniqueness.
    versions = set()

    # Versions from class inheritance.
    if not is_base_class(self.parent_name):
      versions.update(
          self.parent.get_class(self.parent_name).get_all_versions())

    # Versions from virtual methods.
    versions.update(_get_all_versions(self.virtualfuncs))

    versions = list(versions)

    # Clamp to class versions, if specified.
    if self.has_version_added():
      version_added = self.get_version_added()
      versions = [x for x in versions if x >= version_added]
      if not version_added in versions:
        versions.append(version_added)
    if self.has_version_removed():
      versions = [x for x in versions if x < self.get_version_removed()]

    assert len(versions) > 0, self.get_name()

    self.allversions = sorted(versions)
    return self.allversions

  def get_first_version(self):
    """ Returns the first version. """
    return self.get_all_versions()[0]

  def get_closest_version(self, version):
    """ Returns the closest version to |version| that is not greater, or None. """
    return _find_closest_not_greater(self.get_all_versions(), version)


class obj_typedef:
  """ Class representing a typedef statement. """

  def __init__(self, parent, filename, value, alias):
    if not isinstance(parent, obj_header) \
        and not isinstance(parent, obj_class):
      raise Exception('Invalid parent object type')

    self.parent = parent
    self.filename = filename
    self.alias = alias
    self.value = self.parent.get_analysis(value, False)

  def __repr__(self):
    return 'typedef ' + self.value.get_type() + ' ' + self.alias + ';'

  def get_file_name(self):
    """ Return the C++ header file name. """
    return self.filename

  def get_capi_file_name(self, versions=False):
    """ Return the CAPI header file name. """
    return get_capi_file_name(self.filename, versions)

  def get_alias(self):
    """ Return the alias. """
    return self.alias

  def get_value(self):
    """ Return an analysis of the value based on the class or header file
        definition context.
        """
    return self.value

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    name = self.value.get_type()
    if not name in list:
      list[name] = self.value


class obj_function:
  """ Class representing a function. """

  def __init__(self, parent, filename, attrib, retval, argval, comment):
    self.parent = parent
    self.filename = filename
    self.attribs = str_to_dict(attrib)
    self.retval = obj_argument(self, retval)
    self.name = self.retval.remove_name()
    self.comment = comment

    self._validate_attribs()

    # build the argument objects
    self.arguments = []
    arglist = argval.split(',')
    argindex = 0
    while argindex < len(arglist):
      arg = arglist[argindex]
      if arg.find('<') >= 0 and arg.find('>') == -1:
        # We've split inside of a template type declaration. Join the
        # next argument with this argument.
        argindex += 1
        arg += ',' + arglist[argindex]

      arg = arg.strip()
      if len(arg) > 0:
        argument = obj_argument(self, arg)
        if argument.needs_attrib_count_func() and \
            argument.get_attrib_count_func() is None:
          raise Exception("A 'count_func' attribute is required "+ \
                          "for the '"+argument.get_name()+ \
                          "' parameter to "+self.get_qualified_name())
        self.arguments.append(argument)

      argindex += 1

    if self.retval.needs_attrib_default_retval() and \
        self.retval.get_attrib_default_retval() is None:
      raise Exception("A 'default_retval' attribute is required for "+ \
                      self.get_qualified_name())

  def __repr__(self):
    return '/* ' + dict_to_str(self.attribs) + ' */ ' + self.get_cpp_proto()

  def _validate_attribs(self):
    for key in self.attribs.keys():
      if not key in FUNCTION_ATTRIB_KEYS:
        raise Exception('Invalid attribute key \"%s\" for %s' %
                        (key, self.get_qualified_name()))

  def get_file_name(self):
    """ Return the C++ header file name. """
    return self.filename

  def get_capi_file_name(self, versions=False):
    """ Return the CAPI header file name. """
    return get_capi_file_name(self.filename, versions)

  def get_name(self):
    """ Return the function name. """
    return self.name

  def get_qualified_name(self):
    """ Return the fully qualified function name. """
    if isinstance(self.parent, obj_header):
      # global function
      return self.name
    else:
      # member function
      return self.parent.get_name() + '::' + self.name

  def get_capi_name(self, prefix=None):
    """ Return the CAPI function name. """
    return get_capi_name(self.get_attrib('capi_name', self.name), False, prefix)

  def get_comment(self):
    """ Return the function comment as an array of lines. """
    return self.comment

  def get_attribs(self):
    """ Return all attributes as a dictionary. """
    return self.attribs

  def has_attrib(self, name):
    """ Return true if the specified attribute exists. """
    return name in self.attribs

  def get_attrib(self, name, default=None):
    """ Return the first or only value for specified attribute. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is a list
        return self.attribs[name][0]
      else:
        # the value is a string
        return self.attribs[name]
    return default

  def get_attrib_list(self, name):
    """ Return all values for specified attribute as a list. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is already a list
        return self.attribs[name]
      else:
        # convert the value to a list
        return [self.attribs[name]]
    return None

  def get_retval(self):
    """ Return the return value object. """
    return self.retval

  def get_arguments(self):
    """ Return the argument array. """
    return self.arguments

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    for cls in self.arguments:
      cls.get_types(list)

  def get_capi_parts(self,
                     defined_structs=[],
                     isimpl=False,
                     prefix=None,
                     version=None,
                     version_finder=None):
    """ Return the parts of the C API function definition. """
    retval = ''
    dict = self.retval.get_type().get_capi(defined_structs, version_finder)
    if dict['format'] == 'single':
      retval = dict['value']

    name = self.get_capi_name(prefix)
    args = []

    if isinstance(self, obj_function_virtual):
      # virtual functions get themselves as the first argument
      str = 'struct _' + self.parent.get_capi_name(version=version) + '* self'
      if isinstance(self, obj_function_virtual) and self.is_const():
        # const virtual functions get const self pointers
        str = 'const ' + str
      args.append(str)
    elif not isimpl and len(self.arguments) == 0:
      args.append('void')

    if len(self.arguments) > 0:
      for cls in self.arguments:
        type = cls.get_type()
        dict = type.get_capi(defined_structs, version_finder)
        if dict['format'] == 'single':
          args.append(dict['value'])
        elif dict['format'] == 'multi-arg':
          # add an additional argument for the size of the array
          type_name = type.get_name()
          if type.is_const():
            # for const arrays pass the size argument by value
            args.append('size_t ' + type_name + 'Count')
          else:
            # for non-const arrays pass the size argument by address
            args.append('size_t* ' + type_name + 'Count')
          args.append(dict['value'])

    return {'retval': retval, 'name': name, 'args': args}

  def get_capi_proto(self,
                     defined_structs=[],
                     isimpl=False,
                     prefix=None,
                     version=None,
                     version_finder=None):
    """ Return the prototype of the C API function. """
    parts = self.get_capi_parts(defined_structs, isimpl, prefix, version,
                                version_finder)
    result = parts['retval']+' '+parts['name']+ \
             '('+', '.join(parts['args'])+')'
    return result

  def get_cpp_parts(self, isimpl=False):
    """ Return the parts of the C++ function definition. """
    retval = str(self.retval)
    name = self.name

    args = []
    if len(self.arguments) > 0:
      for cls in self.arguments:
        args.append(str(cls))

    if isimpl and isinstance(self, obj_function_virtual):
      # enumeration return values must be qualified with the class name
      # if the type is defined in the class declaration scope.
      type = self.get_retval().get_type()
      if type.is_result_struct() and type.is_result_struct_enum() and \
          self.parent.has_typedef_alias(retval):
        retval = self.parent.get_name() + '::' + retval

    return {'retval': retval, 'name': name, 'args': args}

  def get_cpp_proto(self, classname=None):
    """ Return the prototype of the C++ function. """
    parts = self.get_cpp_parts()
    result = parts['retval'] + ' '
    if not classname is None:
      result += classname + '::'
    result += parts['name'] + '(' + ', '.join(parts['args']) + ')'
    if isinstance(self, obj_function_virtual) and self.is_const():
      result += ' const'
    return result

  def is_same_side(self, other_class_name):
    """ Returns true if this function is on the same side (library or
            client) and the specified class. """
    if isinstance(self.parent, obj_class):
      # this function is part of a class
      this_is_library_side = self.parent.is_library_side()
      header = self.parent.parent
    else:
      # this function is global
      this_is_library_side = True
      header = self.parent

    if is_base_class(other_class_name):
      other_is_library_side = False
    else:
      other_class = header.get_class(other_class_name)
      if other_class is None:
        raise Exception('Unknown class: ' + other_class_name)
      other_is_library_side = other_class.is_library_side()

    return other_is_library_side == this_is_library_side

  def has_version(self):
    """ Returns true if the class has an associated version. """
    return _has_version(self.attribs)

  def get_version_check(self):
    """ Returns the #if check for the associated version. """
    return get_version_check(self.attribs)


class obj_function_static(obj_function):
  """ Class representing a static function. """

  def __init__(self, parent, attrib, retval, argval, comment):
    if not isinstance(parent, obj_class):
      raise Exception('Invalid parent object type')
    obj_function.__init__(self, parent, parent.filename, attrib, retval, argval,
                          comment)

  def __repr__(self):
    return 'static ' + obj_function.__repr__(self) + ';'

  def get_capi_name(self, prefix=None):
    """ Return the CAPI function name. """
    if prefix is None:
      # by default static functions are prefixed with the class name
      prefix = get_capi_name(self.parent.get_name(), False)
    return obj_function.get_capi_name(self, prefix)


class obj_function_virtual(obj_function):
  """ Class representing a virtual function. """

  def __init__(self, parent, attrib, retval, argval, comment, vfmod):
    if not isinstance(parent, obj_class):
      raise Exception('Invalid parent object type')
    obj_function.__init__(self, parent, parent.filename, attrib, retval, argval,
                          comment)
    if vfmod == 'const':
      self.isconst = True
    else:
      self.isconst = False

  def __repr__(self):
    return 'virtual ' + obj_function.__repr__(self) + ';'

  def is_const(self):
    """ Returns true if the method declaration is const. """
    return self.isconst

  def has_version_added(self):
    """ Returns true if a 'added' value was specified. """
    return _has_version_added(self.attribs)

  def get_version_added(self):
    """ Returns the numeric 'added' value, or 0 if unspecified.
        Used for sorting purposes only. """
    return _get_version_added(self.attribs)

  def has_version_removed(self):
    """ Returns true if a 'removed' value was specified. """
    return _has_version_removed(self.attribs)

  def get_version_removed(self):
    """ Returns the numeric 'removed' value, or 0 if unspecified.
        Used for sorting purposes only. """
    return _get_version_removed(self.attribs)

  def removed_at_version(self, version):
    """ Returns true if this function is removed at the specified version. """
    return self.has_version_removed() and self.get_version_removed() <= version

  def exists_at_version(self, version):
    """ Returns true if this function exists at the specified version. """
    if self.has_version_added() and self.get_version_added() > version:
      return False
    return not self.removed_at_version(version)


class obj_argument:
  """ Class representing a function argument. """

  def __init__(self, parent, argval):
    if not isinstance(parent, obj_function):
      raise Exception('Invalid parent object type')

    self.parent = parent
    self.type = self.parent.parent.get_analysis(argval)

  def __repr__(self):
    result = ''
    if self.type.is_const():
      result += 'const '
    result += self.type.get_type()
    if self.type.is_byref():
      result += '&'
    elif self.type.is_byaddr():
      result += '*'
    if self.type.has_name():
      result += ' ' + self.type.get_name()
    return result

  def get_name(self):
    """ Return the name for this argument. """
    return self.type.get_name()

  def remove_name(self):
    """ Remove and return the name value. """
    name = self.type.get_name()
    self.type.name = None
    return name

  def get_type(self):
    """ Return an analysis of the argument type based on the class
        definition context.
        """
    return self.type

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    name = self.type.get_type()
    if not name in list:
      list[name] = self.type

  def needs_attrib_count_func(self):
    """ Returns true if this argument requires a 'count_func' attribute. """
    # A 'count_func' attribute is required for non-const non-string vector
    # attribute types
    return self.type.has_name() and \
        self.type.is_result_vector() and \
        not self.type.is_result_vector_string() and \
        not self.type.is_const()

  def get_attrib_count_func(self):
    """ Returns the count function for this argument. """
    # The 'count_func' attribute value format is name:function
    if not self.parent.has_attrib('count_func'):
      return None
    name = self.type.get_name()
    vals = self.parent.get_attrib_list('count_func')
    for val in vals:
      parts = val.split(':')
      if len(parts) != 2:
        raise Exception("Invalid 'count_func' attribute value for "+ \
                        self.parent.get_qualified_name()+': '+val)
      if parts[0].strip() == name:
        return parts[1].strip()
    return None

  def needs_attrib_default_retval(self):
    """ Returns true if this argument requires a 'default_retval' attribute.
        """
    # A 'default_retval' attribute is required for enumeration return value
    # types.
    return not self.type.has_name() and \
        self.type.is_result_struct() and \
        self.type.is_result_struct_enum()

  def get_attrib_default_retval(self):
    """ Returns the defualt return value for this argument. """
    return self.parent.get_attrib('default_retval')

  def get_arg_type(self):
    """ Returns the argument type as defined in translator.README.txt. """
    if not self.type.has_name():
      raise Exception('Cannot be called for retval types')

    # simple or enumeration type
    if (self.type.is_result_simple() and \
            self.type.get_type() != 'bool') or \
       (self.type.is_result_struct() and \
            self.type.is_result_struct_enum()):
      if self.type.is_byref():
        if self.type.is_const():
          return 'simple_byref_const'
        return 'simple_byref'
      elif self.type.is_byaddr():
        return 'simple_byaddr'
      return 'simple_byval'

    # boolean type
    if self.type.get_type() == 'bool':
      if self.type.is_byref():
        return 'bool_byref'
      elif self.type.is_byaddr():
        return 'bool_byaddr'
      return 'bool_byval'

    # structure type
    if self.type.is_result_struct() and self.type.is_byref():
      if self.type.is_const():
        return 'struct_byref_const'
      return 'struct_byref'

    # string type
    if self.type.is_result_string() and self.type.is_byref():
      if self.type.is_const():
        return 'string_byref_const'
      return 'string_byref'

    # *ptr type
    if self.type.is_result_ptr():
      prefix = self.type.get_result_ptr_type_prefix()
      same_side = self.parent.is_same_side(self.type.get_ptr_type())
      if self.type.is_byref():
        if same_side:
          return prefix + 'ptr_same_byref'
        return prefix + 'ptr_diff_byref'
      if same_side:
        return prefix + 'ptr_same'
      return prefix + 'ptr_diff'

    if self.type.is_result_vector():
      # all vector types must be passed by reference
      if not self.type.is_byref():
        print('ERROR: Invalid (vector not byref) type')
        return 'invalid'

      if self.type.is_result_vector_string():
        # string vector type
        if self.type.is_const():
          return 'string_vec_byref_const'
        return 'string_vec_byref'

      if self.type.is_result_vector_simple():
        if self.type.get_vector_type() != 'bool':
          # simple/enumeration vector types
          if self.type.is_const():
            return 'simple_vec_byref_const'
          return 'simple_vec_byref'

        # boolean vector types
        if self.type.is_const():
          return 'bool_vec_byref_const'
        return 'bool_vec_byref'

      if self.type.is_result_vector_ptr():
        # *ptr vector types
        prefix = self.type.get_result_vector_ptr_type_prefix()
        same_side = self.parent.is_same_side(self.type.get_ptr_type())
        if self.type.is_const():
          if same_side:
            return prefix + 'ptr_vec_same_byref_const'
          return prefix + 'ptr_vec_diff_byref_const'
        if same_side:
          return prefix + 'ptr_vec_same_byref'
        return prefix + 'ptr_vec_diff_byref'

    # string single map type
    if self.type.is_result_map_single():
      if not self.type.is_byref():
        print('ERROR: Invalid (single map not byref) type for %s' %
              self.type.get_name())
        return 'invalid'
      if self.type.is_const():
        return 'string_map_single_byref_const'
      return 'string_map_single_byref'

    # string multi map type
    if self.type.is_result_map_multi():
      if not self.type.is_byref():
        print('ERROR: Invalid (multi map not byref) type for %s' %
              self.type.get_name())
        return 'invalid'
      if self.type.is_const():
        return 'string_map_multi_byref_const'
      return 'string_map_multi_byref'

    print('ERROR: Invalid (unknown) type for %s' % self.type.get_name())
    return 'invalid'

  def get_retval_type(self):
    """ Returns the retval type as defined in translator.README.txt. """
    if self.type.has_name():
      raise Exception('Cannot be called for argument types')

    # special case for void* return value (may also be const)
    if self.type.get_type() == 'void' and self.type.is_byaddr():
      return 'simple_byaddr'

    # unsupported modifiers
    if self.type.is_const():
      print('ERROR: Invalid (const) type for retval')
      return 'invalid'
    if self.type.is_byref():
      print('ERROR: Invalid (byref) type for retval')
      return 'invalid'
    if self.type.is_byaddr():
      print('ERROR: Invalid (byaddr) type for retval')
      return 'invalid'

    # void types don't have a return value
    if self.type.get_type() == 'void':
      return 'none'

    if (self.type.is_result_simple() and \
            self.type.get_type() != 'bool') or \
       (self.type.is_result_struct() and self.type.is_result_struct_enum()):
      return 'simple'

    if self.type.get_type() == 'bool':
      return 'bool'

    if self.type.is_result_string():
      return 'string'

    if self.type.is_result_ptr():
      prefix = self.type.get_result_ptr_type_prefix()
      if self.parent.is_same_side(self.type.get_ptr_type()):
        return prefix + 'ptr_same'
      else:
        return prefix + 'ptr_diff'

    print('ERROR: Invalid (unknown) type for retval')
    return 'invalid'

  def get_retval_default(self, for_capi):
    """ Returns the default return value based on the retval type. """
    # start with the default retval attribute, if any.
    retval = self.get_attrib_default_retval()
    if not retval is None:
      if for_capi:
        # apply any appropriate C API translations.
        if retval == 'true':
          return '1'
        if retval == 'false':
          return '0'
      return retval

    # next look at the retval type value.
    type = self.get_retval_type()
    if type == 'simple':
      return self.get_type().get_result_simple_default()
    elif type == 'simple_byaddr':
      if for_capi:
        return 'NULL'
      return 'nullptr'
    elif type == 'bool':
      if for_capi:
        return '0'
      return 'false'
    elif type == 'string':
      if for_capi:
        return 'NULL'
      return 'CefString()'
    elif type == 'refptr_same' or type == 'refptr_diff' or \
         type == 'rawptr_same' or type == 'rawptr_diff':
      if for_capi:
        return 'NULL'
      return 'nullptr'
    elif type == 'ownptr_same' or type == 'ownptr_diff':
      if for_capi:
        return 'NULL'
      return 'CefOwnPtr<' + self.type.get_ptr_type() + '>()'

    return ''


class obj_analysis:
  """ Class representing an analysis of a data type value. """

  def __init__(self, scopelist, value, named):
    self.value = value
    self.result_type = 'unknown'
    self.result_value = None
    self.result_default = None
    self.ptr_type = None

    # parse the argument string
    partlist = value.strip().split()

    if named == True:
      # extract the name value
      self.name = partlist[-1]
      del partlist[-1]
    else:
      self.name = None

    if len(partlist) == 0:
      raise Exception('Invalid argument value: ' + value)

    # check const status
    if partlist[0] == 'const':
      self.isconst = True
      del partlist[0]
    else:
      self.isconst = False

    if len(partlist) == 0:
      raise Exception('Invalid argument value: ' + value)

    # combine the data type
    self.type = ' '.join(partlist)

    # extract the last character of the data type
    endchar = self.type[-1]

    # check if the value is passed by reference
    if endchar == '&':
      self.isbyref = True
      self.type = self.type[:-1]
    else:
      self.isbyref = False

    # check if the value is passed by address
    if endchar == '*':
      self.isbyaddr = True
      self.type = self.type[:-1]
    else:
      self.isbyaddr = False

    # see if the value is directly identifiable
    if self._check_advanced(self.type) == True:
      return

    # not identifiable, so look it up
    translation = None
    for scope in scopelist:
      if not isinstance(scope, obj_header) \
          and not isinstance(scope, obj_class):
        raise Exception('Invalid scope object type')
      translation = scope.get_alias_translation(self.type)
      if not translation is None:
        break

    if translation is None:
      raise Exception('Failed to translate type: ' + self.type)

    # the translation succeeded so keep the result
    self.result_type = translation.result_type
    self.result_value = translation.result_value

  def _check_advanced(self, value):
    # check for vectors
    if value.find('std::vector') == 0:
      self.result_type = 'vector'
      val = value[12:-1].strip()
      self.result_value = [self._get_basic(val)]
      self.result_value[0]['vector_type'] = val
      return True

    # check for maps
    if value.find('std::map') == 0:
      self.result_type = 'map'
      vals = value[9:-1].split(',')
      if len(vals) == 2:
        self.result_value = [
            self._get_basic(vals[0].strip()),
            self._get_basic(vals[1].strip())
        ]
        return True

    # check for multimaps
    if value.find('std::multimap') == 0:
      self.result_type = 'multimap'
      vals = value[14:-1].split(',')
      if len(vals) == 2:
        self.result_value = [
            self._get_basic(vals[0].strip()),
            self._get_basic(vals[1].strip())
        ]
        return True

    # check for basic types
    basic = self._get_basic(value)
    if not basic is None:
      self.result_type = basic['result_type']
      self.result_value = basic['result_value']
      if 'ptr_type' in basic:
        self.ptr_type = basic['ptr_type']
      if 'result_default' in basic:
        self.result_default = basic['result_default']
      return True

    return False

  def _get_basic(self, value):
    # check for string values
    if value == "CefString":
      return {'result_type': 'string', 'result_value': None}

    # check for simple direct translations
    if value in _simpletypes.keys():
      return {
          'result_type': 'simple',
          'result_value': _simpletypes[value][0],
          'result_default': _simpletypes[value][1],
      }

    # check if already a C API structure
    if value[-2:] == '_t':
      return {'result_type': 'structure', 'result_value': value}

    # check for CEF reference pointers
    p = re.compile(r'^CefRefPtr<(.*?)>$', re.DOTALL)
    list = p.findall(value)
    if len(list) == 1:
      return {
          'result_type': 'refptr',
          'result_value': get_capi_name(list[0], True) + '*',
          'ptr_type': list[0]
      }

    # check for CEF owned pointers
    p = re.compile(r'^CefOwnPtr<(.*?)>$', re.DOTALL)
    list = p.findall(value)
    if len(list) == 1:
      return {
          'result_type': 'ownptr',
          'result_value': get_capi_name(list[0], True) + '*',
          'ptr_type': list[0]
      }

    # check for CEF raw pointers
    p = re.compile(r'^CefRawPtr<(.*?)>$', re.DOTALL)
    list = p.findall(value)
    if len(list) == 1:
      return {
          'result_type': 'rawptr',
          'result_value': get_capi_name(list[0], True) + '*',
          'ptr_type': list[0]
      }

    # check for CEF structure types
    if value[0:3] == 'Cef' and value[-4:] != 'List':
      return {
          'result_type': 'structure',
          'result_value': get_capi_name(value, True)
      }

    return None

  def __repr__(self):
    return '(' + self.result_type + ') ' + str(self.result_value)

  def has_name(self):
    """ Returns true if a name value exists. """
    return (not self.name is None)

  def get_name(self):
    """ Return the name. """
    return self.name

  def get_value(self):
    """ Return the C++ value (type + name). """
    return self.value

  def get_type(self):
    """ Return the C++ type. """
    return self.type

  def get_ptr_type(self):
    """ Return the C++ class type referenced by a CefRefPtr. """
    if self.is_result_vector() and self.is_result_vector_ptr():
      # return the vector RefPtr type
      return self.result_value[0]['ptr_type']
    # return the basic RefPtr type
    return self.ptr_type

  def get_vector_type(self):
    """ Return the C++ class type referenced by a std::vector. """
    if self.is_result_vector():
      return self.result_value[0]['vector_type']
    return None

  def is_const(self):
    """ Returns true if the argument value is constant. """
    return self.isconst

  def is_byref(self):
    """ Returns true if the argument is passed by reference. """
    return self.isbyref

  def is_byaddr(self):
    """ Returns true if the argument is passed by address. """
    return self.isbyaddr

  def is_result_simple(self):
    """ Returns true if this is a simple argument type. """
    return (self.result_type == 'simple')

  def get_result_simple_type_root(self):
    """ Return the simple structure or basic type name. """
    return self.result_value

  def get_result_simple_type(self):
    """ Return the simple type. """
    result = ''
    if self.is_const():
      result += 'const '
    result += self.result_value
    if self.is_byaddr() or self.is_byref():
      result += '*'
    return result

  def get_result_simple_default(self):
    """ Return the default value fo the basic type. """
    return self.result_default

  def is_result_ptr(self):
    """ Returns true if this is a *Ptr type. """
    return self.is_result_refptr() or self.is_result_ownptr() or \
           self.is_result_rawptr()

  def get_result_ptr_type_root(self):
    """ Return the *Ptr type structure name. """
    return self.result_value[:-1]

  def get_result_ptr_type(self, defined_structs=[], version_finder=None):
    """ Return the *Ptr type. """
    result = ''
    name = self.result_value[:-1]
    if not version_finder is None:
      name = version_finder(name)
    if not name in defined_structs:
      result += 'struct _'
    result += name + self.result_value[-1]
    if self.is_byref() or self.is_byaddr():
      result += '*'
    return result

  def get_result_ptr_type_prefix(self):
    """ Returns the *Ptr type prefix. """
    if self.is_result_refptr():
      return 'ref'
    if self.is_result_ownptr():
      return 'own'
    if self.is_result_rawptr():
      return 'raw'
    raise Exception('Not a pointer type')

  def is_result_refptr(self):
    """ Returns true if this is a RefPtr type. """
    return (self.result_type == 'refptr')

  def is_result_ownptr(self):
    """ Returns true if this is a OwnPtr type. """
    return (self.result_type == 'ownptr')

  def is_result_rawptr(self):
    """ Returns true if this is a RawPtr type. """
    return (self.result_type == 'rawptr')

  def is_result_struct(self):
    """ Returns true if this is a structure type. """
    return (self.result_type == 'structure')

  def is_result_struct_enum(self):
    """ Returns true if this struct type is likely an enumeration. """
    # structure values that are passed by reference or address must be
    # structures and not enumerations
    if not self.is_byref() and not self.is_byaddr():
      return True
    return False

  def get_result_struct_type(self, defined_structs=[], version_finder=None):
    """ Return the structure or enumeration type. """
    result = ''

    name = self.result_value

    is_enum = self.is_result_struct_enum()
    if not is_enum:
      if self.is_const():
        result += 'const '
      if not self.result_value in defined_structs:
        result += 'struct _'
      if not version_finder is None:
        name = version_finder(name)

    result += name
    if not is_enum:
      result += '*'
    return result

  def is_result_string(self):
    """ Returns true if this is a string type. """
    return (self.result_type == 'string')

  def get_result_string_type(self):
    """ Return the string type. """
    if not self.has_name():
      # Return values are string structs that the user must free. Use
      # the name of the structure as a hint.
      return 'cef_string_userfree_t'
    elif not self.is_const() and (self.is_byref() or self.is_byaddr()):
      # Parameters passed by reference or address. Use the normal
      # non-const string struct.
      return 'cef_string_t*'
    # Const parameters use the const string struct.
    return 'const cef_string_t*'

  def is_result_vector(self):
    """ Returns true if this is a vector type. """
    return (self.result_type == 'vector')

  def is_result_vector_string(self):
    """ Returns true if this is a string vector. """
    return self.result_value[0]['result_type'] == 'string'

  def is_result_vector_simple(self):
    """ Returns true if this is a string vector. """
    return self.result_value[0]['result_type'] == 'simple'

  def is_result_vector_ptr(self):
    """ Returns true if this is a *Ptr vector. """
    return self.is_result_vector_refptr() or \
           self.is_result_vector_ownptr() or \
           self.is_result_vector_rawptr()

  def get_result_vector_ptr_type_prefix(self):
    """ Returns the *Ptr type prefix. """
    if self.is_result_vector_refptr():
      return 'ref'
    if self.is_result_vector_ownptr():
      return 'own'
    if self.is_result_vector_rawptr():
      return 'raw'
    raise Exception('Not a pointer type')

  def is_result_vector_refptr(self):
    """ Returns true if this is a RefPtr vector. """
    return self.result_value[0]['result_type'] == 'refptr'

  def is_result_vector_ownptr(self):
    """ Returns true if this is a OwnPtr vector. """
    return self.result_value[0]['result_type'] == 'ownptr'

  def is_result_vector_rawptr(self):
    """ Returns true if this is a RawPtr vector. """
    return self.result_value[0]['result_type'] == 'rawptr'

  def get_result_vector_type_root(self):
    """ Return the vector structure or basic type name. """
    return self.result_value[0]['result_value']

  def get_result_vector_type(self, defined_structs=[], version_finder=None):
    """ Return the vector type. """
    if not self.has_name():
      raise Exception('Cannot use vector as a return type')

    type = self.result_value[0]['result_type']
    value = self.result_value[0]['result_value']

    result = {}
    if type == 'string':
      result['value'] = 'cef_string_list_t'
      result['format'] = 'single'
      return result

    if type == 'simple':
      str = value
      if self.is_const():
        str += ' const'
      str += '*'
      result['value'] = str
    elif type == 'refptr' or type == 'ownptr' or type == 'rawptr':
      str = ''

      # remove the * suffix
      name = value[:-1]
      if not version_finder is None:
        name = version_finder(name)

      if not name in defined_structs:
        str += 'struct _'
      str += name + value[-1]
      if self.is_const():
        str += ' const'
      str += '*'
      result['value'] = str
    else:
      raise Exception('Unsupported vector type: ' + type)

    # vector values must be passed as a value array parameter
    # and a size parameter
    result['format'] = 'multi-arg'
    return result

  def is_result_map(self):
    """ Returns true if this is a map type. """
    return (self.result_type == 'map' or self.result_type == 'multimap')

  def is_result_map_single(self):
    """ Returns true if this is a single map type. """
    return (self.result_type == 'map')

  def is_result_map_multi(self):
    """ Returns true if this is a multi map type. """
    return (self.result_type == 'multimap')

  def get_result_map_type(self, defined_structs=[]):
    """ Return the map type. """
    if not self.has_name():
      raise Exception('Cannot use map as a return type')
    if self.result_value[0]['result_type'] == 'string' \
        and self.result_value[1]['result_type'] == 'string':
      if self.result_type == 'map':
        return {'value': 'cef_string_map_t', 'format': 'single'}
      elif self.result_type == 'multimap':
        return {'value': 'cef_string_multimap_t', 'format': 'multi'}
    raise Exception('Only mappings of strings to strings are supported')

  def get_capi(self, defined_structs=[], version_finder=None):
    """ Format the value for the C API. """
    result = ''
    format = 'single'
    if self.is_result_simple():
      result += self.get_result_simple_type()
    elif self.is_result_ptr():
      result += self.get_result_ptr_type(defined_structs, version_finder)
    elif self.is_result_struct():
      result += self.get_result_struct_type(defined_structs, version_finder)
    elif self.is_result_string():
      result += self.get_result_string_type()
    elif self.is_result_map():
      resdict = self.get_result_map_type(defined_structs)
      if resdict['format'] == 'single' or resdict['format'] == 'multi':
        result += resdict['value']
      else:
        raise Exception('Unsupported map type')
    elif self.is_result_vector():
      resdict = self.get_result_vector_type(defined_structs, version_finder)
      if resdict['format'] != 'single':
        format = resdict['format']
      result += resdict['value']

    if self.has_name():
      result += ' ' + self.get_name()

    return {'format': format, 'value': result}


# test the module
if __name__ == "__main__":
  import pprint
  import sys

  # verify that the correct number of command-line arguments are provided
  if len(sys.argv) != 2:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <directory>')
    sys.exit()

  pp = pprint.PrettyPrinter(indent=4)

  # create the header object
  header = obj_header()
  header.add_directory(sys.argv[1])

  # output the type mapping
  types = {}
  header.get_types(types)
  pp.pprint(types)
  sys.stdout.write('\n')

  # output the parsed C++ data
  sys.stdout.write(str(header))

  # output the C API formatted data
  defined_names = header.get_defined_structs()
  result = ''

  # global functions
  funcs = header.get_funcs()
  if len(funcs) > 0:
    for func in funcs:
      result += func.get_capi_proto(defined_names, True) + ';\n'
    result += '\n'

  classes = header.get_classes()
  for cls in classes:
    # virtual functions are inside a structure
    result += 'struct ' + cls.get_capi_name() + '\n{\n'
    funcs = cls.get_virtual_funcs()
    if len(funcs) > 0:
      for func in funcs:
        result += '\t' + func.get_capi_proto(defined_names, True) + ';\n'
    result += '}\n\n'

    defined_names.append(cls.get_capi_name())

    # static functions become global
    funcs = cls.get_static_funcs()
    if len(funcs) > 0:
      for func in funcs:
        result += func.get_capi_proto(defined_names, True) + ';\n'
      result += '\n'
  sys.stdout.write(result)
