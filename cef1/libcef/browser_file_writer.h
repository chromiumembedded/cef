// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FILE_WRITER_H_
#define CEF_LIBCEF_BROWSER_FILE_WRITER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/fileapi/webfilewriter_base.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace fileapi {
class FileSystemContext;
}

// An implementation of WebFileWriter for use in test_shell and DRT.
class BrowserFileWriter : public fileapi::WebFileWriterBase,
                          public base::SupportsWeakPtr<BrowserFileWriter> {
 public:
  BrowserFileWriter(
      const GURL& path,
      WebKit::WebFileWriterClient* client,
      fileapi::FileSystemContext* file_system_context);
  virtual ~BrowserFileWriter();

  // Called by CefProcessIOThread when the context is created and destroyed.
  static void InitializeOnIOThread(net::URLRequestContext* request_context) {
    request_context_ = request_context;
  }
  static void CleanupOnIOThread() {
    request_context_ = NULL;
  }

 protected:
  // WebFileWriterBase overrides
  virtual void DoTruncate(const GURL& path, int64 offset) OVERRIDE;
  virtual void DoWrite(const GURL& path, const GURL& blob_url,
                       int64 offset) OVERRIDE;
  virtual void DoCancel() OVERRIDE;

 private:
  class IOThreadProxy;
  scoped_refptr<IOThreadProxy> io_thread_proxy_;
  static net::URLRequestContext* request_context_;
};

#endif  // CEF_LIBCEF_BROWSER_FILE_WRITER_H_
