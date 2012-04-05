// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_storage.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"
#include "libcef/dom_storage_common.h"
#include "libcef/dom_storage_namespace.h"
#include "libcef/dom_storage_area.h"

namespace {

void UIT_VisitStorage(int64 namespace_id, const CefString& origin,
                      const CefString& key,
                      CefRefPtr<CefStorageVisitor> visitor) {
  REQUIRE_UIT();

  DOMStorageContext* context = _Context->storage_context();

  // Allow storage to be allocated for localStorage so that on-disk data, if
  // any, will be available.
  bool allocation_allowed = (namespace_id == kLocalStorageNamespaceId);

  DOMStorageNamespace* ns =
      context->GetStorageNamespace(namespace_id, allocation_allowed);
  if (!ns)
    return;

  typedef std::vector<DOMStorageArea*> AreaList;
  AreaList areas;

  if (!origin.empty()) {
    // Visit only the area with the specified origin.
    DOMStorageArea* area = ns->GetStorageArea(origin, allocation_allowed);
    if (area)
      areas.push_back(area);
  } else {
    // Visit all areas.
    ns->GetStorageAreas(areas, true);
  }

  if (areas.empty())
    return;

  // Count the total number of matching keys.
  unsigned int total = 0;
  {
    NullableString16 value;
    AreaList::iterator it = areas.begin();
    for (; it != areas.end(); ) {
      DOMStorageArea* area = (*it);
      if (!key.empty()) {
        value = area->GetItem(key);
        if (value.is_null()) {
          it = areas.erase(it);
          // Don't increment the iterator.
          continue;
        } else {
          total++;
        }
      } else {
        total += area->Length();
      }
      ++it;
    }
  }

  if (total == 0)
    return;

  DOMStorageArea* area;
  bool stop = false, deleteData;
  unsigned int count = 0, i, len;
  NullableString16 keyVal, valueVal;
  string16 keyStr, valueStr;
  typedef std::vector<string16> String16List;
  String16List delete_keys;

  // Visit all matching pairs.
  AreaList::iterator it = areas.begin();
  for (; it != areas.end() && !stop; ++it) {
    // Each area.
    area = *(it);
    if (!key.empty()) {
      // Visit only the matching key.
      valueVal = area->GetItem(key);
      if (valueVal.is_null())
        valueStr.clear();
      else
        valueStr = valueVal.string();

      deleteData = false;
      stop = !visitor->Visit(static_cast<CefStorageType>(namespace_id),
          area->origin(), key, valueStr, count, total, deleteData);
      if (deleteData)
        area->RemoveItem(key);
      count++;
    } else {
      // Visit all keys.
      len = area->Length();
      for (i = 0; i < len && !stop; ++i) {
        keyVal = area->Key(i);
        if (keyVal.is_null()) {
          keyStr.clear();
          valueStr.clear();
        } else {
          keyStr = keyVal.string();
          valueVal = area->GetItem(keyStr);
          if (valueVal.is_null())
            valueStr.clear();
          else
            valueStr = valueVal.string();
        }

        deleteData = false;
        stop = !visitor->Visit(static_cast<CefStorageType>(namespace_id),
            area->origin(), keyStr, valueStr, count, total, deleteData);
        if (deleteData)
          delete_keys.push_back(keyStr);
        count++;
      }

      // Delete the requested keys.
      if (!delete_keys.empty()) {
        String16List::const_iterator it = delete_keys.begin();
        for (; it != delete_keys.end(); ++it)
          area->RemoveItem(*it);
        delete_keys.clear();
      }
    }
  }
}

void UIT_SetStoragePath(int64 namespace_id, const CefString& path) {
  REQUIRE_UIT();

  if (namespace_id != kLocalStorageNamespaceId)
    return;

  FilePath file_path;
  if (!path.empty())
    file_path = FilePath(path);

  DOMStorageContext* context = _Context->storage_context();
  DCHECK(context);
  if (context)
    context->SetLocalStoragePath(file_path);
}

}  // namespace

bool CefVisitStorage(CefStorageType type, const CefString& origin,
                     const CefString& key,
                     CefRefPtr<CefStorageVisitor> visitor) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else if (type == ST_SESSIONSTORAGE) {
    namespace_id = kLocalStorageNamespaceId + 1;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    UIT_VisitStorage(namespace_id, origin, key, visitor);
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        base::Bind(&UIT_VisitStorage, namespace_id, origin, key, visitor));
  }

  return true;
}

bool CefSetStorage(CefStorageType type, const CefString& origin,
                   const CefString& key, const CefString& value) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the UI thread.
  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else if (type == ST_SESSIONSTORAGE) {
    namespace_id = kLocalStorageNamespaceId + 1;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  if (origin.empty()) {
    NOTREACHED() << "invalid origin";
    return false;
  }

  DOMStorageArea* area =
      _Context->storage_context()->GetStorageArea(namespace_id, origin, true);
  if (!area)
    return false;

  WebKit::WebStorageArea::Result result;
  area->SetItem(key, value, &result);
  return (result ==  WebKit::WebStorageArea::ResultOK);
}

bool CefDeleteStorage(CefStorageType type, const CefString& origin,
                      const CefString& key) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the UI thread.
  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else if (type == ST_SESSIONSTORAGE) {
    namespace_id = kLocalStorageNamespaceId + 1;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  DOMStorageContext* context = _Context->storage_context();

  // Allow storage to be allocated for localStorage so that on-disk data, if
  // any, will be available.
  bool allocation_allowed = (namespace_id == kLocalStorageNamespaceId);

  if (origin.empty()) {
    // Delete all storage for the namespace.
    if (namespace_id == kLocalStorageNamespaceId)
      context->DeleteAllLocalStorageFiles();
    else
      context->PurgeMemory(namespace_id);
  } else if (key.empty()) {
    // Clear the storage area for the specified origin.
    if (namespace_id == kLocalStorageNamespaceId) {
      context->DeleteLocalStorageForOrigin(origin);
    } else {
      DOMStorageArea* area =
          context->GetStorageArea(namespace_id, origin, allocation_allowed);
      if (area) {
        // Calling Clear() is necessary to remove the data from the namespace.
        area->Clear();
        area->PurgeMemory();
      }
    }
  } else {
    // Delete the specified key.
    DOMStorageArea* area =
        context->GetStorageArea(namespace_id, origin, allocation_allowed);
    if (area)
      area->RemoveItem(key);
  }

  return true;
}

bool CefSetStoragePath(CefStorageType type, const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    UIT_SetStoragePath(namespace_id, path);
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        base::Bind(&UIT_SetStoragePath, namespace_id, path));
  }

  return true;
}
