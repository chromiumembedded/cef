// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/dom_storage_context.h"

#include <algorithm>

#include "libcef/cef_thread.h"
#include "libcef/dom_storage_area.h"
#include "libcef/dom_storage_namespace.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/glue/webkit_glue.h"

const FilePath::CharType DOMStorageContext::kLocalStorageDirectory[] =
    FILE_PATH_LITERAL("Local Storage");

const FilePath::CharType DOMStorageContext::kLocalStorageExtension[] =
    FILE_PATH_LITERAL(".localstorage");

// Use WebStorageNamespace quota sizes as the default.
unsigned int DOMStorageContext::local_storage_quota_ =
    WebKit::WebStorageNamespace::m_localStorageQuota;
unsigned int DOMStorageContext::session_storage_quota_ =
    WebKit::WebStorageNamespace::m_sessionStorageQuota;

DOMStorageContext::DOMStorageContext(const FilePath& local_storage_path)
    : local_storage_path_(local_storage_path),
      last_storage_area_id_(0),
      last_session_storage_namespace_id_on_ui_thread_(kLocalStorageNamespaceId),
      last_session_storage_namespace_id_on_io_thread_(
          kLocalStorageNamespaceId) {
}

DOMStorageContext::~DOMStorageContext() {
  for (StorageNamespaceMap::iterator iter(storage_namespace_map_.begin());
       iter != storage_namespace_map_.end(); ++iter) {
    delete iter->second;
  }
}

int64 DOMStorageContext::AllocateStorageAreaId() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  return ++last_storage_area_id_;
}

int64 DOMStorageContext::AllocateSessionStorageNamespaceId() {
  if (CefThread::CurrentlyOn(CefThread::UI))
    return ++last_session_storage_namespace_id_on_ui_thread_;
  return --last_session_storage_namespace_id_on_io_thread_;
}

int64 DOMStorageContext::CloneSessionStorage(int64 original_id) {
  DCHECK(!CefThread::CurrentlyOn(CefThread::UI));
  int64 clone_id = AllocateSessionStorageNamespaceId();
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      base::Bind(&DOMStorageContext::CompleteCloningSessionStorage, this,
                 original_id, clone_id));
  return clone_id;
}

void DOMStorageContext::RegisterStorageArea(DOMStorageArea* storage_area) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  int64 id = storage_area->id();
  DCHECK(!GetStorageArea(id));
  storage_area_map_[id] = storage_area;
}

void DOMStorageContext::UnregisterStorageArea(DOMStorageArea* storage_area) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  int64 id = storage_area->id();
  DCHECK(GetStorageArea(id));
  storage_area_map_.erase(id);
}

DOMStorageArea* DOMStorageContext::GetStorageArea(int64 id) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  StorageAreaMap::iterator iter = storage_area_map_.find(id);
  if (iter == storage_area_map_.end())
    return NULL;
  return iter->second;
}

void DOMStorageContext::DeleteSessionStorageNamespace(int64 namespace_id) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  StorageNamespaceMap::iterator iter =
      storage_namespace_map_.find(namespace_id);
  if (iter == storage_namespace_map_.end())
    return;
  DCHECK(iter->second->dom_storage_type() == DOM_STORAGE_SESSION);
  delete iter->second;
  storage_namespace_map_.erase(iter);
}

void DOMStorageContext::DeleteLocalStorageNamespace() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  StorageNamespaceMap::iterator iter =
      storage_namespace_map_.find(kLocalStorageNamespaceId);
  if (iter == storage_namespace_map_.end())
    return;
  DCHECK(iter->second->dom_storage_type() == DOM_STORAGE_LOCAL);
  delete iter->second;
  storage_namespace_map_.erase(iter);
}

DOMStorageNamespace* DOMStorageContext::GetStorageNamespace(
    int64 id, bool allocation_allowed) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  StorageNamespaceMap::iterator iter = storage_namespace_map_.find(id);
  if (iter != storage_namespace_map_.end())
    return iter->second;
  if (!allocation_allowed)
    return NULL;
  if (id == kLocalStorageNamespaceId)
    return CreateLocalStorage();
  return CreateSessionStorage(id);
}

DOMStorageArea* DOMStorageContext::GetStorageArea(int64 namespace_id,
    const string16& origin, bool allocation_allowed) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  DOMStorageNamespace* ns =
      GetStorageNamespace(namespace_id, allocation_allowed);
  if (ns)
    return ns->GetStorageArea(origin, allocation_allowed);
  return NULL;
}

void DOMStorageContext::PurgeMemory(int64 namespace_id) {
  DOMStorageNamespace* ns = GetStorageNamespace(namespace_id, false);
  if (ns)
    ns->PurgeMemory();
}

