# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reference parser and correlator for uninstall lifecycle protocol v1."""

import json
import math
import re
import time

MAX_PAYLOAD = 32 * 1024
MAX_ERROR_MESSAGE = 4096
MAX_WARNING = 1024
MAX_WARNINGS = 32
_OPERATION_ID = re.compile(r'^[0-9a-f]{32}$')
_TERMINAL_ERROR_NAMES = {
    100: 'CONFIG_ERROR',
    101: 'NETWORK_ERROR',
    102: 'SIGNATURE_ERROR',
    103: 'NO_MATCHING_VERSION',
    104: 'EXTRACTION_ERROR',
    105: 'INSTALL_ERROR',
    106: 'DATABASE_ERROR',
    107: 'LOCK_TIMEOUT',
    108: 'CANCELLED',
    110: 'NO_SENTINEL',
    111: 'SENTINEL_READ_ERROR',
    112: 'SENTINEL_OWNER_MISMATCH',
    113: 'POLICY_DENIED',
    114: 'INDEX_ERROR',
    115: 'RECOVERY_ERROR',
    116: 'REPAIR_ERROR',
    117: 'QUARANTINE_ERROR',
    118: 'RETENTION_SNAPSHOT_CHANGED',
    119: 'POLICY_ERROR',
    199: 'UNKNOWN_ERROR',
}

DEFAULT_CORRELATION_CAPACITY = 256
DEFAULT_CORRELATION_EXPIRATION_SECONDS = 30 * 60


def _integer(value, minimum=None, maximum=None):
  if isinstance(value, bool) or not isinstance(value, int):
    return False
  return ((minimum is None or value >= minimum) and
          (maximum is None or value <= maximum))


def _uint32_number(value):
  if isinstance(value, bool) or not isinstance(value, (int, float)):
    return False
  if isinstance(value, float):
    return (math.isfinite(value) and value.is_integer() and
            1 <= value <= 0xffffffff)
  return 1 <= value <= 0xffffffff


def _bounded_utf8_string(value, maximum):
  if not isinstance(value, str):
    return False
  try:
    return len(value.encode('utf-8')) <= maximum
  except UnicodeEncodeError:
    return False


def parse_payload(payload):
  """Return (status, event), where status is valid, ignored, or malformed."""
  if not isinstance(payload, str) or '\0' in payload:
    return 'malformed', None
  try:
    encoded = payload.encode('utf-8')
  except UnicodeEncodeError:
    return 'malformed', None
  if not encoded or len(encoded) + 1 > MAX_PAYLOAD:
    return 'malformed', None
  try:
    event = json.loads(payload)
  except (TypeError, ValueError):
    return 'malformed', None
  if not isinstance(event, dict):
    return 'malformed', None
  version = event.get('protocol_version')
  name = event.get('event')
  if not _integer(version) or not isinstance(name, str):
    return 'malformed', None
  if version != 1 or name not in ('relaunch_started', 'operation_result'):
    return 'ignored', None
  operation_id = event.get('operation_id')
  if not isinstance(operation_id,
                    str) or not _OPERATION_ID.fullmatch(operation_id):
    return 'malformed', None
  if name == 'relaunch_started':
    child_pid = event.get('child_pid')
    if (not _uint32_number(child_pid) or child_pid < 1 or
        child_pid > 0xffffffff):
      return 'malformed', None
    return 'valid', event

  success = event.get('success')
  outcome = event.get('outcome')
  exit_code = event.get('exit_code')
  if (event.get('command') != 'uninstall' or not isinstance(success, bool) or
      outcome not in ('committed', 'cleanup_deferred', 'failed') or
      not _integer(exit_code)):
    return 'malformed', None
  error_fields = ('error_code', 'error_name', 'error_message')
  if success:
    if outcome == 'failed' or exit_code != 0 or any(
        k in event for k in error_fields):
      return 'malformed', None
  else:
    error_code = event.get('error_code')
    error_name = event.get('error_name')
    error_message = event.get('error_message')
    if (outcome != 'failed' or not _integer(error_code) or error_code == 0 or
        error_code != exit_code or error_code not in _TERMINAL_ERROR_NAMES or
        _TERMINAL_ERROR_NAMES[error_code] != error_name or
        not _bounded_utf8_string(error_message, MAX_ERROR_MESSAGE)):
      return 'malformed', None
  warnings = event.get('warnings', [])
  if (not isinstance(warnings, list) or len(warnings) > MAX_WARNINGS or
      any(not _bounded_utf8_string(item, MAX_WARNING) for item in warnings)):
    return 'malformed', None
  if ('diagnostics_truncated' in event and
      not isinstance(event['diagnostics_truncated'], bool)):
    return 'malformed', None
  return 'valid', event


