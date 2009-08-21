// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#pragma once

#include "cef.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

// Load a resource of type BINARY
bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes);


// Implementation of the stream read handler for reading in-memory data.
class ClientReadHandler : public CefThreadSafeBase<CefReadHandler>
{
public:
  ClientReadHandler(LPBYTE pBytes, DWORD dwSize);

  // Read raw binary data.
  virtual size_t Read(void* ptr, size_t size, size_t n);
	
  // Seek to the specified offset position. |whence| may be any one of
  // SEEK_CUR, SEEK_END or SEEK_SET.
  virtual int Seek(long offset, int whence);
	
  // Return the current offset position.
  virtual long Tell();

  // Return non-zero if at end of file.
  virtual int Eof();

private:
  LPBYTE bytes_;
  DWORD size_, offset_;
};
