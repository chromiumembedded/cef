// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _ZIP_READER_IMPL_H
#define _ZIP_READER_IMPL_H

#include "../include/cef.h"
#include "base/threading/platform_thread.h"
#include "third_party/zlib/contrib/minizip/unzip.h"
#include <sstream>

// Implementation of CefZipReader
class CefZipReaderImpl : public CefZipReader
{
public:
  CefZipReaderImpl();
  ~CefZipReaderImpl();

  // Initialize the reader context.
  bool Initialize(CefRefPtr<CefStreamReader> stream);

  virtual bool MoveToFirstFile();
  virtual bool MoveToNextFile();
  virtual bool MoveToFile(const CefString& fileName, bool caseSensitive);
  virtual bool Close();
  virtual CefString GetFileName();
  virtual long GetFileSize();
  virtual time_t GetFileLastModified();
  virtual bool OpenFile(const CefString& password);
  virtual bool CloseFile();
  virtual int ReadFile(void* buffer, size_t bufferSize);
  virtual long Tell();
  virtual bool Eof();

  bool GetFileInfo();
  
  // Verify that the reader exists and is being accessed from the correct
  // thread.
  bool VerifyContext();

protected:
  base::PlatformThreadId supported_thread_id_;
  unzFile reader_;
  bool has_fileopen_;
  bool has_fileinfo_;
  CefString filename_;
  long filesize_;
  time_t filemodified_;

  IMPLEMENT_REFCOUNTING(CefZipReaderImpl);
};

#endif // _ZIP_READER_IMPL_H
