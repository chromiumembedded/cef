// Copyright (c) 2009 Marshall A. Greenblatt. All rights reserved.
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

#ifndef _CEF_STRING_H
#define _CEF_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cef_export.h"
#include <wchar.h>

// CEF strings are NUL-terminated wide character strings prefixed with a size
// value, similar to the Microsoft BSTR type.  Use the below API functions for
// allocating, managing and freeing CEF strings.
typedef wchar_t* cef_string_t;

// Return the wide character length of the CEF string as allocated by
// cef_string_alloc_len(). The returned value does not include the NUL
// terminating character. This length may differ from the string length
// as returned by wcslen().
CEF_EXPORT size_t cef_string_length(cef_string_t str);

// Allocate and return a new CEF string that is a copy of |str|.  If |str| is
// NULL or if allocation fails NULL will be returned.  If |str| is of length
// 0 a valid empty CEF string will be returned.
CEF_EXPORT cef_string_t cef_string_alloc(const wchar_t* str);

// Allocate and return a new CEF string that is a copy of |str|. |len| is the
// wide character length of the new CEF string not including the NUL
// terminating character. |str| will be copied without checking for a NUL
// terminating character. If |str| is NULL or if allocation fails NULL will
// be returned.  If |str| is of length 0 a valid empty CEF string will be
// returned.
CEF_EXPORT cef_string_t cef_string_alloc_length(const wchar_t* str,
                                                size_t len);

// Reallocate an existing CEF string.  The contents of |oldstr| will be
// replaced with the contents of |newstr|; |newstr| may not be NULL. Returns 1
// on success and 0 on failure.
CEF_EXPORT int cef_string_realloc(cef_string_t* oldstr, const wchar_t* newstr);

// Reallocate an existing CEF string.  If |newstr| is NULL the contents of
// |oldstr| will remain unchanged; otherwise, they will be replaced with the
// contents of |newstr|. |len| is the new wide character length of the string
// not including the NUL terminating character.  Returns 1 on success and 0
// on failure.
CEF_EXPORT int cef_string_realloc_length(cef_string_t* oldstr,
                                         const wchar_t* newstr,
                                         size_t len);

// Free a CEF string.  If |str| is NULL this function does nothing.
CEF_EXPORT void cef_string_free(cef_string_t str);

#ifdef __cplusplus
}
#endif

#endif // _CEF_STRING_H
