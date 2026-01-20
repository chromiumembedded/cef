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
  if (m.contains(1)) {}
  if (s.contains(1)) {}
  if (um.contains(1)) {}
  if (us.contains(1)) {}

  // find() == end() (negated)
  if (!m.contains(1)) {}
  if (!s.contains(2)) {}

  // count() patterns
  if (m.contains(1)) {}
  if (m.contains(1)) {}
  if (!m.contains(1)) {}
  if (m.contains(1)) {}

  // Reversed operand order
  if (m.contains(1)) {}
  if (m.contains(1)) {}

  // Pointer access
  std::map<int, int>* ptr = &m;
  if (ptr->contains(1)) {}
  if (ptr->contains(1)) {}
}
