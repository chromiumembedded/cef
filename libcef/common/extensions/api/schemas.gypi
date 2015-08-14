# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'schema_files': [
      'streams_private.idl',
    ],
    'non_compiled_schema_files': [
    ],
    
    'chromium_code': 1,
    'cc_dir': 'cef/libcef/common/extensions/api',
    # Match the Chrome namespace and bundle name to minimize code changes.
    'root_namespace': 'extensions::api::%(namespace)s',
    'bundle_name': 'Chrome',
    'impl_dir_': 'cef/libcef/browser/extensions/api',
  },
}
