// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FILE_SYSTEM_H_
#define CEF_LIBCEF_BROWSER_FILE_SYSTEM_H_
#pragma once

#include <vector>

#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_temp_dir.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "webkit/fileapi/file_system_types.h"

namespace WebKit {
class WebFileSystemCallbacks;
class WebFrame;
class WebURL;
}

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;
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

 private:
  // Helpers.
  fileapi::FileSystemOperation* GetNewOperation(
      WebKit::WebFileSystemCallbacks* callbacks);

  // A temporary directory for FileSystem API.
  ScopedTempDir file_system_dir_;

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFileSystem);
};

#endif  // CEF_LIBCEF_BROWSER_FILE_SYSTEM_H_
