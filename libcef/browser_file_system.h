// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_FILE_SYSTEM_H_
#define BROWSER_FILE_SYSTEM_H_

#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_temp_dir.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "webkit/fileapi/file_system_types.h"
#include <vector>

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

  void OpenFileSystem(WebKit::WebFrame* frame,
                      WebKit::WebFileSystem::Type type,
                      long long size,
                      bool create,
                      WebKit::WebFileSystemCallbacks* callbacks);

  fileapi::FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }

  // New WebKit::WebFileSystem overrides.
  virtual void move(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebKit::WebFileSystemCallbacks*);

  virtual void copy(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebKit::WebFileSystemCallbacks*);

  virtual void remove(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void removeRecursively(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void readMetadata(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void createFile(
      const WebKit::WebURL& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*);

  virtual void createDirectory(
      const WebKit::WebURL& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*);

  virtual void fileExists(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void directoryExists(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void readDirectory(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*);

  virtual WebKit::WebFileWriter* createFileWriter(
      const WebKit::WebURL& path, WebKit::WebFileWriterClient*);

  // Old WebKit::WebFileSystem overrides, soon to go away.
  virtual void move(const WebKit::WebString& src_path,
                    const WebKit::WebString& dest_path,
                    WebKit::WebFileSystemCallbacks* callbacks);
  virtual void copy(const WebKit::WebString& src_path,
                    const WebKit::WebString& dest_path,
                    WebKit::WebFileSystemCallbacks* callbacks);
  virtual void remove(const WebKit::WebString& path,
                      WebKit::WebFileSystemCallbacks* callbacks);
  virtual void removeRecursively(const WebKit::WebString& path,
                                 WebKit::WebFileSystemCallbacks* callbacks);
  virtual void readMetadata(const WebKit::WebString& path,
                            WebKit::WebFileSystemCallbacks* callbacks);
  virtual void createFile(const WebKit::WebString& path,
                          bool exclusive,
                          WebKit::WebFileSystemCallbacks* callbacks);
  virtual void createDirectory(const WebKit::WebString& path,
                               bool exclusive,
                               WebKit::WebFileSystemCallbacks* callbacks);
  virtual void fileExists(const WebKit::WebString& path,
                          WebKit::WebFileSystemCallbacks* callbacks);
  virtual void directoryExists(const WebKit::WebString& path,
                               WebKit::WebFileSystemCallbacks* callbacks);
  virtual void readDirectory(const WebKit::WebString& path,
                             WebKit::WebFileSystemCallbacks* callbacks);
  virtual WebKit::WebFileWriter* createFileWriter(
      const WebKit::WebString& path, WebKit::WebFileWriterClient* client);

 private:
  // Helpers.
  fileapi::FileSystemOperation* GetNewOperation(
      WebKit::WebFileSystemCallbacks* callbacks);

  // A temporary directory for FileSystem API.
  ScopedTempDir file_system_dir_;

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFileSystem);
};

#endif  // BROWSER_FILE_SYSTEM_H_
