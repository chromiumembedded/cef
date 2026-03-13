# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_json_builder import cef_json_builder
from cef_installer_builder import cef_installer_builder
import datetime
import json
import unittest


class TestCefInstallerBuilder(unittest.TestCase):

  def setUp(self):
    # Allow sandbox_compat queries for all milestones in tests.
    self._orig_min = cef_json_builder.MIN_SANDBOX_COMPAT_MILESTONE
    cef_json_builder.MIN_SANDBOX_COMPAT_MILESTONE = 0

  def tearDown(self):
    cef_json_builder.MIN_SANDBOX_COMPAT_MILESTONE = self._orig_min

  # Helper to create a builder pre-populated with version data.
  # Avoids remote queries by setting chromium_version and sandbox_compat.
  def _create_builder(self):
    return cef_json_builder(silent=True)

  def _add_version(self,
                   builder,
                   platform,
                   cef_version,
                   chromium_version,
                   types=None,
                   channel='stable',
                   sandbox_compat=None,
                   last_modified=None):
    """Add a version with specified file types to the builder.

    Args:
      types: List of distribution types to add. Defaults to ['signed'].
      last_modified: datetime or string. Defaults to a fixed time.
    """
    if types is None:
      types = ['signed']
    if last_modified is None:
      last_modified = '2026-02-10T12:00:00.000Z'

    # Pre-populate to avoid queries.
    builder.set_chromium_version(cef_version, chromium_version)
    if sandbox_compat is not None:
      builder.set_sandbox_compat(cef_version, sandbox_compat)

    for t in types:
      name = cef_json_builder.get_file_name(cef_version, platform, t,
                                            channel) + '.tar.bz2'
      builder.add_file(name, 12345, last_modified, 'a' * 40)

  # --- test_get_stable_milestone ---

  def test_get_stable_milestone(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '136.0.3+g1111111+chromium-136.0.6778.0', '136.0.6778.0')
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')

    ib = cef_installer_builder(builder)
    self.assertEqual(137, ib.get_stable_milestone())

  def test_get_stable_milestone_empty(self):
    builder = self._create_builder()
    ib = cef_installer_builder(builder)
    self.assertIsNone(ib.get_stable_milestone())

  # --- test_generate_stable_txt ---

  def test_generate_stable_txt(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')

    ib = cef_installer_builder(builder)
    self.assertEqual('137', ib.generate_stable_txt())

  def test_generate_stable_txt_empty(self):
    builder = self._create_builder()
    ib = cef_installer_builder(builder)
    self.assertIsNone(ib.generate_stable_txt())

  # --- test_generate_milestone_json ---

  def test_generate_milestone_json_basic(self):
    builder = self._create_builder()
    # Add versions on two platforms (use different cef_versions to avoid
    # sandbox_compat cache sharing across platforms).
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890')
    self._add_version(builder, 'linux64',
                      '137.3.4+g1111111+chromium-137.0.7204.5', '137.0.7204.5')

    ib = cef_installer_builder(builder)
    result = json.loads(ib.generate_milestone_json(137))

    # Both platforms present.
    self.assertIn('windows64', result)
    self.assertIn('linux64', result)

    # Windows has abi_hash.
    self.assertEqual('137.3.5', result['windows64']['version'])
    self.assertEqual('abc123def4567890', result['windows64']['abi_hash'])
    self.assertIn('signed', result['windows64']['file'])

    # Linux has no abi_hash.
    self.assertEqual('137.3.4', result['linux64']['version'])
    self.assertNotIn('abi_hash', result['linux64'])

  def test_generate_milestone_json_newest_version(self):
    builder = self._create_builder()
    # Older version added first with earlier timestamp.
    self._add_version(builder,
                      'linux64',
                      '137.1.1+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      last_modified='2026-02-08T12:00:00.000Z')
    # Newer version added second with later timestamp.
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      last_modified='2026-02-10T12:00:00.000Z')

    ib = cef_installer_builder(builder)
    result = json.loads(ib.generate_milestone_json(137))

    # Should pick the newest version.
    self.assertEqual('137.3.5', result['linux64']['version'])

  def test_generate_milestone_json_no_abi_hash(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')

    ib = cef_installer_builder(builder)
    result = json.loads(ib.generate_milestone_json(137))

    self.assertNotIn('abi_hash', result['linux64'])

  def test_generate_milestone_json_omits_platforms_without_signed(self):
    builder = self._create_builder()
    # Only add 'minimal' type, no 'signed'.
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      types=['minimal'])

    ib = cef_installer_builder(builder)
    result = json.loads(ib.generate_milestone_json(137))

    self.assertNotIn('linux64', result)

  def test_generate_milestone_json_empty(self):
    builder = self._create_builder()
    ib = cef_installer_builder(builder)
    self.assertEqual('{}', ib.generate_milestone_json(999))

  # --- test_generate_milestone_platform_json ---

  def test_generate_milestone_platform_json_ordering(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'linux64',
                      '137.1.1+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      last_modified='2026-02-08T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      last_modified='2026-02-10T12:00:00.000Z')

    ib = cef_installer_builder(builder)
    result = json.loads(ib.generate_milestone_platform_json(137, 'linux64'))

    self.assertEqual(2, len(result))
    # Newest first.
    self.assertEqual('137.3.5', result[0]['version'])
    self.assertEqual('137.1.1', result[1]['version'])

  def test_generate_milestone_platform_json_filters_channel(self):
    builder = self._create_builder()
    # Stable version.
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      channel='stable')
    # Beta version.
    self._add_version(builder,
                      'linux64',
                      '137.4.0+g2222222+chromium-137.0.7204.8',
                      '137.0.7204.8',
                      channel='beta')

    ib = cef_installer_builder(builder)

    # Stable query should only return stable.
    result_stable = json.loads(
        ib.generate_milestone_platform_json(137, 'linux64', channel='stable'))
    self.assertEqual(1, len(result_stable))
    self.assertEqual('137.3.5', result_stable[0]['version'])

    # Beta query should only return beta.
    result_beta = json.loads(
        ib.generate_milestone_platform_json(137, 'linux64', channel='beta'))
    self.assertEqual(1, len(result_beta))
    self.assertEqual('137.4.0', result_beta[0]['version'])

  def test_generate_milestone_platform_json_empty(self):
    builder = self._create_builder()
    ib = cef_installer_builder(builder)
    self.assertEqual('[]', ib.generate_milestone_platform_json(999, 'linux64'))

  # --- test_generate_abi_hash_platform_json ---

  def test_generate_abi_hash_platform_json_basic(self):
    builder = self._create_builder()
    # Same abi_hash across two milestones.
    self._add_version(builder,
                      'windows64',
                      '136.0.3+g1111111+chromium-136.0.6778.0',
                      '136.0.6778.0',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-05T12:00:00.000Z')
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-10T12:00:00.000Z')

    ib = cef_installer_builder(builder)
    result = json.loads(
        ib.generate_abi_hash_platform_json('abc123def4567890', 'windows64'))

    # Both milestones present.
    self.assertEqual(2, len(result))

  def test_generate_abi_hash_platform_json_ordering(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'windows64',
                      '136.0.3+g1111111+chromium-136.0.6778.0',
                      '136.0.6778.0',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-05T12:00:00.000Z')
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-10T12:00:00.000Z')

    ib = cef_installer_builder(builder)
    result = json.loads(
        ib.generate_abi_hash_platform_json('abc123def4567890', 'windows64'))

    # Newest first.
    self.assertEqual('137.3.5', result[0]['version'])
    self.assertEqual('136.0.3', result[1]['version'])

  def test_generate_abi_hash_platform_json_excludes_other_hashes(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-10T12:00:00.000Z')
    self._add_version(builder,
                      'windows64',
                      '138.0.1+g3333333+chromium-138.0.7204.0',
                      '138.0.7204.0',
                      sandbox_compat='def456abc7890123',
                      last_modified='2026-02-12T12:00:00.000Z')

    ib = cef_installer_builder(builder)
    result = json.loads(
        ib.generate_abi_hash_platform_json('abc123def4567890', 'windows64'))

    self.assertEqual(1, len(result))
    self.assertEqual('137.3.5', result[0]['version'])

  def test_generate_abi_hash_platform_json_non_windows(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')

    ib = cef_installer_builder(builder)
    self.assertEqual(
        '[]', ib.generate_abi_hash_platform_json('abc123def4567890', 'linux64'))
    self.assertEqual(
        '[]', ib.generate_abi_hash_platform_json('abc123def4567890',
                                                 'macosx64'))

  # --- test_get_abi_hashes ---

  def test_get_abi_hashes(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890')
    self._add_version(builder,
                      'windows64',
                      '138.0.1+g3333333+chromium-138.0.7204.0',
                      '138.0.7204.0',
                      sandbox_compat='def456abc7890123')

    ib = cef_installer_builder(builder)
    hashes = ib.get_abi_hashes(platform='windows64')
    self.assertEqual({'abc123def4567890', 'def456abc7890123'}, hashes)

  def test_get_abi_hashes_non_windows(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')

    ib = cef_installer_builder(builder)
    self.assertEqual(set(), ib.get_abi_hashes(platform='linux64'))

  # --- test_get_milestones ---

  def test_get_milestones(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')
    self._add_version(builder, 'linux64',
                      '136.0.3+g1111111+chromium-136.0.6778.0', '136.0.6778.0')

    ib = cef_installer_builder(builder)
    self.assertEqual([136, 137], ib.get_milestones())

  # --- test_skips_versions_without_signed_file ---

  def test_skips_versions_without_signed_file(self):
    builder = self._create_builder()
    # Only add 'minimal' and 'client' types, no 'signed'.
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      types=['minimal', 'client'])

    ib = cef_installer_builder(builder)
    result = json.loads(ib.generate_milestone_platform_json(137, 'linux64'))
    self.assertEqual(0, len(result))

  # --- test_revoked_versions_excluded ---

  def test_revoked_versions_excluded(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'linux64',
                      '137.1.0+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      last_modified='2026-02-08T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      last_modified='2026-02-10T12:00:00.000Z')

    revoked = {
        'revoked_versions': [{
            'version': '137.1.0',
            'reason': 'CVE-2024-XXXXX',
            'revoked_at': '2024-01-15T00:00:00Z'
        }]
    }
    ib = cef_installer_builder(builder, revoked_json=revoked)

    # milestone_platform_json should exclude the revoked version.
    result = json.loads(ib.generate_milestone_platform_json(137, 'linux64'))
    self.assertEqual(1, len(result))
    self.assertEqual('137.3.5', result[0]['version'])

    # milestone_json should pick non-revoked newest.
    result_milestone = json.loads(ib.generate_milestone_json(137))
    self.assertEqual('137.3.5', result_milestone['linux64']['version'])

  def test_revoked_versions_excluded_from_abi_hash(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'windows64',
                      '137.1.0+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-08T12:00:00.000Z')
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890',
                      last_modified='2026-02-10T12:00:00.000Z')

    revoked = {
        'revoked_versions': [{
            'version': '137.1.0',
            'reason': 'test',
            'revoked_at': '2024-01-15T00:00:00Z'
        }]
    }
    ib = cef_installer_builder(builder, revoked_json=revoked)
    result = json.loads(
        ib.generate_abi_hash_platform_json('abc123def4567890', 'windows64'))

    self.assertEqual(1, len(result))
    self.assertEqual('137.3.5', result[0]['version'])

  def test_revoked_range_excludes_matching_versions(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'linux64',
                      '137.1.0+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      last_modified='2026-02-08T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '137.2.0+g2222222+chromium-137.0.7204.2',
                      '137.0.7204.2',
                      last_modified='2026-02-09T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      last_modified='2026-02-10T12:00:00.000Z')

    revoked = {
        'revoked_versions': [{
            'version_min': '137.1.0',
            'version_max': '137.2.99',
            'reason': 'CVE-2024-YYYYY',
            'revoked_at': '2024-02-01T00:00:00Z'
        }]
    }
    ib = cef_installer_builder(builder, revoked_json=revoked)

    result = json.loads(ib.generate_milestone_platform_json(137, 'linux64'))
    self.assertEqual(1, len(result))
    self.assertEqual('137.3.5', result[0]['version'])

  def test_revoked_range_and_point_combined(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'linux64',
                      '137.1.0+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      last_modified='2026-02-08T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '137.2.0+g2222222+chromium-137.0.7204.2',
                      '137.0.7204.2',
                      last_modified='2026-02-09T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      last_modified='2026-02-10T12:00:00.000Z')

    revoked = {
        'revoked_versions': [
            {
                'version': '137.3.5',
                'reason': 'point revocation',
                'revoked_at': '2024-01-15T00:00:00Z'
            },
            {
                'version_min': '137.1.0',
                'version_max': '137.1.99',
                'reason': 'range revocation',
                'revoked_at': '2024-02-01T00:00:00Z'
            },
        ]
    }
    ib = cef_installer_builder(builder, revoked_json=revoked)

    # Only 137.2.0 survives.
    result = json.loads(ib.generate_milestone_platform_json(137, 'linux64'))
    self.assertEqual(1, len(result))
    self.assertEqual('137.2.0', result[0]['version'])

  def test_revoked_range_partial_version_specs(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'linux64',
                      '137.1.0+g1111111+chromium-137.0.7204.0',
                      '137.0.7204.0',
                      last_modified='2026-02-08T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '138.0.0+g2222222+chromium-138.0.7300.0',
                      '138.0.7300.0',
                      last_modified='2026-02-09T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '138.3.0+g3333333+chromium-138.0.7300.3',
                      '138.0.7300.3',
                      last_modified='2026-02-10T12:00:00.000Z')
    self._add_version(builder,
                      'linux64',
                      '139.0.0+g4444444+chromium-139.0.7400.0',
                      '139.0.7400.0',
                      last_modified='2026-02-11T12:00:00.000Z')

    # "138" means 138.0.0, "138.2" means 138.2.0 â€” revokes 138.0.0 but
    # not 138.3.0.
    revoked = {
        'revoked_versions': [{
            'version_min': '138',
            'version_max': '138.2',
            'reason': 'partial version spec test',
            'revoked_at': '2024-02-01T00:00:00Z'
        }]
    }
    ib = cef_installer_builder(builder, revoked_json=revoked)

    result = json.loads(ib.generate_milestone_platform_json(138, 'linux64'))
    self.assertEqual(1, len(result))
    self.assertEqual('138.3.0', result[0]['version'])

  # --- Round-trip tests ---

  def test_round_trip_milestone_json(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890')

    ib = cef_installer_builder(builder)
    output = ib.generate_milestone_json(137)
    parsed = json.loads(output)
    self.assertIsInstance(parsed, dict)
    self.assertIn('windows64', parsed)

  def test_round_trip_milestone_platform_json(self):
    builder = self._create_builder()
    self._add_version(builder, 'linux64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6', '137.0.7204.6')

    ib = cef_installer_builder(builder)
    output = ib.generate_milestone_platform_json(137, 'linux64')
    parsed = json.loads(output)
    self.assertIsInstance(parsed, list)
    self.assertEqual(1, len(parsed))

  def test_round_trip_abi_hash_platform_json(self):
    builder = self._create_builder()
    self._add_version(builder,
                      'windows64',
                      '137.3.5+g62d140e+chromium-137.0.7204.6',
                      '137.0.7204.6',
                      sandbox_compat='abc123def4567890')

    ib = cef_installer_builder(builder)
    output = ib.generate_abi_hash_platform_json('abc123def4567890', 'windows64')
    parsed = json.loads(output)
    self.assertIsInstance(parsed, list)
    self.assertEqual(1, len(parsed))


class TestVersionTupleComparison(unittest.TestCase):
  """Verify _parse_version_tuple comparison matches base::Version semantics.

  base::Version treats missing trailing components as zero, so
  Version("137") == Version("137.0") == Version("137.0.0"). The Python
  tuple comparison must agree.
  """

  _parse = staticmethod(cef_installer_builder._parse_version_tuple)

  def test_single_component_equals_padded(self):
    self.assertEqual(self._parse('137'), self._parse('137.0.0'))

  def test_two_components_equals_padded(self):
    self.assertEqual(self._parse('137.4'), self._parse('137.4.0'))

  def test_three_components_unchanged(self):
    self.assertEqual(self._parse('137.3.5'), (137, 3, 5))

  def test_less_than_across_component_counts(self):
    self.assertLess(self._parse('137'), self._parse('137.0.1'))
    self.assertLess(self._parse('137.3'), self._parse('137.3.1'))
    self.assertLess(self._parse('136'), self._parse('137'))

  def test_greater_than_across_component_counts(self):
    self.assertGreater(self._parse('138'), self._parse('137.99.99'))
    self.assertGreater(self._parse('137.4'), self._parse('137.3.99'))

  def test_range_check_with_partial_bounds(self):
    """Simulate the range check: version_min <= version <= version_max."""
    v = self._parse('137.3.5')
    self.assertTrue(self._parse('137') <= v <= self._parse('137.99'))
    self.assertTrue(self._parse('137.3') <= v <= self._parse('137.3.5'))
    self.assertFalse(self._parse('137.3') <= v <= self._parse('137.3'))
    self.assertFalse(self._parse('137.4') <= v <= self._parse('138'))
    self.assertFalse(self._parse('137') <= v <= self._parse('137.3.4'))


# Program entry point.
if __name__ == '__main__':
  unittest.main()
