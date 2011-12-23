// Copyright (c) 2011 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// The contents of this file are only available to applications that link
// against the libcef_dll_wrapper target.
//

#ifndef _CEF_BYTE_READ_HANDLER_H
#define _CEF_BYTE_READ_HANDLER_H

#include "../cef_base.h"
#include "../cef_stream.h"

///
// Thread safe implementation of the CefReadHandler class for reading an
// in-memory array of bytes.
///
class CefByteReadHandler : public CefReadHandler
{
public:
  ///
  // Create a new object for reading an array of bytes. An optional |source|
  // reference can be kept to keep the underlying data source from being
  // released while the reader exists.
  ///
  CefByteReadHandler(const unsigned char* bytes, size_t size,
                     CefRefPtr<CefBase> source);

  ///
  // Read raw binary data.
  ///
  virtual size_t Read(void* ptr, size_t size, size_t n);

  ///
  // Seek to the specified offset position. |whence| may be any one of
  // SEEK_CUR, SEEK_END or SEEK_SET.
  ///
  virtual int Seek(long offset, int whence);

  ///
  // Return the current offset position.
  ///
  virtual long Tell();

  ///
  // Return non-zero if at end of file.
  ///
  virtual int Eof();

private:
  const unsigned char* bytes_;
  size_t size_;
  size_t offset_;
  CefRefPtr<CefBase> source_;

  IMPLEMENT_REFCOUNTING(CefByteReadHandler);
  IMPLEMENT_LOCKING(CefByteReadHandler);
};

#endif // _CEF_BYTE_READ_HANDLER_H
