# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
import json

from cef_json_builder import cef_json_builder, cef_json_encoder


class cef_installer_builder:
  """Generates installer-specific CDN query files from cef_json_builder data."""

  _WINDOWS_PLATFORMS = ('windows32', 'windows64', 'windowsarm64')

  def __init__(self, builder, revoked_json=None):
    """Create a new cef_installer_builder.

    Args:
      builder: A populated cef_json_builder instance.
      revoked_json: Optional parsed revoked.json dict (result of json.loads()).
          When provided, revoked versions are excluded from all generated output.
    """
    self._builder = builder
    # Trigger version sorting (normally only done on __repr__).
    self._builder._sort_versions()
    self._revoked_points = set()
    self._revoked_ranges = []
    if revoked_json is not None:
      for entry in revoked_json.get('revoked_versions', []):
        if 'version_min' in entry and 'version_max' in entry:
          self._revoked_ranges.append(
              (entry['version_min'], entry['version_max']))
        elif 'version' in entry:
          self._revoked_points.add(entry['version'])

  @staticmethod
  def _parse_version_tuple(version_str):
    """Parse '137.3.5' into (137, 3, 5) for comparison.

    Pads with zeros so '137' becomes (137, 0, 0) and '137.4' becomes
    (137, 4, 0).
    """
    parts = [int(x) for x in version_str.split('.')]
    while len(parts) < 3:
      parts.append(0)
    return tuple(parts)

  def _is_revoked(self, short_version):
    """Returns True if the short version is revoked (point or range)."""
    if short_version in self._revoked_points:
      return True
    v = self._parse_version_tuple(short_version)
    for vmin, vmax in self._revoked_ranges:
      if (self._parse_version_tuple(vmin) <= v <=
          self._parse_version_tuple(vmax)):
        return True
    return False

  def _get_installer_entry(self, version_obj):
    """Build one installer entry from a version record.

    Returns None if the version has no signed-type file or is revoked.
    """
    cef_version = version_obj['cef_version']
    short_version = cef_json_builder.get_short_version(cef_version)

    if self._is_revoked(short_version):
      return None

    # Find the signed file.
    signed_file = None
    for f in version_obj['files']:
      if f['type'] == 'signed':
        signed_file = f
        break

    if signed_file is None:
      return None

    entry = {
        'version': short_version,
        'file': signed_file['name'],
        'sha1': signed_file['sha1'],
        'last_modified': signed_file['last_modified'],
    }

    sandbox_compat = version_obj.get('sandbox_compat')
    if sandbox_compat is not None:
      entry['abi_hash'] = sandbox_compat

    return entry

  @staticmethod
  def _to_json(result, prettyprint):
    """Serialize result to a JSON string with sorted keys."""
    if prettyprint:
      return json.dumps(result,
                        cls=cef_json_encoder,
                        sort_keys=True,
                        indent=2,
                        separators=(',', ': '))
    return json.dumps(result, cls=cef_json_encoder, sort_keys=True)

  def get_stable_milestone(self, channel='stable'):
    """Returns the highest milestone integer in the given channel.

    Returns None if no versions exist.
    """
    highest = None
    for platform in self._builder.get_platforms():
      for version_obj in self._builder.get_versions(platform):
        if version_obj.get('channel', 'stable') != channel:
          continue
        milestone = cef_json_builder.get_milestone(version_obj['cef_version'])
        if highest is None or milestone > highest:
          highest = milestone
    return highest

  def generate_stable_txt(self, channel='stable'):
    """Returns string content for stable.txt (just the milestone number).

    Returns None if no versions exist.
    """
    milestone = self.get_stable_milestone(channel)
    if milestone is None:
      return None
    return str(milestone)

  def generate_milestone_json(self,
                              milestone,
                              channel='stable',
                              prettyprint=False):
    """Returns JSON for {milestone}.json â€” newest version per platform.

    Returns '{}' when no platforms qualify.
    """
    result = {}
    for platform in self._builder.get_platforms():
      for version_obj in self._builder.get_versions(platform):
        if version_obj.get('channel', 'stable') != channel:
          continue
        if cef_json_builder.get_milestone(
            version_obj['cef_version']) != milestone:
          continue
        entry = self._get_installer_entry(version_obj)
        if entry is not None:
          # First matching version is newest (sorted by last_modified).
          result[platform] = entry
          break

    return self._to_json(result, prettyprint)

  def generate_milestone_platform_json(self,
                                       milestone,
                                       platform,
                                       channel='stable',
                                       prettyprint=False):
    """Returns JSON for {milestone}_{platform}.json â€” all versions for a
    milestone+platform, sorted newest-first.

    Returns '[]' for unknown milestone/platform combinations.
    """
    result = []
    for version_obj in self._builder.get_versions(platform):
      if version_obj.get('channel', 'stable') != channel:
        continue
      if cef_json_builder.get_milestone(
          version_obj['cef_version']) != milestone:
        continue
      entry = self._get_installer_entry(version_obj)
      if entry is not None:
        result.append(entry)

    return self._to_json(result, prettyprint)

  def generate_abi_hash_platform_json(self,
                                      abi_hash,
                                      platform,
                                      channel='stable',
                                      prettyprint=False):
    """Returns JSON for {abi_hash}_{platform}.json â€” all builds matching
    abi_hash for a platform across all milestones, sorted newest-first.

    Only applicable to Windows platforms. Returns '[]' for non-Windows.
    """
    if platform not in self._WINDOWS_PLATFORMS:
      return '[]'

    result = []
    for version_obj in self._builder.get_versions(platform):
      if version_obj.get('channel', 'stable') != channel:
        continue
      if version_obj.get('sandbox_compat') != abi_hash:
        continue
      entry = self._get_installer_entry(version_obj)
      if entry is not None:
        result.append(entry)

    return self._to_json(result, prettyprint)

  def get_abi_hashes(self, platform=None, channel='stable'):
    """Returns the set of distinct sandbox_compat values.

    Args:
      platform: Optional platform filter. Non-Windows platforms return empty.
      channel: Channel filter (default 'stable').
    """
    if platform is not None and platform not in self._WINDOWS_PLATFORMS:
      return set()

    platforms = [platform] if platform else self._builder.get_platforms()
    hashes = set()
    for p in platforms:
      if p not in self._WINDOWS_PLATFORMS:
        continue
      for version_obj in self._builder.get_versions(p):
        if version_obj.get('channel', 'stable') != channel:
          continue
        sc = version_obj.get('sandbox_compat')
        if sc is not None:
          hashes.add(sc)
    return hashes

  def get_milestones(self, channel='stable'):
    """Returns sorted list of all milestone integers in the given channel."""
    milestones = set()
    for platform in self._builder.get_platforms():
      for version_obj in self._builder.get_versions(platform):
        if version_obj.get('channel', 'stable') != channel:
          continue
        milestones.add(
            cef_json_builder.get_milestone(version_obj['cef_version']))
    return sorted(milestones)
