// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_file_system.h"
#include "browser_file_writer.h"

#include "base/file_path.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemEntry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
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
using WebKit::WebURL;
using WebKit::WebVector;

using fileapi::FileSystemCallbackDispatcher;
using fileapi::FileSystemContext;
using fileapi::FileSystemFileUtil;
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

  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
      const FilePath& platform_path) {
    DCHECK(file_system_);
    WebFileInfo web_file_info;
    web_file_info.length = info.size;
    web_file_info.modificationTime = info.last_modified.ToDoubleT();
    web_file_info.type = info.is_directory ?
        WebFileInfo::TypeDirectory : WebFileInfo::TypeFile;
    web_file_info.platformPath =
        webkit_glue::FilePathToWebString(platform_path);
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
      const std::string& name, const GURL& root) {
    DCHECK(file_system_);
    if (!root.is_valid())
      callbacks_->didFail(WebKit::WebFileErrorSecurity);
    else
// Temporary hack to ease a 4-phase Chromium/WebKit commit.
#ifdef WEBFILESYSTEMCALLBACKS_USE_URL_NOT_STRING
      callbacks_->didOpenFileSystem(WebString::fromUTF8(name), root);
#else
      callbacks_->didOpenFileSystem(
          WebString::fromUTF8(name), WebString::fromUTF8(root.spec()));
#endif
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
        NULL /* special storage policy */,
        NULL /* quota manager */,
        file_system_dir_.path(),
        false /* incognito */,
        true /* allow_file_access */,
        true /* unlimited_quota */,
        NULL);
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
  else if (web_filesystem_type == WebFileSystem::TypeExternal)
    type = fileapi::kFileSystemTypeExternal;
  else {
    // Unknown type filesystem is requested.
    callbacks->didFail(WebKit::WebFileErrorSecurity);
    return;
  }

  GURL origin_url(frame->document().securityOrigin().toString());
  GetNewOperation(callbacks)->OpenFileSystem(origin_url, type, create);
}

void BrowserFileSystem::move(const WebString& src_path,
                            const WebString& dest_path,
                            WebFileSystemCallbacks* callbacks) {
  move(GURL(src_path), GURL(dest_path), callbacks);
}

void BrowserFileSystem::copy(const WebString& src_path,
                            const WebString& dest_path,
                            WebFileSystemCallbacks* callbacks) {
  copy(GURL(src_path), GURL(dest_path), callbacks);
}

void BrowserFileSystem::remove(const WebString& path,
                              WebFileSystemCallbacks* callbacks) {
  remove(GURL(path), callbacks);
}

void BrowserFileSystem::removeRecursively(const WebString& path,
                                         WebFileSystemCallbacks* callbacks) {
  removeRecursively(GURL(path), callbacks);
}

void BrowserFileSystem::readMetadata(const WebString& path,
                                    WebFileSystemCallbacks* callbacks) {
  readMetadata(GURL(path), callbacks);
}

void BrowserFileSystem::createFile(const WebString& path,
                                  bool exclusive,
                                  WebFileSystemCallbacks* callbacks) {
  createFile(GURL(path), exclusive, callbacks);
}

void BrowserFileSystem::createDirectory(const WebString& path,
                                       bool exclusive,
                                       WebFileSystemCallbacks* callbacks) {
  createDirectory(GURL(path), exclusive, callbacks);
}

void BrowserFileSystem::fileExists(const WebString& path,
                                  WebFileSystemCallbacks* callbacks) {
  fileExists(GURL(path), callbacks);
}

void BrowserFileSystem::directoryExists(const WebString& path,
                                       WebFileSystemCallbacks* callbacks) {
  directoryExists(GURL(path), callbacks);
}

void BrowserFileSystem::readDirectory(const WebString& path,
                                     WebFileSystemCallbacks* callbacks) {
  readDirectory(GURL(path), callbacks);
}

WebKit::WebFileWriter* BrowserFileSystem::createFileWriter(
    const WebString& path, WebKit::WebFileWriterClient* client) {
  return createFileWriter(GURL(path), client);
}

void BrowserFileSystem::move(
    const WebURL& src_path,
    const WebURL& dest_path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->Move(GURL(src_path), GURL(dest_path));
}

void BrowserFileSystem::copy(
    const WebURL& src_path, const WebURL& dest_path,
    WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->Copy(GURL(src_path), GURL(dest_path));
}

void BrowserFileSystem::remove(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->Remove(path, false /* recursive */);
}

void BrowserFileSystem::removeRecursively(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->Remove(path, true /* recursive */);
}

void BrowserFileSystem::readMetadata(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->GetMetadata(path);
}

void BrowserFileSystem::createFile(
    const WebURL& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->CreateFile(path, exclusive);
}

void BrowserFileSystem::createDirectory(
    const WebURL& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->CreateDirectory(path, exclusive, false);
}

void BrowserFileSystem::fileExists(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->FileExists(path);
}

void BrowserFileSystem::directoryExists(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->DirectoryExists(path);
}

void BrowserFileSystem::readDirectory(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(callbacks)->ReadDirectory(path);
}

WebFileWriter* BrowserFileSystem::createFileWriter(
    const WebURL& path, WebFileWriterClient* client) {
  return new BrowserFileWriter(path, client, file_system_context_.get());
}

FileSystemOperation* BrowserFileSystem::GetNewOperation(
    WebFileSystemCallbacks* callbacks) {
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(AsWeakPtr(), callbacks);
  FileSystemOperation* operation = new FileSystemOperation(
      dispatcher, base::MessageLoopProxy::CreateForCurrentThread(),
      file_system_context_.get(), NULL);
  return operation;
}
