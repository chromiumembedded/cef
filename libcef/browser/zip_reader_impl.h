// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ZIP_READER_IMPL_H_
#define CEF_LIBCEF_BROWSER_ZIP_READER_IMPL_H_
#pragma once

#include <sstream>

#include "base/threading/platform_thread.h"
#include "include/cef_zip_reader.h"
#include "third_party/zlib/contrib/minizip/unzip.h"

// Implementation of CefZipReader
class CefZipReaderImpl : public CefZipReader {
 public:
  CefZipReaderImpl();
  ~CefZipReaderImpl() override;

  // Initialize the reader context.
  bool Initialize(CefRefPtr<CefStreamReader> stream);

  bool MoveToFirstFile() override;
  bool MoveToNextFile() override;
  bool MoveToFile(const CefString& fileName, bool caseSensitive) override;
  bool Close() override;
  CefString GetFileName() override;
  int64_t GetFileSize() override;
  CefBaseTime GetFileLastModified() override;
  bool OpenFile(const CefString& password) override;
  bool CloseFile() override;
  int ReadFile(void* buffer, size_t bufferSize) override;
  int64_t Tell() override;
  bool Eof() override;

  bool GetFileInfo();

  // Verify that the reader exists and is being accessed from the correct
  // thread.
  bool VerifyContext();

 protected:
  base::PlatformThreadId supported_thread_id_;
  unzFile reader_ = nullptr;
  bool has_fileopen_ = false;
  bool has_fileinfo_ = false;
  CefString filename_;
  int64_t filesize_ = 0;
  time_t filemodified_ = 0;

  IMPLEMENT_REFCOUNTING(CefZipReaderImpl);
};

#endif  // CEF_LIBCEF_BROWSER_ZIP_READER_IMPL_H_
