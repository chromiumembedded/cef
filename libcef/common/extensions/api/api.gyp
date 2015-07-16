# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cef_api',
      'type': 'static_library',
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../../build/json_schema_bundle_compile.gypi',
        '../../../../../build/json_schema_compile.gypi',
        'schemas.gypi',
      ],
    },
  ],
}
