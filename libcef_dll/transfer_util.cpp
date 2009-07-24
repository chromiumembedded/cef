// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "transfer_util.h"


void transfer_string_map_contents(cef_string_map_t fromMap,
                                  std::map<std::wstring, std::wstring>& toMap)
{
  int size = cef_string_map_size(fromMap);
  cef_string_t key, value;
  std::wstring keystr, valuestr;
  
  for(int i = 0; i < size; ++i) {
    key = cef_string_map_key(fromMap, i);
    value = cef_string_map_value(fromMap, i);
    
    if(key) {
      keystr = key;
      cef_string_free(key);
    } else if(!keystr.empty())
      keystr.clear();

    if(value) {
      valuestr = value;
      cef_string_free(value);
    } else if(!valuestr.empty())
      valuestr.clear();

    toMap.insert(std::pair<std::wstring, std::wstring>(keystr, valuestr));
  }
}

void transfer_string_map_contents(const std::map<std::wstring, std::wstring>& fromMap,
                                  cef_string_map_t toMap)
{
  std::map<std::wstring, std::wstring>::const_iterator it = fromMap.begin();
  for(; it != fromMap.end(); ++it) {
    cef_string_map_append(toMap, it->first.c_str(), it->second.c_str());
  }
}

void transfer_string_contents(const std::wstring& fromString,
                              cef_string_t* toString)
{
  if(*toString == NULL || *toString != fromString) {
    if(*toString) {
      cef_string_free(*toString);
      *toString = NULL;
    }
    if(!fromString.empty())
      *toString = cef_string_alloc(fromString.c_str());
  }
}

void transfer_string_contents(cef_string_t fromString, std::wstring& toString,
                              bool freeFromString)
{
  if(fromString == NULL || fromString != toString) {
    if(fromString) {
      toString = fromString;
      if(freeFromString)
        cef_string_free(fromString);
    } else if(!toString.empty())
      toString.clear();
  }
}
