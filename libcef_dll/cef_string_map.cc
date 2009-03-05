// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "cef_string_map.h"
#include <map>
#include "base/logging.h"


typedef std::map<std::wstring, std::wstring> StringMap;

CEF_EXPORT cef_string_map_t cef_string_map_alloc()
{
  return new StringMap;
}

CEF_EXPORT int cef_string_map_size(cef_string_map_t map)
{
  DCHECK(map);
  StringMap* impl = (StringMap*)map;
  return impl->size();
}

CEF_EXPORT cef_string_t cef_string_map_find(cef_string_map_t map,
    const wchar_t* key)
{
  DCHECK(map);
  StringMap* impl = (StringMap*)map;
  std::wstring keystr;
  if(key)
    keystr = key;
  StringMap::const_iterator it = impl->find(keystr);
  if(it == impl->end())
    return NULL;
  return cef_string_alloc(it->second.c_str());
}

CEF_EXPORT cef_string_t cef_string_map_key(cef_string_map_t map, int index)
{
  DCHECK(map);
  StringMap* impl = (StringMap*)map;
  DCHECK(index >= 0 && index < (int)impl->size());
  if(index < 0 || index >= (int)impl->size())
    return NULL;
  StringMap::const_iterator it = impl->begin();
  for(int ct = 0; it != impl->end(); ++it, ct++) {
    if(ct == index)
      return cef_string_alloc(it->first.c_str());
  }
  return NULL;
}

CEF_EXPORT cef_string_t cef_string_map_value(cef_string_map_t map, int index)
{
  DCHECK(map);
  StringMap* impl = (StringMap*)map;
  DCHECK(index >= 0 && index < (int)impl->size());
  if(index < 0 || index >= (int)impl->size())
    return NULL;
  StringMap::const_iterator it = impl->begin();
  for(int ct = 0; it != impl->end(); ++it, ct++) {
    if(ct == index)
      return cef_string_alloc(it->second.c_str());
  }
  return NULL;
}

CEF_EXPORT void cef_string_map_append(cef_string_map_t map, const wchar_t* key,
    const wchar_t* value)
{
  DCHECK(map);
  StringMap* impl = (StringMap*)map;
  std::wstring keystr, valstr;
  if(key)
    keystr = key;
  if(value)
    valstr = value;
  impl->insert(std::pair<std::wstring, std::wstring>(keystr, valstr));
}

CEF_EXPORT void cef_string_map_clear(cef_string_map_t map)
{
  DCHECK(map);
  StringMap* impl = (StringMap*)map;
  impl->clear();
}

CEF_EXPORT void cef_string_map_free(cef_string_map_t map)
{
  DCHECK(map);
  delete (StringMap*)map;
}
