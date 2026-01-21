// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Negative test cases - these should NOT be transformed
// Path contains /cef/ to pass path filter

#include <map>
#include <set>
#include <string>
#include <vector>

void use(int) {}
void use(int, int) {}
template <typename T>
void consume(const T&) {}

void test_iterator_used_for_erase() {
  std::map<int, int> m;

  // Iterator used for erase - DO NOT TRANSFORM
  for (auto it = m.begin(); it != m.end(); ) {
    if (it->first == 0) {
      it = m.erase(it);
    } else {
      ++it;
    }
  }
}

void test_iterator_dereferenced_directly() {
  std::map<int, int> m;

  // Iterator passed to function via *it - DO NOT TRANSFORM
  for (auto it = m.begin(); it != m.end(); ++it) {
    consume(*it);
  }
}

void test_set_iteration() {
  std::set<int> s;

  // Set elements are not pairs - DO NOT TRANSFORM
  for (auto it = s.begin(); it != s.end(); ++it) {
    use(*it);
  }
}

void test_iterator_assigned_elsewhere() {
  std::map<int, int> m;

  // Iterator used outside of normal ->first/->second - DO NOT TRANSFORM
  for (auto it = m.begin(); it != m.end(); ++it) {
    auto copy = it;
    use(copy->first, copy->second);
  }
}

void test_different_containers() {
  std::map<int, int> m1, m2;

  // begin() and end() on different containers - DO NOT TRANSFORM
  for (auto it = m1.begin(); it != m2.end(); ++it) {
    use(it->first, it->second);
  }
}

void test_local_var_named_key() {
  std::map<int, int> m;

  // Local variable named "key" would conflict - DO NOT TRANSFORM
  for (auto it = m.begin(); it != m.end(); ++it) {
    int key = it->first;
    use(key);
  }
}

void test_local_var_named_value() {
  std::map<int, int> m;

  // Local variable named "value" would conflict - DO NOT TRANSFORM
  for (auto it = m.begin(); it != m.end(); ++it) {
    int value = it->second;
    use(value);
  }
}

void test_complex_increment() {
  std::map<int, int> m;

  // Complex increment (not ++it or it++) - DO NOT TRANSFORM
  for (auto it = m.begin(); it != m.end(); std::advance(it, 1)) {
    use(it->first, it->second);
  }
}

void test_reverse_iterator() {
  std::map<int, int> m;

  // Reverse iterator - DO NOT TRANSFORM (rbegin/rend)
  for (auto it = m.rbegin(); it != m.rend(); ++it) {
    use(it->first, it->second);
  }
}

void test_no_init_decl() {
  std::map<int, int> m;
  std::map<int, int>::iterator it;

  // No declaration in init - DO NOT TRANSFORM
  for (it = m.begin(); it != m.end(); ++it) {
    use(it->first, it->second);
  }
}
