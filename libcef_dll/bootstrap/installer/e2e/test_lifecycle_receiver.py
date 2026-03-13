# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for the lifecycle reference receiver."""

import json
import unittest

from lifecycle_receiver import Correlator, parse_payload

ID_A = '0123456789abcdef0123456789abcdef'
ID_B = 'fedcba9876543210fedcba9876543210'
ID_C = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'


def handoff(operation_id=ID_A, pid=123):
  return json.dumps(
      {
          'protocol_version': 1,
          'event': 'relaunch_started',
          'operation_id': operation_id,
          'child_pid': pid,
      },
      separators=(',', ':'))


def terminal(operation_id=ID_A, success=True):
  value = {
      'protocol_version': 1,
      'event': 'operation_result',
      'operation_id': operation_id,
      'command': 'uninstall',
      'success': success,
      'outcome': 'committed' if success else 'failed',
      'exit_code': 0 if success else 106,
  }
  if not success:
    value.update({
        'error_code': 106,
        'error_name': 'DATABASE_ERROR',
        'error_message': 'Database save failed',
    })
  return json.dumps(value, separators=(',', ':'))


class TestLifecycleReceiver(unittest.TestCase):

  def test_parse_valid_and_unknown(self):
    self.assertEqual('valid', parse_payload(handoff())[0])
    self.assertEqual('valid', parse_payload(terminal(success=False))[0])
    future = json.loads(handoff())
    future['protocol_version'] = 2
    self.assertEqual('ignored', parse_payload(json.dumps(future))[0])
    future['protocol_version'] = 1
    future['event'] = 'future'
    self.assertEqual('ignored', parse_payload(json.dumps(future))[0])

  def test_parse_rejects_malformed_and_bounds(self):
    malformed = [
        '', '[]', '{',
        handoff(pid=0),
        handoff(pid=True),
        handoff(operation_id=ID_A.upper()),
        terminal().replace('"exit_code":0', '"exit_code":1'),
        terminal() + '\0'
    ]
    for payload in malformed:
      self.assertEqual('malformed', parse_payload(payload)[0], payload)
    value = json.loads(terminal(success=False))
    value['error_message'] = 'x' * 4097
    self.assertEqual('malformed', parse_payload(json.dumps(value))[0])
    value = json.loads(terminal())
    value['diagnostics_truncated'] = 1
    self.assertEqual('malformed', parse_payload(json.dumps(value))[0])
    value = json.loads(terminal(success=False))
    value['error_message'] = '\ud800'
    self.assertEqual('malformed', parse_payload(json.dumps(value))[0])
    value['error_message'] = 'safe'
    value['warnings'] = ['\ud800']
    self.assertEqual('malformed', parse_payload(json.dumps(value))[0])

  def test_parse_rejects_impossible_and_unknown_terminal_codes(self):
    for error_code, error_name in ((109, 'RELAUNCHED'), (120, 'UNKNOWN_ERROR'),
                                   (9999, 'UNKNOWN_ERROR')):
      value = json.loads(terminal(success=False))
      value['exit_code'] = error_code
      value['error_code'] = error_code
      value['error_name'] = error_name
      self.assertEqual('malformed', parse_payload(json.dumps(value))[0], value)

    value = json.loads(terminal(success=False))
    value['exit_code'] = 199
    value['error_code'] = 199
    value['error_name'] = 'UNKNOWN_ERROR'
    self.assertEqual('valid', parse_payload(json.dumps(value))[0])

  def test_terminal_first_duplicate_and_missing_handoff(self):
    receiver = Correlator()
    self.assertEqual('buffered', receiver.accept_payload(terminal()))
    self.assertNotIn(ID_A, receiver.results)
    self.assertEqual('duplicate', receiver.accept_payload(terminal()))
    self.assertEqual('correlated', receiver.accept_payload(handoff()))
    self.assertIn(ID_A, receiver.results)
    self.assertEqual('duplicate', receiver.accept_payload(terminal()))
    self.assertEqual('duplicate', receiver.accept_payload(handoff()))

    self.assertEqual('buffered', receiver.accept_payload(terminal(ID_B)))
    self.assertNotIn(ID_B, receiver.results)

  def test_concurrent_operation_ids_do_not_cross_correlate(self):
    receiver = Correlator()
    self.assertEqual('handoff', receiver.accept_payload(handoff(ID_A, 1)))
    self.assertEqual('handoff', receiver.accept_payload(handoff(ID_B, 2)))
    self.assertEqual('correlated', receiver.accept_payload(terminal(ID_B)))
    self.assertNotIn(ID_A, receiver.results)
    self.assertEqual('correlated', receiver.accept_payload(terminal(ID_A)))
    self.assertEqual(ID_A, receiver.results[ID_A]['operation_id'])
    self.assertEqual(ID_B, receiver.results[ID_B]['operation_id'])

  def test_correlation_is_bounded_expiring_and_consumable(self):
    now = [100.0]
    receiver = Correlator(capacity=2,
                          expiration_seconds=10,
                          clock=lambda: now[0])
    self.assertEqual('buffered', receiver.accept_payload(terminal(ID_A)))
    now[0] += 1
    self.assertEqual('handoff', receiver.accept_payload(handoff(ID_B, 2)))
    now[0] += 1
    self.assertEqual('buffered', receiver.accept_payload(terminal(ID_C)))
    self.assertEqual(2, receiver.tracked_operation_count())

    # Prefer evicting the oldest orphan terminal over an active handoff.
    self.assertEqual('handoff', receiver.accept_payload(handoff(ID_A, 1)))
    self.assertNotIn(ID_A, receiver.results)
    self.assertNotIn(ID_C, receiver.pending)
    self.assertIn(ID_B, receiver.handoffs)
    self.assertEqual('ignored', receiver.accept_payload(terminal(ID_C)))
    self.assertEqual(2, receiver.tracked_operation_count())

    self.assertEqual('correlated', receiver.accept_payload(terminal(ID_A)))
    self.assertEqual(ID_A, receiver.consume_result(ID_A)['operation_id'])
    self.assertNotIn(ID_A, receiver.handoffs)
    self.assertNotIn(ID_A, receiver.results)
    self.assertTrue(receiver.remove(ID_B))
    self.assertFalse(receiver.remove(ID_B))

    self.assertEqual('buffered', receiver.accept_payload(terminal(ID_C)))
    self.assertTrue(receiver.remove(ID_C))
    self.assertEqual(0, receiver.tracked_operation_count())
    self.assertEqual('buffered', receiver.accept_payload(terminal(ID_C)))
    now[0] += 10
    self.assertEqual(1, receiver.purge_expired())
    self.assertEqual(0, receiver.tracked_operation_count())
    self.assertEqual('handoff', receiver.accept_payload(handoff(ID_C, 3)))


if __name__ == '__main__':
  unittest.main()