void DOMStorageContext::DeleteDataModifiedSince(
    const base::Time& cutoff,
    const char* url_scheme_to_be_skipped,
    const std::vector<string16>& protected_origins) {
  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  PurgeMemory(kLocalStorageNamespaceId);

  if (local_storage_path_.empty())
    return;

  file_util::FileEnumerator file_enumerator(
      local_storage_path_.Append(kLocalStorageDirectory), false,
      file_util::FileEnumerator::FILES);
  for (FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    WebKit::WebSecurityOrigin web_security_origin =
        WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
            webkit_glue::FilePathToWebString(path.BaseName()));
    if (EqualsASCII(web_security_origin.protocol(), url_scheme_to_be_skipped))
      continue;

    std::vector<string16>::const_iterator find_iter =
        std::find(protected_origins.begin(), protected_origins.end(),
            web_security_origin.databaseIdentifier());
    if (find_iter != protected_origins.end())
      continue;

    file_util::FileEnumerator::FindInfo find_info;
    file_enumerator.GetFindInfo(&find_info);
    if (file_util::HasFileBeenModifiedSince(find_info, cutoff))
      file_util::Delete(path, false);
  }
}

void DOMStorageContext::DeleteLocalStorageForOrigin(const string16& origin) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  DOMStorageArea* area =
      GetStorageArea(kLocalStorageNamespaceId, origin, false);
  if (!area)
    return;

  // Calling Clear() is necessary to remove the data from the namespace.
  area->Clear();
  area->PurgeMemory();

  if (local_storage_path_.empty())
    return;

  FilePath file_path = GetLocalStorageFilePath(origin);
  if (!file_path.empty())
    file_util::Delete(file_path, false);
}

void DOMStorageContext::DeleteAllLocalStorageFiles() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  PurgeMemory(kLocalStorageNamespaceId);

  if (local_storage_path_.empty())
    return;

  file_util::FileEnumerator file_enumerator(
      local_storage_path_.Append(kLocalStorageDirectory), false,
      file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLocalStorageExtension)
      file_util::Delete(file_path, false);
  }
}

void DOMStorageContext::SetLocalStoragePath(
    const FilePath& local_storage_path) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  if ((local_storage_path.empty() && local_storage_path_.empty()) ||
      local_storage_path == local_storage_path_)
    return;

  // Make sure that we don't swap out a database that's currently being accessed
  // by unloading all of the databases temporarily.
  PurgeMemory(kLocalStorageNamespaceId);

  // Delete the current namespace, if any. It will be recreated using the new
  // path when needed.
  DeleteLocalStorageNamespace();

  local_storage_path_ = local_storage_path;
}

DOMStorageNamespace* DOMStorageContext::CreateLocalStorage() {
  FilePath dir_path;
  if (!local_storage_path_.empty())
    dir_path = local_storage_path_.Append(kLocalStorageDirectory);
  DOMStorageNamespace* new_namespace =
      DOMStorageNamespace::CreateLocalStorageNamespace(this, dir_path);
  RegisterStorageNamespace(new_namespace);
  return new_namespace;
}

DOMStorageNamespace* DOMStorageContext::CreateSessionStorage(
    int64 namespace_id) {
  DOMStorageNamespace* new_namespace =
      DOMStorageNamespace::CreateSessionStorageNamespace(this, namespace_id);
  RegisterStorageNamespace(new_namespace);
  return new_namespace;
}

void DOMStorageContext::RegisterStorageNamespace(
    DOMStorageNamespace* storage_namespace) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  int64 id = storage_namespace->id();
  DCHECK(!GetStorageNamespace(id, false));
  storage_namespace_map_[id] = storage_namespace;
}

/* static */
void DOMStorageContext::CompleteCloningSessionStorage(
    DOMStorageContext* context, int64 existing_id, int64 clone_id) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  DOMStorageNamespace* existing_namespace =
      context->GetStorageNamespace(existing_id, false);
  // If nothing exists, then there's nothing to clone.
  if (existing_namespace)
    context->RegisterStorageNamespace(existing_namespace->Copy(clone_id));
}

// static
void DOMStorageContext::ClearLocalState(const FilePath& profile_path,
                                        const char* url_scheme_to_be_skipped) {
  file_util::FileEnumerator file_enumerator(profile_path.Append(
      kLocalStorageDirectory), false, file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLocalStorageExtension) {
      WebKit::WebSecurityOrigin web_security_origin =
          WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
              webkit_glue::FilePathToWebString(file_path.BaseName()));
      if (!EqualsASCII(web_security_origin.protocol(),
                       url_scheme_to_be_skipped))
        file_util::Delete(file_path, false);
    }
  }
}

FilePath DOMStorageContext::GetLocalStorageFilePath(
    const string16& origin) const {
  DCHECK(!local_storage_path_.empty());

  string16 origin_id =
      webkit_database::DatabaseUtil::GetOriginIdentifier(GURL(origin));

  FilePath storageDir = local_storage_path_.Append(
      DOMStorageContext::kLocalStorageDirectory);
  FilePath::StringType id =
      webkit_glue::WebStringToFilePathString(origin_id);
  return storageDir.Append(id.append(kLocalStorageExtension));
}
