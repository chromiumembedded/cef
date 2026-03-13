#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for e2e_filter (no build directory required)."""

import unittest

from e2e_filter import compile_filter, flatten_suite


class CompileFilterTest(unittest.TestCase):

  def test_single_keyword_substring(self):
    m = compile_filter('launch')
    self.assertTrue(m('test_launch_success (mod.C.test_launch_success)'))
    self.assertFalse(m('test_install_flow (mod.C.test_install_flow)'))

  def test_case_insensitive(self):
    self.assertTrue(compile_filter('LAUNCH')('xx_launch_yy'))
    self.assertTrue(compile_filter('launch')('XX_LAUNCH_YY'))

  def test_or(self):
    m = compile_filter('alpha or beta')
    self.assertTrue(m('zz_alpha_zz'))
    self.assertTrue(m('zz_beta_zz'))
    self.assertFalse(m('zz_gamma_zz'))

  def test_and(self):
    m = compile_filter('alpha and beta')
    self.assertTrue(m('alpha_beta'))
    self.assertFalse(m('alpha_only'))

  def test_not(self):
    m = compile_filter('not alpha')
    self.assertTrue(m('beta'))
    self.assertFalse(m('alpha'))

  def test_parentheses_precedence(self):
    m = compile_filter('(alpha or beta) and not gamma')
    self.assertTrue(m('alpha_x'))
    self.assertTrue(m('beta_x'))
    self.assertFalse(m('alpha_gamma'))
    self.assertFalse(m('delta'))

  def test_precedence_without_parens(self):
    # `and` binds tighter than `or`: 'a or (b and c)'.
    m = compile_filter('alpha or beta and gamma')
    self.assertTrue(m('alpha_only'))  # alpha satisfies the or
    self.assertTrue(m('beta_gamma'))  # beta and gamma together
    self.assertFalse(m('beta_only'))  # beta without gamma -> false

  def test_hyphenated_keyword_falls_back_to_substring(self):
    # Hyphen parses as subtraction (BinOp), not a keyword -> literal substring.
    m = compile_filter('a-b')
    self.assertTrue(m('zz_a-b_zz'))
    self.assertFalse(m('zz_ab_zz'))

  def test_malformed_expression_falls_back_to_substring(self):
    # Trailing operator is a genuine SyntaxError -> literal substring match.
    m = compile_filter('alpha or')
    self.assertTrue(m('zz_alpha or_zz'))
    self.assertFalse(m('zz_alpha_zz'))

  def test_python_keyword_falls_back_to_substring(self):
    # A bare Python keyword (`import`) is a SyntaxError in eval mode -> literal.
    m = compile_filter('import')
    self.assertTrue(m('test_import_flow'))
    self.assertFalse(m('test_export_flow'))

  def test_empty_expression_matches_everything(self):
    # '' is a SyntaxError -> substring fallback, and '' is in every string.
    # The runner guards this with `if args.filter:`, but pin the module behavior.
    m = compile_filter('')
    self.assertTrue(m('test_anything'))
    self.assertTrue(m(''))

  def test_no_match_returns_false(self):
    self.assertFalse(compile_filter('nonexistent_xyz')('test_anything'))

  def test_non_boolean_expression_falls_back_to_substring(self):
    # Parseable but not a boolean expr (binary +) -> literal substring match.
    m = compile_filter('a + b')
    self.assertTrue(m('zz_a + b_zz'))
    self.assertFalse(m('zz_ab_zz'))

  def test_dotted_keyword_falls_back_to_substring(self):
    # Qualified name parses as ast.Attribute -> literal substring match.
    m = compile_filter('mod.Class.test_x')
    self.assertTrue(m('test_x (mod.Class.test_x)'))
    self.assertFalse(m('test_y (mod.Class.test_y)'))


class FlattenSuiteTest(unittest.TestCase):

  class _Dummy(unittest.TestCase):

    def test_noop(self):
      pass

  def test_flattens_nested(self):
    inner = unittest.TestSuite([self._Dummy('test_noop')])
    outer = unittest.TestSuite([unittest.TestSuite([inner])])
    flat = list(flatten_suite(outer))
    self.assertEqual(1, len(flat))
    self.assertIsInstance(flat[0], self._Dummy)

  def test_empty_suite_yields_nothing(self):
    self.assertEqual([], list(flatten_suite(unittest.TestSuite())))

  def test_flat_suite(self):
    suite = unittest.TestSuite([self._Dummy('test_noop')])
    flat = list(flatten_suite(suite))
    self.assertEqual(1, len(flat))
    self.assertIsInstance(flat[0], self._Dummy)

  def test_mixed_flat_and_nested_preserves_order(self):
    a = self._Dummy('test_noop')
    b = self._Dummy('test_noop')
    c = self._Dummy('test_noop')
    # A bare test, then a nested sub-suite, then another bare test.
    suite = unittest.TestSuite([a, unittest.TestSuite([b]), c])
    self.assertEqual([a, b, c], list(flatten_suite(suite)))


if __name__ == '__main__':
  unittest.main()
