// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

void test() {
  std::map<int, int> m;
  std::set<int> s;
  std::unordered_map<int, int> um;
  std::unordered_set<int> us;

  // Basic patterns - find() != end()
  if (m.find(1) != m.end()) {}
  if (s.find(1) != s.end()) {}
  if (um.find(1) != um.end()) {}
  if (us.find(1) != us.end()) {}

  // find() == end() (negated)
  if (m.find(1) == m.end()) {}
  if (s.find(2) == s.end()) {}

  // count() patterns
  if (m.count(1) != 0) {}
  if (m.count(1) > 0) {}
  if (m.count(1) == 0) {}
  if (m.count(1)) {}

  // Reversed operand order
  if (m.end() != m.find(1)) {}
  if (0 != m.count(1)) {}

  // Pointer access
  std::map<int, int>* ptr = &m;
  if (ptr->find(1) != ptr->end()) {}
  if (ptr->count(1) != 0) {}
}
