// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>
#include <vector>

#include "include/internal/cef_string_map.h"

#include "base/check_op.h"

namespace {

class StringMap {
  using Map = std::map<CefString, CefString>;
  using value_type = Map::value_type;

 public:
  using const_iterator = Map::const_iterator;

  size_t size() const { return map_ref_.size(); }
  size_t count(const CefString& key) const { return map_.count(key); }
  const_iterator find(const CefString& value) const { return map_.find(value); }
  const_iterator cend() const { return map_.cend(); }
  const value_type& operator[](size_t pos) const { return *map_ref_[pos]; }

  void insert(value_type&& value) {
    // does not invalidate iterators
    const auto [it, inserted] = map_.insert(std::move(value));
    if (inserted) {
      map_ref_.push_back(std::move(it));
    }
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

CEF_EXPORT cef_string_map_t cef_string_map_alloc() {
  return reinterpret_cast<cef_string_map_t>(new StringMap);
}

CEF_EXPORT size_t cef_string_map_size(cef_string_map_t map) {
  DCHECK(map);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  return impl->size();
}

CEF_EXPORT int cef_string_map_find(cef_string_map_t map,
                                   const cef_string_t* key,
                                   cef_string_t* value) {
  DCHECK(map);
  DCHECK(value);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  StringMap::const_iterator it = impl->find(CefString(key));
  if (it == impl->cend()) {
    return 0;
  }

  const CefString& val = it->second;
  return cef_string_set(val.c_str(), val.length(), value, true);
}

CEF_EXPORT int cef_string_map_key(cef_string_map_t map,
                                  size_t index,
                                  cef_string_t* key) {
  DCHECK(map);
  DCHECK(key);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  DCHECK_LT(index, impl->size());
  if (index >= impl->size()) {
    return 0;
  }

  const auto& [k, _] = (*impl)[index];
  return cef_string_set(k.c_str(), k.length(), key, true);
}

CEF_EXPORT int cef_string_map_value(cef_string_map_t map,
                                    size_t index,
                                    cef_string_t* value) {
  DCHECK(map);
  DCHECK(value);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  DCHECK_LT(index, impl->size());
  if (index >= impl->size()) {
    return 0;
  }

  const auto& [_, v] = (*impl)[index];
  return cef_string_set(v.c_str(), v.length(), value, true);
}

CEF_EXPORT int cef_string_map_append(cef_string_map_t map,
                                     const cef_string_t* key,
                                     const cef_string_t* value) {
  DCHECK(map);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  impl->insert(std::make_pair(CefString(key), CefString(value)));
  return 1;
}

CEF_EXPORT void cef_string_map_clear(cef_string_map_t map) {
  DCHECK(map);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  impl->clear();
}

CEF_EXPORT void cef_string_map_free(cef_string_map_t map) {
  DCHECK(map);
  StringMap* impl = reinterpret_cast<StringMap*>(map);
  delete impl;
}
