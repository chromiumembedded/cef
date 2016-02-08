# Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    # Don't use the chrome style plugin with CEF.
    'clang_use_chrome_plugins': 0,
    # Set ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1.
    'enable_basic_printing': 1,
    'enable_print_preview': 0,
    # Enable support for Widevine CDM.
    'enable_widevine': 1,
    # Disable support for plugin installation.
    'enable_plugin_installation': 0,
    'conditions': [
      # Directory for CEF source files.
      [ 'OS=="win"', {
        'cef_directory' : '<!(echo %CEF_DIRECTORY%)',
      }, { # OS!="win"
        'cef_directory' : '<!(echo $CEF_DIRECTORY)',
      }],
      [ 'OS=="mac"', {
        # Strip symbols and create dSYM files for the Release target.
        'mac_strip_release': 1,
      }],
      ['os_posix==1 and OS!="mac" and OS!="android"', {
        # Disable theme support on Linux so we don't need to implement
        # ThemeService[Factory] classes.
        'enable_themes': 0,
      }]
    ]
  }
}
