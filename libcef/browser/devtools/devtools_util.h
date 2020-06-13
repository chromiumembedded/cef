// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_UTIL_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_UTIL_H_
#pragma once

#include "base/strings/string_piece.h"

namespace devtools_util {

// Fast parser for DevTools JSON protocol messages. This implementation makes
// certain assumptions about the JSON object structure (value order and
// formatting) to avoid stateful parsing of messages that may be large
// (sometimes > 1MB in size). The message must be a JSON dictionary that starts
// with a "method" or "id" value which is non-empty and of the expected data
// type. Messages that have a "method" value (event message) may optionally have
// a "params" dictionary. Messages that have an "id" value (result message) must
// have a "result" or "error" dictionary. The dictionary contents are not
// validated and may be empty ("{}").
//
// Example event message:
// {"method":"Target.targetDestroyed","params":{"targetId":"1234..."}}
//
// Example result messages:
// {"id":3,"result":{}}
// {"id":4,"result":{"debuggerId":"-2193881606781505058.81393575456727957"}}
// {"id":5,"error":{"code":-32000,"message":"Not supported"}}
struct ProtocolParser {
  ProtocolParser() = default;

  // Checks for a non-empty JSON dictionary.
  static bool IsValidMessage(const base::StringPiece& message);

  // Returns false if already initialized.
  bool Initialize(const base::StringPiece& message);

  bool IsInitialized() const { return status_ != UNINITIALIZED; }
  bool IsEvent() const { return status_ == EVENT; }
  bool IsResult() const { return status_ == RESULT; }
  bool IsFailure() const { return status_ == FAILURE; }

  void Reset() { status_ = UNINITIALIZED; }

  // For event messages:
  //   "method" string:
  base::StringPiece method_;

  // For result messages:
  //   "id" int:
  int message_id_ = 0;
  //   true if "result" value, false if "error" value:
  bool success_ = false;

  // For both:
  //   "params", "result" or "error" dictionary:
  base::StringPiece params_;

 private:
  enum Status {
    UNINITIALIZED,
    EVENT,    // Event message.
    RESULT,   // Result message.
    FAILURE,  // Parsing failure.
  };
  Status status_ = UNINITIALIZED;
};

}  // namespace devtools_util

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_UTIL_H_