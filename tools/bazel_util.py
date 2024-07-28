# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

QUIET = False


def bazel_set_quiet(quiet):
  global QUIET
  QUIET = quiet


LAST_ERROR = None


def bazel_last_error():
  return LAST_ERROR


def _set_last_error(error):
  global LAST_ERROR
  LAST_ERROR = error


def _print(label, data, pos, msg):
  data_to_pos = data[0:pos]
  line = data_to_pos.count('\n')
  pos_in_line = (pos - data_to_pos.rfind('\n') - 1) if line > 0 else pos

  _set_last_error('ERROR: %s: %s at line %d:%d' % (label, msg, line + 1,
                                                   pos_in_line + 1))
  if not QUIET:
    print(LAST_ERROR)


def bazel_substitute(data,
                     variables,
                     var_delim_start='${',
                     var_delim_end='}',
                     path_list_delim=',\n    ',
                     path_relative_to=None,
                     path_abs_prefix='//',
                     label='data'):
  _set_last_error(None)

  if not path_relative_to is None and len(path_relative_to) == 0:
    path_relative_to = None
  if not path_relative_to is None and path_relative_to[-1] != '/':
    path_relative_to += '/'

  result = ''

  epos = 0
  spos = data.find(var_delim_start, epos)
  while spos >= 0:
    result += data[epos:spos]
    epos = data.find(var_delim_end, spos + len(var_delim_start))
    if epos > spos:
      sub = ''
      var = data[spos + len(var_delim_start):epos]
      if var in variables:
        val = variables[var]
        if isinstance(val, list):
          # Assumed to be a list of file paths.
          paths = []
          for path in val:
            if not path_relative_to is None and path.startswith(
                path_relative_to):
              # Use only the remainder of the path.
              path = path[len(path_relative_to):]
            else:
              # Use a fully qualified path.
              path = path_abs_prefix + path
            paths.append('"%s"' % path)
          sub = path_list_delim.join(paths)
        else:
          # Direct replacement.
          sub = str(val)
      else:
        _print(label, data, spos,
               'No value for "%s%s%s"' % (var_delim_start, var, var_delim_end))
      if len(sub) > 0:
        result += sub
      epos += len(var_delim_end)
    else:
      _print(label, data, spos,
             'Missing close bracket for "%s..."' % (data[spos:spos + 5]))
      # Can't parse any further.
      break
    spos = data.find(var_delim_start, epos)

  if epos >= 0:
    result += data[epos:]

  return result


# Test the module.
if __name__ == "__main__":
  # Don't print error messages.
  bazel_set_quiet(True)

  # No substitution
  assert (bazel_substitute('foobar', {}) == 'foobar')
  assert (bazel_last_error() == None)

  # No matching variable
  assert (bazel_substitute('${a}foobar', {}) == 'foobar')
  assert (bazel_last_error() == 'ERROR: data: No value for "${a}" at line 1:1')
  assert (bazel_substitute('\nfoo${a}bar', {}) == '\nfoobar')
  assert (bazel_last_error() == 'ERROR: data: No value for "${a}" at line 2:4')
  assert (bazel_substitute('\n\nfoobar${a}', {}) == '\n\nfoobar')
  assert (bazel_last_error() == 'ERROR: data: No value for "${a}" at line 3:7')

  # Missing end bracket.
  assert (bazel_substitute('${afoobar', {}) == '')
  assert (bazel_last_error() ==
          'ERROR: data: Missing close bracket for "${afo..." at line 1:1')
  assert (bazel_substitute('\nfoo${abar', {}) == '\nfoo')
  assert (bazel_last_error() ==
          'ERROR: data: Missing close bracket for "${aba..." at line 2:4')
  assert (bazel_substitute('\n\nfoobar${a', {}) == '\n\nfoobar')
  assert (bazel_last_error() ==
          'ERROR: data: Missing close bracket for "${a..." at line 3:7')

  # Variable substitution
  assert (bazel_substitute('foo${choo}bar', {'choo': 'blah'}) == 'fooblahbar')
  assert (bazel_last_error() == None)
  assert (bazel_substitute('${ah}${choo}bar${ba}',
                           {'ah': 'foo',
                            'choo': 5,
                            'ba': ''}) == 'foo5bar')
  assert (bazel_last_error() == None)

  # Custom variable delimiters.
  assert (bazel_substitute(
      '$$ah$$$$choo$$bar$$ba$$', {'ah': 'foo',
                                  'choo': 5,
                                  'ba': ''},
      var_delim_start='$$',
      var_delim_end='$$') == 'foo5bar')
  assert (bazel_last_error() == None)

  paths = [
      'path/to/a.ext',
      'path/to/b.ext',
      'another/path/c.ext',
  ]

  # All absolute paths.
  assert (bazel_substitute('[${paths}]', {'paths': paths}, path_list_delim=',')
          == '["//path/to/a.ext","//path/to/b.ext","//another/path/c.ext"]')
  assert (bazel_last_error() == None)

  # Some relative paths.
  assert (bazel_substitute(
      '[${paths}]', {'paths': paths},
      path_list_delim=',',
      path_relative_to='path/to') == '["a.ext","b.ext","//another/path/c.ext"]')
  assert (bazel_last_error() == None)

  print('Tests passed!')
