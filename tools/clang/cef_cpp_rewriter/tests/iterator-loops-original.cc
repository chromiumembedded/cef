// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Test file for iterator loop to range-for transformation
// Path contains /cef/ to pass path filter

#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

void use(int, int) {}
void use(const std::string&, int) {}

void test_basic_iterator_loop() {
  std::map<int, int> m;

  // Basic iterator loop with ++it
  for (auto it = m.begin(); it != m.end(); ++it) {
    use(it->first, it->second);
  }
}

void test_postfix_increment() {
  std::map<int, int> m;

  // Postfix increment it++
  for (auto it = m.begin(); it != m.end(); it++) {
    use(it->first, it->second);
  }
}

void test_unordered_map() {
  std::unordered_map<std::string, int> um;

  for (auto it = um.begin(); it != um.end(); ++it) {
    use(it->first, it->second);
  }
}

void test_only_first() {
  std::map<int, int> m;

  // Only uses ->first
  for (auto it = m.begin(); it != m.end(); ++it) {
    use(it->first, 0);
  }
}

void test_only_second() {
  std::map<int, int> m;

  // Only uses ->second
  for (auto it = m.begin(); it != m.end(); ++it) {
    use(0, it->second);
  }
}

void test_multiple_uses() {
  std::map<int, int> m;

  // Multiple uses of ->first and ->second
  for (auto it = m.begin(); it != m.end(); ++it) {
    use(it->first, it->second);
    use(it->first + 1, it->second + 1);
  }
}

void test_const_iterator() {
  std::map<int, int> m;

  // Const iterator pattern
  for (std::map<int, int>::const_iterator it = m.begin(); it != m.end(); ++it) {
    use(it->first, it->second);
  }
}

void test_vector_of_pairs() {
  std::vector<std::pair<int, int>> v;

  // Vector of pairs should also be transformed
  for (auto it = v.begin(); it != v.end(); ++it) {
    use(it->first, it->second);
  }
}
