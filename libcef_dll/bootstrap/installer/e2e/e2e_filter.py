#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test-selection helpers for the E2E runner.

`compile_filter` turns a pytest-style -k expression into a predicate over a
test id; `flatten_suite` yields individual test cases from a nested suite.
Kept separate from run_e2e_tests.py so it is unit-testable without a build.
"""

import ast
import unittest


def _is_boolean_expr(node):
  """True if `node` is built only from and/or/not over bare keywords.

  Anything else -- a binary op (`a-b`, `a+b`), attribute access (`mod.Class`),
  a call, a comparison, a literal -- is not a keyword expression we evaluate;
  the caller treats the whole string as a literal substring instead.
  """
  if isinstance(node, ast.Expression):
    return _is_boolean_expr(node.body)
  if isinstance(node, ast.BoolOp):
    return all(_is_boolean_expr(v) for v in node.values)
  if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
    return _is_boolean_expr(node.operand)
  return isinstance(node, ast.Name)


def compile_filter(expr):
  """Compile a pytest-style -k expression into predicate(test_id) -> bool.

  Supports `and`, `or`, `not`, parentheses, and bare keywords matched
  case-insensitively as substrings of the test id. Any expression that is not
  a pure boolean combination of bare keywords -- because it fails to parse
  (`a or`) or uses non-keyword syntax (`a-b`, `mod.Class.test_x`, `a + b`) --
  falls back to plain substring containment of the whole string, so any
  single-keyword usage works regardless of the characters it contains.
  """
  try:
    tree = ast.parse(expr, mode='eval')
  except SyntaxError:
    tree = None

  if tree is None or not _is_boolean_expr(tree):
    return lambda test_id: expr.lower() in test_id.lower()

  def evaluate(node, test_id):
    if isinstance(node, ast.Expression):
      return evaluate(node.body, test_id)
    if isinstance(node, ast.BoolOp):
      results = [evaluate(v, test_id) for v in node.values]
      return all(results) if isinstance(node.op, ast.And) else any(results)
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
      return not evaluate(node.operand, test_id)
    # _is_boolean_expr guarantees this is an ast.Name.
    return node.id.lower() in test_id.lower()

  return lambda test_id: evaluate(tree, test_id)


def flatten_suite(suite):
  """Yield individual test cases from an arbitrarily nested TestSuite."""
  for item in suite:
    if isinstance(item, unittest.TestSuite):
      yield from flatten_suite(item)
    else:
      yield item
