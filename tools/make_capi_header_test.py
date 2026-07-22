#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for CAPI header generation.

Run from the repository root:
  python3 tools/make_capi_header_test.py
  python3 -m unittest discover -s tools -p 'make_capi_header_test.py' -v
"""

import unittest

from cef_parser import obj_header
from make_capi_header import make_capi_header
from make_capi_versions_header import make_capi_versions_header


class TestMakeCapiHeader(unittest.TestCase):

  def _create_process_util_header(self):
    header = obj_header()
    header.add_file('include/cef_command_line.h')
    header.add_file('include/cef_process_util.h')
    return header

  def test_global_function_argument_include(self):
    contents = make_capi_header(self._create_process_util_header(),
                                'cef_process_util.h')
    self.assertIn('#include "include/capi/cef_command_line_capi.h"', contents)

  def test_global_function_argument_versions_include(self):
    contents = make_capi_versions_header(self._create_process_util_header(),
                                         'cef_process_util.h')
    self.assertIn('#include "include/capi/cef_command_line_capi_versions.h"',
                  contents)


if __name__ == '__main__':
  unittest.main()