class Correlator:
  """Bounded, expiring attribution of terminals to matching handoffs."""

  def __init__(self,
               capacity=DEFAULT_CORRELATION_CAPACITY,
               expiration_seconds=DEFAULT_CORRELATION_EXPIRATION_SECONDS,
               clock=time.monotonic):
    if capacity < 1 or expiration_seconds <= 0:
      raise ValueError('correlation bounds must be positive')
    self._capacity = capacity
    self._expiration_seconds = expiration_seconds
    self._clock = clock
    self._last_activity = {}
    self.handoffs = {}
    self.pending = {}
    self.results = {}

  def _discard(self, operation_id):
    existed = operation_id in self._last_activity
    self._last_activity.pop(operation_id, None)
    self.handoffs.pop(operation_id, None)
    self.pending.pop(operation_id, None)
    self.results.pop(operation_id, None)
    return existed

  def purge_expired(self):
    now = self._clock()
    expired = [
        operation_id
        for operation_id, last_activity in self._last_activity.items()
        if now - last_activity >= self._expiration_seconds
    ]
    for operation_id in expired:
      self._discard(operation_id)
    return len(expired)

  def _make_room(self, terminal_first):
    if len(self._last_activity) < self._capacity:
      return True
    candidates = [
        operation_id for operation_id in self.pending
        if operation_id not in self.handoffs
    ]
    if not candidates:
      if terminal_first:
        return False
      candidates = list(self._last_activity)
    oldest = min(candidates, key=self._last_activity.__getitem__)
    self._discard(oldest)
    return True

  def accept_payload(self, payload):
    status, event = parse_payload(payload)
    if status != 'valid':
      return status
    self.purge_expired()
    now = self._clock()
    operation_id = event['operation_id']
    if event['event'] == 'relaunch_started':
      if operation_id in self.handoffs:
        return 'duplicate'
      if (operation_id not in self._last_activity and
          not self._make_room(terminal_first=False)):
        return 'ignored'
      self.handoffs[operation_id] = event
      self._last_activity[operation_id] = now
      if operation_id in self.pending:
        self.results[operation_id] = self.pending.pop(operation_id)
        return 'correlated'
      return 'handoff'
    if operation_id in self.pending or operation_id in self.results:
      return 'duplicate'
    if operation_id not in self.handoffs:
      if (operation_id not in self._last_activity and
          not self._make_room(terminal_first=True)):
        return 'ignored'
      self.pending[operation_id] = event
      self._last_activity[operation_id] = now
      return 'buffered'
    self.results[operation_id] = event
    self._last_activity[operation_id] = now
    return 'correlated'

  def consume_result(self, operation_id):
    self.purge_expired()
    result = self.results.get(operation_id)
    if result is not None:
      self._discard(operation_id)
    return result

  def remove(self, operation_id):
    self.purge_expired()
    return self._discard(operation_id)

  def tracked_operation_count(self):
    self.purge_expired()
    return len(self._last_activity)

  def snapshot(self):
    self.purge_expired()
    return {
        'handoffs': dict(self.handoffs),
        'pending': dict(self.pending),
        'results': dict(self.results),
    }


def correlate_helper_output(output):
  """Parse the structured helper output and return receiver public state."""
  correlator = Correlator()
  statuses = []
  for payload in output.get('lifecycle', []):
    statuses.append(correlator.accept_payload(payload))
  state = correlator.snapshot()
  return {
      'statuses': statuses,
      **state,
  }
