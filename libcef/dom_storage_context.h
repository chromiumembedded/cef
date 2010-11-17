// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _DOM_STORAGE_CONTEXT_H
#define _DOM_STORAGE_CONTEXT_H

#include <map>
#include <set>

#include "base/file_path.h"
#include "base/string16.h"
#include "base/time.h"

class DOMStorageArea;
class DOMStorageNamespace;

// This is owned by CefContext and is all the dom storage information that's
// shared by all of the browser windows.  The specifics of responsibilities are
// fairly well documented here and in StorageNamespace and StorageArea.
// Everything is only to be accessed on the WebKit thread unless noted
// otherwise.
//
// NOTE: Virtual methods facilitate mocking functions for testing.
class DOMStorageContext {
 public:
  DOMStorageContext();
  virtual ~DOMStorageContext();

  // Allocate a new storage area id.  Only call on the WebKit thread.
  int64 AllocateStorageAreaId();

  // Allocate a new session storage id.  Only call on the UI or IO thread.
  int64 AllocateSessionStorageNamespaceId();

  // Clones a session storage namespace and returns the cloned namespaces' id.
  // Only call on the IO thread.
  int64 CloneSessionStorage(int64 original_id);

  // Various storage area methods.  The storage area is owned by one of the
  // namespaces that's owned by this class.
  void RegisterStorageArea(DOMStorageArea* storage_area);
  void UnregisterStorageArea(DOMStorageArea* storage_area);
  DOMStorageArea* GetStorageArea(int64 id);

  // Called on WebKit thread when a session storage namespace can be deleted.
  void DeleteSessionStorageNamespace(int64 namespace_id);

  // Get a namespace from an id.  What's returned is owned by this class.  If
  // allocation_allowed is true, then this function will create the storage
  // namespace if it hasn't been already.
  DOMStorageNamespace* GetStorageNamespace(int64 id, bool allocation_allowed);

  // Tells storage namespaces to purge any memory they do not need.
  virtual void PurgeMemory();

  // Delete any local storage files that have been touched since the cutoff
  // date that's supplied.
  void DeleteDataModifiedSince(const base::Time& cutoff,
                               const char* url_scheme_to_be_skipped,
                               const std::vector<string16>& protected_origins);

  // Deletes a single local storage file.
  void DeleteLocalStorageFile(const FilePath& file_path);

  // Deletes the local storage file for the given origin.
  void DeleteLocalStorageForOrigin(const string16& origin_id);

  // Deletes all local storage files.
  void DeleteAllLocalStorageFiles();

  // The local storage directory.
  static const FilePath::CharType kLocalStorageDirectory[];

  // The local storage file extension.
  static const FilePath::CharType kLocalStorageExtension[];

  // Delete all non-extension local storage files.
  static void ClearLocalState(const FilePath& profile_path,
                              const char* url_scheme_to_be_skipped);

  // Get the file name of the local storage file for the given origin.
  FilePath GetLocalStorageFilePath(const string16& origin_id) const;

 private:
  // Get the local storage instance.  The object is owned by this class.
  DOMStorageNamespace* CreateLocalStorage();

  // Get a new session storage namespace.  The object is owned by this class.
  DOMStorageNamespace* CreateSessionStorage(int64 namespace_id);

  // Used internally to register storage namespaces we create.
  void RegisterStorageNamespace(DOMStorageNamespace* storage_namespace);

  // The WebKit thread half of CloneSessionStorage above.  Static because
  // DOMStorageContext isn't ref counted thus we can't use a runnable method.
  // That said, we know this is safe because this class is destroyed on the
  // WebKit thread, so there's no way it could be destroyed before this is run.
  static void CompleteCloningSessionStorage(DOMStorageContext* context,
                                            int64 existing_id, int64 clone_id);

  // The last used storage_area_id and storage_namespace_id's.  For the storage
  // namespaces, IDs allocated on the UI thread are positive and count up while
  // IDs allocated on the IO thread are negative and count down.  This allows us
  // to allocate unique IDs on both without any locking.  All storage area ids
  // are allocated on the WebKit thread.
  int64 last_storage_area_id_;
  int64 last_session_storage_namespace_id_on_ui_thread_;
  int64 last_session_storage_namespace_id_on_io_thread_;

  // Maps ids to StorageAreas.  We do NOT own these objects.  StorageNamespace
  // (which does own them) will notify us when we should remove the entries.
  typedef std::map<int64, DOMStorageArea*> StorageAreaMap;
  StorageAreaMap storage_area_map_;

  // Maps ids to StorageNamespaces.  We own these objects.
  typedef std::map<int64, DOMStorageNamespace*> StorageNamespaceMap;
  StorageNamespaceMap storage_namespace_map_;
};

#endif  // _DOM_STORAGE_CONTEXT_H
