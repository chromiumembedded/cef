// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/devtools/devtools_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace devtools_util {

namespace {

bool IsValidDictionary(const base::StringPiece& str, bool allow_empty) {
  return str.length() >= (allow_empty ? 2 : 3) && str[0] == '{' &&
         str[str.length() - 1] == '}';
}

// Example:
// {"method":"Target.targetDestroyed","params":{"targetId":"1234..."}}
bool ParseEvent(const base::StringPiece& message,
                base::StringPiece& method,
                base::StringPiece& params) {
  static const char kMethodStart[] = "{\"method\":\"";
  static const char kMethodEnd[] = "\"";
  static const char kParamsStart[] = ",\"params\":";

  if (!base::StartsWith(message, kMethodStart)) {
    return false;
  }

  const size_t method_start = sizeof(kMethodStart) - 1;
  const size_t method_end = message.find(kMethodEnd, method_start);
  if (method_end == base::StringPiece::npos) {
    return false;
  }
  method = message.substr(method_start, method_end - method_start);
  if (method.empty()) {
    return false;
  }

  size_t remainder_start = method_end + sizeof(kMethodEnd) - 1;
  if (remainder_start == message.size() - 1) {
    // No more contents.
    params = base::StringPiece();
  } else {
    const base::StringPiece& remainder = message.substr(remainder_start);
    if (base::StartsWith(remainder, kParamsStart)) {
      // Stop immediately before the message closing bracket.
      remainder_start += sizeof(kParamsStart) - 1;
      params =
          message.substr(remainder_start, message.size() - 1 - remainder_start);
    } else {
      // Invalid format.
      return false;
    }

    if (!IsValidDictionary(params, /*allow_empty=*/true)) {
      return false;
    }
  }

  return true;
}

// Examples:
// {"id":3,"result":{}}
// {"id":4,"result":{"debuggerId":"-2193881606781505058.81393575456727957"}}
// {"id":5,"error":{"code":-32000,"message":"Not supported"}}
bool ParseResult(const base::StringPiece& message,
                 int& message_id,
                 bool& success,
                 base::StringPiece& result) {
  static const char kIdStart[] = "{\"id\":";
  static const char kIdEnd[] = ",";
  static const char kResultStart[] = "\"result\":";
  static const char kErrorStart[] = "\"error\":";

  if (!base::StartsWith(message, kIdStart)) {
    return false;
  }

  const size_t id_start = sizeof(kIdStart) - 1;
  const size_t id_end = message.find(kIdEnd, id_start);
  if (id_end == base::StringPiece::npos) {
    return false;
  }
  const base::StringPiece& id_str = message.substr(id_start, id_end - id_start);
  if (id_str.empty() || !base::StringToInt(id_str, &message_id)) {
    return false;
  }

  size_t remainder_start = id_end + sizeof(kIdEnd) - 1;
  const base::StringPiece& remainder = message.substr(remainder_start);
  if (base::StartsWith(remainder, kResultStart)) {
    // Stop immediately before the message closing bracket.
    remainder_start += sizeof(kResultStart) - 1;
    result =
        message.substr(remainder_start, message.size() - 1 - remainder_start);
    success = true;
  } else if (base::StartsWith(remainder, kErrorStart)) {
    // Stop immediately before the message closing bracket.
    remainder_start += sizeof(kErrorStart) - 1;
    result =
        message.substr(remainder_start, message.size() - 1 - remainder_start);
    success = false;
  } else {
    // Invalid format.
    return false;
  }

  if (!IsValidDictionary(result, /*allow_empty=*/true)) {
    return false;
  }

  return true;
}

}  // namespace

// static
bool ProtocolParser::IsValidMessage(const base::StringPiece& message) {
  return IsValidDictionary(message, /*allow_empty=*/false);
}

bool ProtocolParser::Initialize(const base::StringPiece& message) {
  if (status_ != UNINITIALIZED) {
    return false;
  }

  if (ParseEvent(message, method_, params_)) {
    status_ = EVENT;
  } else if (ParseResult(message, message_id_, success_, params_)) {
    status_ = RESULT;
  } else {
    status_ = FAILURE;
  }
  return true;
}

}  // namespace devtools_util
