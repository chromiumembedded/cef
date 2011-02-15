// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_file_system.h"
#include "browser_file_writer.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_callback_factory.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemEntry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/glue/webkit_glue.h"

using base::WeakPtr;

using WebKit::WebFileInfo;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebFileWriter;
using WebKit::WebFileWriterClient;
using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebVector;

using fileapi::FileSystemCallbackDispatcher;
using fileapi::FileSystemContext;
using fileapi::FileSystemOperation;

namespace {

class BrowserFileSystemCallbackDispatcher
    : public FileSystemCallbackDispatcher {
 public:
  BrowserFileSystemCallbackDispatcher(
      const WeakPtr<BrowserFileSystem>& file_system,
      WebFileSystemCallbacks* callbacks)
      : file_system_(file_system),
        callbacks_(callbacks) {
  }

  ~BrowserFileSystemCallbackDispatcher() {
  }

  virtual void DidSucceed() {
    DCHECK(file_system_);
    callbacks_->didSucceed();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info) {
    DCHECK(file_system_);
    WebFileInfo web_file_info;
    web_file_info.length = info.size;
    web_file_info.modificationTime = info.last_modified.ToDoubleT();
    web_file_info.type = info.is_directory ?
        WebFileInfo::TypeDirectory : WebFileInfo::TypeFile;
    callbacks_->didReadMetadata(web_file_info);
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) {
    DCHECK(file_system_);
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
  }

  virtual void DidOpenFileSystem(
      const std::string& name, const FilePath& path) {
    DCHECK(file_system_);
    if (path.empty())
      callbacks_->didFail(WebKit::WebFileErrorSecurity);
    else
      callbacks_->didOpenFileSystem(
          UTF8ToUTF16(name), webkit_glue::FilePathToWebString(path));
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    DCHECK(file_system_);
    callbacks_->didFail(
        webkit_glue::PlatformFileErrorToWebFileError(error_code));
  }

  virtual void DidWrite(int64, bool) {
    NOTREACHED();
  }

 private:
  WeakPtr<BrowserFileSystem> file_system_;
  WebFileSystemCallbacks* callbacks_;
};

}  // namespace

BrowserFileSystem::BrowserFileSystem() {
  if (file_system_dir_.CreateUniqueTempDir()) {
    file_system_context_ = new FileSystemContext(
        base::MessageLoopProxy::CreateForCurrentThread(),
        base::MessageLoopProxy::CreateForCurrentThread(),
        file_system_dir_.path(),
        false /* incognito */,
        true /* allow_file_access */,
        false /* unlimited_quota */);
  } else {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
  }
}

BrowserFileSystem::~BrowserFileSystem() {
}

void BrowserFileSystem::OpenFileSystem(
    WebFrame* frame, WebFileSystem::Type web_filesystem_type,
    long long, bool create,
    WebFileSystemCallbacks* callbacks) {
  if (!frame || !file_system_context_.get()) {
    // The FileSystem temp directory was not initialized successfully.
    callbacks->didFail(WebKit::WebFileErrorSecurity);
    return;
  }

  fileapi::FileSystemType type;
  if (web_filesystem_type == WebFileSystem::TypeTemporary)
    type = fileapi::kFileSystemTypeTemporary;
  else if (web_filesystem_type == WebFileSystem::TypePersistent)
    type = fileapi::kFileSystemTypePersistent;
  else {
    // Unknown type filesystem is requested.
    callbacks->didFail(WebKit::WebFileErrorSecurity);
    return;
  }

  GURL origin_url(frame->securityOrigin().toString());
  GetNewOperation(callbacks)->OpenFileSystem(origin_url, type, create);
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

FileSystemOperation* BrowserFileSystem::GetNewOperation(
    WebFileSystemCallbacks* callbacks) {
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(AsWeakPtr(), callbacks);
  FileSystemOperation* operation = new FileSystemOperation(
      dispatcher, base::MessageLoopProxy::CreateForCurrentThread(),
      file_system_context_.get());
  return operation;
}
