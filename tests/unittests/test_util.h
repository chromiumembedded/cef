// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_UNITTESTS_TEST_UTIL_H_
#define CEF_TESTS_UNITTESTS_TEST_UTIL_H_
#pragma once

#include "include/cef_process_message.h"
#include "include/cef_values.h"

// Test if two binary values are equal.
void TestBinaryEqual(CefRefPtr<CefBinaryValue> val1,
                     CefRefPtr<CefBinaryValue> val2);

// Test if two list values are equal.
void TestListEqual(CefRefPtr<CefListValue> val1,
                   CefRefPtr<CefListValue> val2);

// Test if two dictionary values are equal.
void TestDictionaryEqual(CefRefPtr<CefDictionaryValue> val1,
                         CefRefPtr<CefDictionaryValue> val2);

// Test if two process message values are equal.
void TestProcessMessageEqual(CefRefPtr<CefProcessMessage> val1,
                             CefRefPtr<CefProcessMessage> val2);

#endif  // CEF_TESTS_UNITTESTS_TEST_UTIL_H_
