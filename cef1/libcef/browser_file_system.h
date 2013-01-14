// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FILE_SYSTEM_H_
#define CEF_LIBCEF_BROWSER_FILE_SYSTEM_H_
#pragma once

#include <vector>

#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_types.h"

namespace WebKit {
class WebFileSystemCallbacks;
class WebFrame;
class WebURL;
}

namespace fileapi {
class FileSystemContext;
class FileSystemURL;
}

namespace webkit_blob {
class BlobStorageController;
}

class BrowserFileSystem
    : public WebKit::WebFileSystem,
      public base::SupportsWeakPtr<BrowserFileSystem> {
 public:
  BrowserFileSystem();
  virtual ~BrowserFileSystem();

  void CreateContext();

  void OpenFileSystem(WebKit::WebFrame* frame,
                      WebKit::WebFileSystem::Type type,
                      long long size,  // NOLINT(runtime/int)
                      bool create,
                      WebKit::WebFileSystemCallbacks* callbacks);
  void DeleteFileSystem(WebKit::WebFrame* frame,
                        WebKit::WebFileSystem::Type type,
                        WebKit::WebFileSystemCallbacks* callbacks);

  fileapi::FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }

  // WebKit::WebFileSystem implementation.
  virtual void move(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void copy(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void remove(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void removeRecursively(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void readMetadata(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void createFile(
      const WebKit::WebURL& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void createDirectory(
      const WebKit::WebURL& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void fileExists(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void directoryExists(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void readDirectory(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual WebKit::WebFileWriter* createFileWriter(
      const WebKit::WebURL& path, WebKit::WebFileWriterClient*) OVERRIDE;
  virtual void createSnapshotFileAndReadMetadata(
      const WebKit::WebURL& blobURL,
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks* callbacks) OVERRIDE;

  static void InitializeOnIOThread(
      webkit_blob::BlobStorageController* blob_storage_controller);
  static void CleanupOnIOThread();

 private:
  enum FilePermission {
    FILE_PERMISSION_READ,
    FILE_PERMISSION_WRITE,
    FILE_PERMISSION_CREATE,
  };

  // Helpers.
  bool HasFilePermission(const fileapi::FileSystemURL& url,
                         FilePermission permission);
  fileapi::FileSystemOperation* GetNewOperation(
      const fileapi::FileSystemURL& url);

  // Callback Handlers
  fileapi::FileSystemOperation::StatusCallback FinishHandler(
      WebKit::WebFileSystemCallbacks* callbacks);
  fileapi::FileSystemOperation::GetMetadataCallback GetMetadataHandler(
      WebKit::WebFileSystemCallbacks* callbacks);
  fileapi::FileSystemOperation::ReadDirectoryCallback
      ReadDirectoryHandler(WebKit::WebFileSystemCallbacks* callbacks);
  fileapi::FileSystemContext::OpenFileSystemCallback OpenFileSystemHandler(
      WebKit::WebFileSystemCallbacks* callbacks);
  fileapi::FileSystemContext::DeleteFileSystemCallback DeleteFileSystemHandler(
      WebKit::WebFileSystemCallbacks* callbacks);
  fileapi::FileSystemOperation::SnapshotFileCallback
      SnapshotFileHandler(const GURL& blob_url,
                          WebKit::WebFileSystemCallbacks* callbacks);
  void DidFinish(WebKit::WebFileSystemCallbacks* callbacks,
                 base::PlatformFileError result);
  void DidGetMetadata(WebKit::WebFileSystemCallbacks* callbacks,
                      base::PlatformFileError result,
                      const base::PlatformFileInfo& info,
                      const FilePath& platform_path);
  void DidReadDirectory(
      WebKit::WebFileSystemCallbacks* callbacks,
      base::PlatformFileError result,
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more);
  void DidOpenFileSystem(WebKit::WebFileSystemCallbacks* callbacks,
                         base::PlatformFileError result,
                         const std::string& name, const GURL& root);
  void DidDeleteFileSystem(WebKit::WebFileSystemCallbacks* callbacks,
                           base::PlatformFileError result);
  void DidCreateSnapshotFile(
      const GURL& blob_url,
      WebKit::WebFileSystemCallbacks* callbacks,
      base::PlatformFileError result,
      const base::PlatformFileInfo& info,
      const FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFileSystem);
};

#endif  // CEF_LIBCEF_BROWSER_FILE_SYSTEM_H_
