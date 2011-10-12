# Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'conditions': [
      # Directory for CEF source files.
      [ 'OS=="win"', {
        'cef_directory' : '<!(echo %CEF_DIRECTORY%)',
        # Keep the build output in the CEF directory.
        'build_dir_prefix': '..\\<!(echo %CEF_DIRECTORY%)\\',
      }, { # OS!="win"
        'cef_directory' : '<!(echo $CEF_DIRECTORY)',
      }],
      [ 'OS=="mac"', {
        # Don't use the chrome style plugin with CEF.
        'clang_use_chrome_plugins': 0,
      }],
    ]
  },
}
