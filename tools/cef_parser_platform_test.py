# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file.

import unittest

from cef_parser import get_version_surround
from generator_test_util import make_fixture_header
from make_capi_header import make_capi_header
from make_capi_versions_header import make_capi_versions_header
from make_cpptoc_impl import make_cpptoc_global_impl
from make_ctocpp_impl import make_ctocpp_global_impl
from make_libcef_dll_dylib_impl import make_libcef_dll_dylib_impl


class PlatformFunctionTest(unittest.TestCase):

  def test_platform_guards_cover_all_binding_routes(self):
    for attributes, predicate in (
        ('platform=win', 'defined(OS_WIN)'), ('platform=mac',
                                              'defined(OS_MAC)'),
        ('platform=linux',
         'defined(OS_LINUX)'), ('platform=win,platform=mac',
                                'defined(OS_WIN) || defined(OS_MAC)'),
        ('platform=!mac',
         '!defined(OS_MAC)'), ('platform=!win,platform=!mac',
                               '!defined(OS_WIN) && !defined(OS_MAC)'),
        ('platform=win,platform=!mac', '!defined(OS_MAC) && (defined(OS_WIN))'),
        ('platform=win,platform=mac,platform=!mac',
         '!defined(OS_MAC) && (defined(OS_WIN) || defined(OS_MAC))')):
      with self.subTest(attributes=attributes):
        header = make_fixture_header()
        filename = 'cef_fixture_platform.h'
        header.add_data(
            filename, '''
///
/// Platform fixture function.
///
/*--cef(added=next,%s)--*/
int CefFixturePlatform();
''' % attributes)
        platform_guard = '#if ' + predicate
        guard = platform_guard + '\n#if CEF_API_ADDED(CEF_NEXT)\n'
        outputs = [
            make_capi_header(header, filename),
            make_cpptoc_global_impl(header, '')[0],
            make_ctocpp_global_impl(header, '')[0],
            make_libcef_dll_dylib_impl(header),
        ]
        for output in outputs:
          self.assertIn(guard, output)
          # Every occurrence in the dylib table, initialization, and function
          # definition must stay within the platform/version guard.
          guards = []
          for line in output.splitlines():
            if line.startswith('#if'):
              guards.append(line == platform_guard)
            elif line.startswith('#endif') and guards:
              guards.pop()
            if 'cef_fixture_platform(' in line or \
                'INIT_ENTRY(cef_fixture_platform)' in line or \
                'decltype(&cef_fixture_platform)' in line:
              self.assertTrue(any(guards), line)
        versions = make_capi_versions_header(header, filename)
        self.assertIn(platform_guard + '\nCEF_EXPORT int cef_fixture_platform',
                      versions)

  def test_unversioned_platform_guard_and_invalid_platform(self):
    header = make_fixture_header()
    header.add_data(
        'cef_fixture_platform.h', '''
///
/// Platform fixture function.
///
/*--cef(platform=win)--*/
int CefFixturePlatform();
''')
    self.assertEqual(get_version_surround(header.get_funcs()[-1]),
                     ('#if defined(OS_WIN)\n', '#endif\n'))
    with self.assertRaisesRegex(Exception, 'Invalid platform attribute'):
      header.add_data(
          'cef_fixture_invalid.h', '''
///
/// Invalid platform fixture.
///
/*--cef(platform=unknown)--*/
int CefFixtureInvalid();
''')

  def test_platform_rejected_on_classes_and_methods(self):
    for class_attributes, declaration, message in ((
        'source=library,platform=win', '', 'Invalid attribute key "platform"'),
                                                   ('source=library', '''
  ///
  /// Virtual method fixture.
  ///
  /*--cef(platform=win)--*/
  virtual int GetValue() = 0;
''', 'Invalid platform attribute'), ('source=library', '''
  ///
  /// Static method fixture.
  ///
  /*--cef(platform=win)--*/
  static int GetValue();
''', 'Invalid platform attribute')):
      with self.subTest(class_attributes=class_attributes,
                        declaration=declaration):
        header = make_fixture_header()
        with self.assertRaisesRegex(Exception, message):
          header.add_data(
              'cef_fixture_invalid.h', '''
///
/// Invalid platform fixture class.
///
/*--cef(%s)--*/
class CefFixtureInvalid : public virtual CefBaseRefCounted {%s
};
''' % (class_attributes, declaration))


if __name__ == '__main__':
  unittest.main()
