// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "string_util.h"
#include "string_util_mac.h"
#include "util.h"

#if defined(__APPLE__)

#if TARGET_RT_BIG_ENDIAN
#define WCHAR_T_ENCODING NSUTF32BigEndianStringEncoding
#else
#define WCHAR_T_ENCODING NSUTF32LittleEndianStringEncoding
#endif

std::wstring StringToWString(const std::string& s)
{
  NSString* nsstr = [[NSString alloc] initWithCString:s.c_str()];
  std::wstring wstr = NSStringToWString(nsstr);
  [nsstr release];
  return wstr;
}

std::string WStringToString(const std::wstring& s)
{
	NSString* nsstr = WStringToNSString(s);
  std::string str = [nsstr UTF8String];
  [nsstr release];
  return str;
}

NSString* WStringToNSString(const std::wstring& str)
{
  return [[NSString alloc] initWithBytes:(void*)str.c_str()
                                  length:str.length()*4
                                encoding:WCHAR_T_ENCODING];
}

std::wstring NSStringToWString(NSString* str)
{
  NSData* data = [str dataUsingEncoding:WCHAR_T_ENCODING];
  return std::wstring((wchar_t*)[data bytes],
                      [data length] / sizeof(wchar_t));
}

#endif // defined(__APPLE__)
