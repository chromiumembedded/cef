# Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'conditions': [
      # Directory for CEF source files.
      [ 'OS=="win"', {
        'cef_directory' : '<!(echo %CEF_DIRECTORY%)',
        # Use SKIA text rendering for transparency support.
        'enable_skia_text': 1,
      }, { # OS!="win"
        'cef_directory' : '<!(echo $CEF_DIRECTORY)',
      }],
      [ 'OS=="mac"', {
        # Don't use the chrome style plugin with CEF.
        'clang_use_chrome_plugins': 0,
      }],
    ]
  }, 'conditions': [
    ['OS=="mac" and clang==1', {
      'target_defaults': {
        'xcode_settings': {
          # Temporary workaround for an Xcode 4 build error. This can be
          # removed with the next Chromium update. See crbug.com/156530.
          'WARNING_CFLAGS': ['-Wno-unknown-warning-option'],
        },
      },
    }],
    ['os_posix==1 and OS!="mac" and OS!="android"', {
      'target_defaults': {
        'cflags_cc': ['-Wno-deprecated-declarations'],
      },
    }]
  ]
}
