// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_file_system.h"
#include "browser_file_writer.h"

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemEntry.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebFileWriter;
using WebKit::WebFileWriterClient;
using WebKit::WebString;
using WebKit::WebVector;

namespace {

class BrowserFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  BrowserFileSystemCallbackDispatcher(
      BrowserFileSystem* file_system,
      WebFileSystemCallbacks* callbacks)
      : file_system_(file_system),
        callbacks_(callbacks),
        request_id_(-1) {
  }

  void set_request_id(int request_id) { request_id_ = request_id; }

  virtual void DidSucceed() {
    callbacks_->didSucceed();
    file_system_->RemoveCompletedOperation(request_id_);
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info) {
    WebFileInfo web_file_info;
    web_file_info.length = info.size;
    web_file_info.modificationTime = info.last_modified.ToDoubleT();
    web_file_info.type = info.is_directory ?
        WebFileInfo::TypeDirectory : WebFileInfo::TypeFile;
    callbacks_->didReadMetadata(web_file_info);
    file_system_->RemoveCompletedOperation(request_id_);
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) {
    std::vector<WebFileSystemEntry> web_entries_vector;
    for (std::vector<base::FileUtilProxy::Entry>::const_iterator it =
             entries.begin(); it != entries.end(); ++it) {
      WebFileSystemEntry entry;
      entry.name = webkit_glue::FilePathStringToWebString(it->name);
      entry.isDirectory = it->is_directory;
      web_entries_vector.push_back(entry);
    }
    WebVector<WebKit::WebFileSystemEntry> web_entries =
        web_entries_vector;
    callbacks_->didReadDirectory(web_entries, has_more);
    file_system_->RemoveCompletedOperation(request_id_);
  }

  virtual void DidOpenFileSystem(const std::string&, const FilePath&) {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    callbacks_->didFail(
        webkit_glue::PlatformFileErrorToWebFileError(error_code));
    file_system_->RemoveCompletedOperation(request_id_);
  }

  virtual void DidWrite(int64, bool) {
    NOTREACHED();
  }

 private:
  BrowserFileSystem* file_system_;
  WebFileSystemCallbacks* callbacks_;
  int request_id_;
};

} // namespace

BrowserFileSystem::~BrowserFileSystem() {
  // Drop all the operations.
  for (OperationsMap::const_iterator iter(&operations_);
       !iter.IsAtEnd(); iter.Advance())
    operations_.Remove(iter.GetCurrentKey());
}

void BrowserFileSystem::move(
    const WebString& src_path,
    const WebString& dest_path, WebFileSystemCallbacks* callbacks) {
  FilePath dest_filepath(webkit_glue::WebStringToFilePath(dest_path));
  FilePath src_filepath(webkit_glue::WebStringToFilePath(src_path));

  GetNewOperation(callbacks)->Move(src_filepath, dest_filepath);
}

void BrowserFileSystem::copy(
    const WebString& src_path, const WebString& dest_path,
    WebFileSystemCallbacks* callbacks) {
  FilePath dest_filepath(webkit_glue::WebStringToFilePath(dest_path));
  FilePath src_filepath(webkit_glue::WebStringToFilePath(src_path));

  GetNewOperation(callbacks)->Copy(src_filepath, dest_filepath);
}

void BrowserFileSystem::remove(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->Remove(filepath, false /* recursive */);
}

void BrowserFileSystem::removeRecursively(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->Remove(filepath, true /* recursive */);
}

void BrowserFileSystem::readMetadata(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->GetMetadata(filepath);
}

void BrowserFileSystem::createFile(
    const WebString& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->CreateFile(filepath, exclusive);
}

void BrowserFileSystem::createDirectory(
    const WebString& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->CreateDirectory(filepath, exclusive, false);
}

void BrowserFileSystem::fileExists(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->FileExists(filepath);
}

void BrowserFileSystem::directoryExists(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->DirectoryExists(filepath);
}

void BrowserFileSystem::readDirectory(
    const WebString& path, WebFileSystemCallbacks* callbacks) {
  FilePath filepath(webkit_glue::WebStringToFilePath(path));

  GetNewOperation(callbacks)->ReadDirectory(filepath);
}

WebFileWriter* BrowserFileSystem::createFileWriter(
    const WebString& path, WebFileWriterClient* client) {
  return new BrowserFileWriter(path, client);
}

fileapi::FileSystemOperation* BrowserFileSystem::GetNewOperation(
    WebFileSystemCallbacks* callbacks) {
  // This pointer will be owned by |operation|.
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(this, callbacks);
  fileapi::FileSystemOperation* operation = new fileapi::FileSystemOperation(
      dispatcher, base::MessageLoopProxy::CreateForCurrentThread());
  int32 request_id = operations_.Add(operation);
  dispatcher->set_request_id(request_id);
  return operation;
}

void BrowserFileSystem::RemoveCompletedOperation(int request_id) {
  operations_.Remove(request_id);
}
