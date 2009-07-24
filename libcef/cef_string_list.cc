// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "include/cef_string_list.h"
#include "base/logging.h"
#include <vector>


typedef std::vector<std::wstring> StringList;

CEF_EXPORT cef_string_list_t cef_string_list_alloc()
{
  return new StringList;
}

CEF_EXPORT int cef_string_list_size(cef_string_list_t list)
{
  DCHECK(list);
  StringList* impl = (StringList*)list;
  return impl->size();
}

CEF_EXPORT cef_string_t cef_string_list_value(cef_string_list_t list, int index)
{
  DCHECK(list);
  StringList* impl = (StringList*)list;
  DCHECK(index >= 0 && index < (int)impl->size());
  if(index < 0 || index >= (int)impl->size())
    return NULL;
  return cef_string_alloc((*impl)[index].c_str());
}

CEF_EXPORT void cef_string_list_append(cef_string_list_t list, const wchar_t* value)
{
  DCHECK(list);
  StringList* impl = (StringList*)list;
  std::wstring valstr;
  if(value)
    valstr = value;
  impl->push_back(valstr);
}

CEF_EXPORT void cef_string_list_clear(cef_string_list_t list)
{
  DCHECK(list);
  StringList* impl = (StringList*)list;
  impl->clear();
}

CEF_EXPORT void cef_string_list_free(cef_string_list_t list)
{
  DCHECK(list);
  delete (StringList*)list;
}
