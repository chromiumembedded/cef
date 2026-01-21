// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Test file for structured bindings transformation
// Path contains /cef/ to pass path filter

#include <map>
#include <string>
#include <unordered_map>

void use(int, int) {}
void use(const std::string&, int) {}

void test_basic_map() {
  std::map<int, int> m;

  // Basic pattern: const auto& pair with .first/.second
  for (const auto& pair : m) {
    use(pair.first, pair.second);
  }

  // Non-const reference
  for (auto& p : m) {
    use(p.first, p.second);
  }

  // Value copy
  for (auto entry : m) {
    use(entry.first, entry.second);
  }
}

void test_unordered_map() {
  std::unordered_map<std::string, int> um;

  for (const auto& item : um) {
    use(item.first, item.second);
  }
}

void test_only_first() {
  std::map<int, int> m;

  // Only uses .first
  for (const auto& pair : m) {
    use(pair.first, 0);
  }
}

void test_only_second() {
  std::map<int, int> m;

  // Only uses .second
  for (const auto& pair : m) {
    use(0, pair.second);
  }
}

void test_multiple_uses() {
  std::map<int, int> m;

  // Multiple uses of .first and .second
  for (const auto& pair : m) {
    use(pair.first, pair.second);
    use(pair.first + 1, pair.second + 1);
  }
}
