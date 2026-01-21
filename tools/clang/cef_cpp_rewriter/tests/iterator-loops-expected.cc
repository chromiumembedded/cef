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
  for (const auto& [key, value] : m) {
    use(key, value);
  }
}

void test_postfix_increment() {
  std::map<int, int> m;

  // Postfix increment it++
  for (const auto& [key, value] : m) {
    use(key, value);
  }
}

void test_unordered_map() {
  std::unordered_map<std::string, int> um;

  for (const auto& [key, value] : um) {
    use(key, value);
  }
}

void test_only_first() {
  std::map<int, int> m;

  // Only uses ->first
  for (const auto& [key, value] : m) {
    use(key, 0);
  }
}

void test_only_second() {
  std::map<int, int> m;

  // Only uses ->second
  for (const auto& [key, value] : m) {
    use(0, value);
  }
}

void test_multiple_uses() {
  std::map<int, int> m;

  // Multiple uses of ->first and ->second
  for (const auto& [key, value] : m) {
    use(key, value);
    use(key + 1, value + 1);
  }
}

void test_const_iterator() {
  std::map<int, int> m;

  // Const iterator pattern
  for (const auto& [key, value] : m) {
    use(key, value);
  }
}

void test_vector_of_pairs() {
  std::vector<std::pair<int, int>> v;

  // Vector of pairs should also be transformed
  for (const auto& [key, value] : v) {
    use(key, value);
  }
}
