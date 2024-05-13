// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_STREAM_IMPL_H_
#define CEF_LIBCEF_BROWSER_STREAM_IMPL_H_
#pragma once

#include <stdio.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "cef/include/cef_stream.h"

// Implementation of CefStreamReader for files.
class CefFileReader : public CefStreamReader {
 public:
  CefFileReader(FILE* file, bool close);
  ~CefFileReader() override;

  size_t Read(void* ptr, size_t size, size_t n) override;
  int Seek(int64_t offset, int whence) override;
  int64_t Tell() override;
  int Eof() override;
  bool MayBlock() override { return true; }

 protected:
  bool close_;
  raw_ptr<FILE> file_;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefFileReader);
};

// Implementation of CefStreamWriter for files.
class CefFileWriter : public CefStreamWriter {
 public:
  CefFileWriter(FILE* file, bool close);
  ~CefFileWriter() override;

  size_t Write(const void* ptr, size_t size, size_t n) override;
  int Seek(int64_t offset, int whence) override;
  int64_t Tell() override;
  int Flush() override;
  bool MayBlock() override { return true; }

 protected:
  raw_ptr<FILE> file_;
  bool close_;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefFileWriter);
};

// Implementation of CefStreamReader for byte buffers.
class CefBytesReader : public CefStreamReader {
 public:
  // |data| is always copied.
  CefBytesReader(void* data, int64_t datasize);

  size_t Read(void* ptr, size_t size, size_t n) override;
  int Seek(int64_t offset, int whence) override;
  int64_t Tell() override;
  int Eof() override;
  bool MayBlock() override { return false; }

  // |data| is always copied.
  void SetData(void* data, int64_t datasize);

 protected:
  std::vector<unsigned char> data_;
  int64_t offset_ = 0;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefBytesReader);
};

// Implementation of CefStreamReader for handlers.
class CefHandlerReader : public CefStreamReader {
 public:
  explicit CefHandlerReader(CefRefPtr<CefReadHandler> handler)
      : handler_(handler) {}

  size_t Read(void* ptr, size_t size, size_t n) override {
    return handler_->Read(ptr, size, n);
  }
  int Seek(int64_t offset, int whence) override {
    return handler_->Seek(offset, whence);
  }
  int64_t Tell() override { return handler_->Tell(); }
  int Eof() override { return handler_->Eof(); }
  bool MayBlock() override { return handler_->MayBlock(); }

 protected:
  CefRefPtr<CefReadHandler> handler_;

  IMPLEMENT_REFCOUNTING(CefHandlerReader);
};

// Implementation of CefStreamWriter for handlers.
class CefHandlerWriter : public CefStreamWriter {
 public:
  explicit CefHandlerWriter(CefRefPtr<CefWriteHandler> handler)
      : handler_(handler) {}

  size_t Write(const void* ptr, size_t size, size_t n) override {
    return handler_->Write(ptr, size, n);
  }
  int Seek(int64_t offset, int whence) override {
    return handler_->Seek(offset, whence);
  }
  int64_t Tell() override { return handler_->Tell(); }
  int Flush() override { return handler_->Flush(); }
  bool MayBlock() override { return handler_->MayBlock(); }

 protected:
  CefRefPtr<CefWriteHandler> handler_;

  IMPLEMENT_REFCOUNTING(CefHandlerWriter);
};

#endif  // CEF_LIBCEF_BROWSER_STREAM_IMPL_H_
