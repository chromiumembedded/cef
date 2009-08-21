// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "stdafx.h"
#include "resource_util.h"


bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes)
{
  extern HINSTANCE hInst;
	HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(binaryId),
                            MAKEINTRESOURCE(256));
	if(hRes)
	{
		HGLOBAL hGlob = LoadResource(hInst, hRes);
		if(hGlob)
		{
			dwSize = SizeofResource(hInst, hRes);
			pBytes = (LPBYTE)LockResource(hGlob);
			if(dwSize > 0 && pBytes)
				return true;
		}
	}

	return false;
}


// ClientReadHandler implementation

ClientReadHandler::ClientReadHandler(LPBYTE pBytes, DWORD dwSize)
  : bytes_(pBytes), size_(dwSize), offset_(0) {}

size_t ClientReadHandler::Read(void* ptr, size_t size, size_t n)
{
  Lock();
  size_t s = (size_ - offset_) / size;
  size_t ret = min(n, s);
  memcpy(ptr, bytes_ + offset_, ret * size);
  offset_ += ret * size;
  Unlock();
  return ret;
}
	
int ClientReadHandler::Seek(long offset, int whence)
{
  int rv = -1L;
  Lock();
  switch(whence) {
  case SEEK_CUR:
    if(offset_ + offset > size_) {
      break;	
    }
    offset_ += offset;
    rv = offset_;
    break;
  case SEEK_END:
    if(offset > (int)size_) {
      break;
    }
    offset_ = size_ - offset;
    rv = offset_;
  case SEEK_SET:
    if(offset > (int)size_) {
      break;	
    }
    offset_ = offset;
    rv = offset_;
    break;
  }
  Unlock();

  return rv;
}
	
long ClientReadHandler::Tell()
{
  Lock();
  long rv = offset_;
  Unlock();
  return rv;
}

int ClientReadHandler::Eof()
{
  Lock();
  int rv = (offset_ >= size_);
  Unlock();
  return rv;
}
