// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <vector>

#include "include/internal/cef_string_list.h"

#include "base/check_op.h"

namespace {
using StringList = std::vector<CefString>;
}  // namespace

CEF_EXPORT cef_string_list_t cef_string_list_alloc() {
  return reinterpret_cast<cef_string_list_t>(new StringList);
}

CEF_EXPORT size_t cef_string_list_size(cef_string_list_t list) {
  DCHECK(list);
  StringList* impl = reinterpret_cast<StringList*>(list);
  return impl->size();
}

CEF_EXPORT int cef_string_list_value(cef_string_list_t list,
                                     size_t index,
                                     cef_string_t* value) {
  DCHECK(list);
  DCHECK(value);
  StringList* impl = reinterpret_cast<StringList*>(list);
  DCHECK_LT(index, impl->size());
  if (index >= impl->size()) {
    return false;
  }
  const CefString& str = (*impl)[index];
  return cef_string_copy(str.c_str(), str.length(), value);
}

CEF_EXPORT void cef_string_list_append(cef_string_list_t list,
                                       const cef_string_t* value) {
  DCHECK(list);
  StringList* impl = reinterpret_cast<StringList*>(list);
  if (value) {
    impl->emplace_back(value->str, value->length, /*copy=*/true);
  } else {
    impl->emplace_back();
  }
}

CEF_EXPORT void cef_string_list_clear(cef_string_list_t list) {
  DCHECK(list);
  StringList* impl = reinterpret_cast<StringList*>(list);
  impl->clear();
}

CEF_EXPORT void cef_string_list_free(cef_string_list_t list) {
  DCHECK(list);
  StringList* impl = reinterpret_cast<StringList*>(list);
  delete impl;
}

CEF_EXPORT cef_string_list_t cef_string_list_copy(cef_string_list_t list) {
  DCHECK(list);
  StringList* impl = reinterpret_cast<StringList*>(list);
  return reinterpret_cast<cef_string_list_t>(new StringList(*impl));
}
