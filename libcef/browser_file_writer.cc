// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_file_writer.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/glue/webkit_glue.h"

using fileapi::FileSystemCallbackDispatcher;
using fileapi::FileSystemContext;
using fileapi::FileSystemOperation;
using fileapi::WebFileWriterBase;
using WebKit::WebFileWriterClient;
using WebKit::WebString;
using WebKit::WebURL;

net::URLRequestContext* BrowserFileWriter::request_context_ = NULL;

// Helper class to proxy the write and truncate calls to the IO thread,
// and to proxy the results back to the main thead. There is a one-to-one
// relationship between BrowserFileWriters and IOThreadBackends.
class BrowserFileWriter::IOThreadProxy
    : public base::RefCountedThreadSafe<BrowserFileWriter::IOThreadProxy> {
 public:
  IOThreadProxy(const base::WeakPtr<BrowserFileWriter>& simple_writer,
                FileSystemContext* file_system_context)
      : simple_writer_(simple_writer),
        operation_(NULL),
        file_system_context_(file_system_context) {
    io_thread_ = CefThread::GetMessageLoopProxyForThread(CefThread::IO);
    main_thread_ = base::MessageLoopProxy::current();
  }

  virtual ~IOThreadProxy() {
  }

  void Truncate(const GURL& path, int64 offset) {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::Truncate, this, path, offset));
      return;
    }
    DCHECK(!operation_);
    operation_ = GetNewOperation();
    operation_->Truncate(path, offset);
  }

  void Write(const GURL& path, const GURL& blob_url, int64 offset) {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::Write, this, path, blob_url, offset));
      return;
    }
    DCHECK(request_context_);
    DCHECK(!operation_);
    operation_ = GetNewOperation();
    operation_->Write(request_context_, path, blob_url, offset);
  }

  void Cancel() {
    if (!io_thread_->BelongsToCurrentThread()) {
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::Cancel, this));
      return;
    }
    if (!operation_) {
      DidFail(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
      return;
    }
    operation_->Cancel(GetNewOperation());
  }

 private:
  // Inner class to receive callbacks from FileSystemOperation.
  class CallbackDispatcher : public FileSystemCallbackDispatcher {
   public:
    explicit CallbackDispatcher(IOThreadProxy* proxy) : proxy_(proxy) {
    }

    ~CallbackDispatcher() {
      proxy_->ClearOperation();
    }

    virtual void DidSucceed() {
      proxy_->DidSucceed();
    }

    virtual void DidFail(base::PlatformFileError error_code) {
      proxy_->DidFail(error_code);
    }

    virtual void DidWrite(int64 bytes, bool complete) {
      proxy_->DidWrite(bytes, complete);
    }

    virtual void DidReadMetadata(
        const base::PlatformFileInfo&,
        const FilePath&) {
      NOTREACHED();
    }

    virtual void DidReadDirectory(
        const std::vector<base::FileUtilProxy::Entry>& entries,
        bool has_more) {
      NOTREACHED();
    }

    virtual void DidOpenFileSystem(
        const std::string& name,
        const GURL& root) {
      NOTREACHED();
    }

    scoped_refptr<IOThreadProxy> proxy_;
  };

  FileSystemOperation* GetNewOperation() {
    // The FileSystemOperation takes ownership of the CallbackDispatcher.
    return new FileSystemOperation(new CallbackDispatcher(this),
                                   io_thread_, file_system_context_.get());
  }

  void DidSucceed() {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::DidSucceed, this));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidSucceed();
  }

  void DidFail(base::PlatformFileError error_code) {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::DidFail, this, error_code));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidFail(error_code);
  }

  void DidWrite(int64 bytes, bool complete) {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::Bind(&IOThreadProxy::DidWrite, this, bytes, complete));
      return;
    }
    if (simple_writer_)
      simple_writer_->DidWrite(bytes, complete);
  }

  void ClearOperation() {
    DCHECK(io_thread_->BelongsToCurrentThread());
    operation_ = NULL;
  }

  scoped_refptr<base::MessageLoopProxy> io_thread_;
  scoped_refptr<base::MessageLoopProxy> main_thread_;

  // Only used on the main thread.
  base::WeakPtr<BrowserFileWriter> simple_writer_;

  // Only used on the io thread.
  FileSystemOperation* operation_;

  scoped_refptr<FileSystemContext> file_system_context_;
};


BrowserFileWriter::BrowserFileWriter(
    const GURL& path,
    WebFileWriterClient* client,
    FileSystemContext* file_system_context)
  : WebFileWriterBase(path, client),
    io_thread_proxy_(new IOThreadProxy(AsWeakPtr(), file_system_context)) {
}

BrowserFileWriter::~BrowserFileWriter() {
}

void BrowserFileWriter::DoTruncate(const GURL& path, int64 offset) {
  io_thread_proxy_->Truncate(path, offset);
}

void BrowserFileWriter::DoWrite(
    const GURL& path, const GURL& blob_url, int64 offset) {
  io_thread_proxy_->Write(path, blob_url, offset);
}

void BrowserFileWriter::DoCancel() {
  io_thread_proxy_->Cancel();
}
