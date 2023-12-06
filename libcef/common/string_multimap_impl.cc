// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>
#include <vector>

#include "include/internal/cef_string_multimap.h"

#include "base/check_op.h"

namespace {

class StringMultimap {
  using Map = std::multimap<CefString, CefString>;
  using value_type = Map::value_type;

 public:
  using const_iterator = Map::const_iterator;

  size_t size() const { return map_ref_.size(); }
  size_t count(const CefString& key) const { return map_.count(key); }
  const value_type operator[](size_t pos) const { return *map_ref_[pos]; }

  std::pair<const_iterator, const_iterator> equal_range(
      const CefString& key) const {
    return map_.equal_range(key);
  }
  void insert(value_type&& value) {
    auto it = map_.insert(std::move(value));  // does not invalidate iterators
    map_ref_.push_back(std::move(it));
  }
  void clear() {
    map_ref_.clear();
    map_.clear();
  }

 private:
  Map map_;
  std::vector<Map::const_iterator> map_ref_;
};

}  // namespace

CEF_EXPORT cef_string_multimap_t cef_string_multimap_alloc() {
  return reinterpret_cast<cef_string_multimap_t>(new StringMultimap);
}

CEF_EXPORT size_t cef_string_multimap_size(cef_string_multimap_t map) {
  DCHECK(map);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  return impl->size();
}

CEF_EXPORT size_t cef_string_multimap_find_count(cef_string_multimap_t map,
                                                 const cef_string_t* key) {
  DCHECK(map);
  DCHECK(key);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  return impl->count(CefString(key));
}

CEF_EXPORT int cef_string_multimap_enumerate(cef_string_multimap_t map,
                                             const cef_string_t* key,
                                             size_t value_index,
                                             cef_string_t* value) {
  DCHECK(map);
  DCHECK(key);
  DCHECK(value);

  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  CefString key_str(key);

  DCHECK_LT(value_index, impl->count(key_str));
  if (value_index >= impl->count(key_str)) {
    return 0;
  }

  std::pair<StringMultimap::const_iterator, StringMultimap::const_iterator>
      range_it = impl->equal_range(key_str);

  size_t count = value_index;
  while (count-- && range_it.first != range_it.second) {
    range_it.first++;
  }

  if (range_it.first == range_it.second) {
    return 0;
  }

  const CefString& val = range_it.first->second;
  return cef_string_set(val.c_str(), val.length(), value, true);
}

CEF_EXPORT int cef_string_multimap_key(cef_string_multimap_t map,
                                       size_t index,
                                       cef_string_t* key) {
  DCHECK(map);
  DCHECK(key);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  DCHECK_LT(index, impl->size());
  if (index >= impl->size()) {
    return 0;
  }

  const auto& [k, _] = (*impl)[index];
  return cef_string_set(k.c_str(), k.length(), key, true);
}

CEF_EXPORT int cef_string_multimap_value(cef_string_multimap_t map,
                                         size_t index,
                                         cef_string_t* value) {
  DCHECK(map);
  DCHECK(value);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  DCHECK_LT(index, impl->size());
  if (index >= impl->size()) {
    return 0;
  }

  const auto& [_, v] = (*impl)[index];
  return cef_string_set(v.c_str(), v.length(), value, true);
}

CEF_EXPORT int cef_string_multimap_append(cef_string_multimap_t map,
                                          const cef_string_t* key,
                                          const cef_string_t* value) {
  DCHECK(map);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  impl->insert(std::make_pair(CefString(key), CefString(value)));
  return 1;
}

CEF_EXPORT void cef_string_multimap_clear(cef_string_multimap_t map) {
  DCHECK(map);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  impl->clear();
}

CEF_EXPORT void cef_string_multimap_free(cef_string_multimap_t map) {
  DCHECK(map);
  StringMultimap* impl = reinterpret_cast<StringMultimap*>(map);
  delete impl;
}
