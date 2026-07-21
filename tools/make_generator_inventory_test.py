# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""Inventory guard for direct make_* tools."""

import os
import unittest

EXPECTED_GENERATOR_COUNT = 18


def generator_files(tools_directory):
  return sorted(filename for filename in os.listdir(tools_directory)
                if filename.startswith('make_') and filename.endswith('.py') and
                not filename.endswith('_test.py'))


def missing_test_modules(tools_directory, generators=None):
  if generators is None:
    generators = generator_files(tools_directory)
  return sorted(filename[:-3] + '_test.py'
                for filename in generators
                if not os.path.isfile(
                    os.path.join(tools_directory, filename[:-3] + '_test.py')))


class MakeGeneratorInventoryTest(unittest.TestCase):

  def setUp(self):
    self.tools_directory = os.path.dirname(__file__)

  def test_expected_generator_count(self):
    self.assertEqual(len(generator_files(self.tools_directory)),
                     EXPECTED_GENERATOR_COUNT)

  def test_matching_test_modules(self):
    missing = missing_test_modules(self.tools_directory)
    self.assertEqual(missing, [], 'Missing matching tests: %s' % missing)

  def test_missing_test_diagnostic_is_stably_sorted(self):
    generators = ['make_z.py', 'make_a.py']
    missing = missing_test_modules('/definitely/missing', generators)
    self.assertEqual(missing, ['make_a_test.py', 'make_z_test.py'])

  def test_nested_generators_and_tests_are_not_inventory_entries(self):
    self.assertNotIn('nested/make_nested.py',
                     generator_files(self.tools_directory))
    self.assertNotIn('make_revocation_resource_test.py',
                     generator_files(self.tools_directory))


if __name__ == '__main__':
  unittest.main()
