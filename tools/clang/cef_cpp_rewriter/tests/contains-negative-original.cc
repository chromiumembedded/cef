// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>
#include <string>

void test() {
  std::map<int, int> m1, m2;
  std::string str;

  // Different containers - DO NOT TRANSFORM
  if (m1.find(1) != m2.end()) {}

  // std::string::find - DO NOT TRANSFORM (different semantics)
  if (str.find("x") != std::string::npos) {}
  if (str.find('x') != std::string::npos) {}

  // Iterator is used - DO NOT TRANSFORM
  // (These won't match because find() result isn't directly compared)
  auto it = m1.find(1);
  if (it != m1.end()) { /* use it */ }
}
