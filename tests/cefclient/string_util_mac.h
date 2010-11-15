// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_STRING_UTIL_MAC_H
#define _CEFCLIENT_STRING_UTIL_MAC_H

#if defined(__APPLE__)

#include <string>
#import <Cocoa/Cocoa.h>

// Convert a std::wstring to an NSString. The NSString must be released by the
// caller.
NSString* WStringToNSString(const std::wstring& str);

// Convert an NSString to a std::wstring.
std::wstring NSStringToWString(NSString* str);

#endif // defined(__APPLE__)

#endif // _CEFCLIENT_STRING_UTIL_MAC_H
