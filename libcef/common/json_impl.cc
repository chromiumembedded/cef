// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_parser.h"
#include "libcef/common/values_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace {

int GetJSONReaderOptions(cef_json_parser_options_t options) {
  int op = base::JSON_PARSE_RFC;
  if (options & JSON_PARSER_ALLOW_TRAILING_COMMAS) {
    op |= base::JSON_ALLOW_TRAILING_COMMAS;
  }
  return op;
}

int GetJSONWriterOptions(cef_json_writer_options_t options) {
  int op = 0;
  if (options & JSON_WRITER_OMIT_BINARY_VALUES) {
    op |= base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES;
  }
  if (options & JSON_WRITER_OMIT_DOUBLE_TYPE_PRESERVATION) {
    op |= base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION;
  }
  if (options & JSON_WRITER_PRETTY_PRINT) {
    op |= base::JSONWriter::OPTIONS_PRETTY_PRINT;
  }
  return op;
}

}  // namespace

CefRefPtr<CefValue> CefParseJSON(const CefString& json_string,
                                 cef_json_parser_options_t options) {
  const std::string& json = json_string.ToString();
  return CefParseJSON(json.data(), json.size(), options);
}

CefRefPtr<CefValue> CefParseJSON(const void* json,
                                 size_t json_size,
                                 cef_json_parser_options_t options) {
  if (!json || json_size == 0) {
    return nullptr;
  }
  absl::optional<base::Value> parse_result = base::JSONReader::Read(
      base::StringPiece(static_cast<const char*>(json), json_size),
      GetJSONReaderOptions(options));
  if (parse_result) {
    return new CefValueImpl(std::move(parse_result.value()));
  }
  return nullptr;
}

CefRefPtr<CefValue> CefParseJSONAndReturnError(
    const CefString& json_string,
    cef_json_parser_options_t options,
    CefString& error_msg_out) {
  const std::string& json = json_string.ToString();

  std::string error_msg;
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      json, GetJSONReaderOptions(options));
  if (result.has_value()) {
    return new CefValueImpl(std::move(*result));
  }

  error_msg_out = result.error().message;
  return nullptr;
}

CefString CefWriteJSON(CefRefPtr<CefValue> node,
                       cef_json_writer_options_t options) {
  if (!node.get() || !node->IsValid()) {
    return CefString();
  }

  CefValueImpl* impl = static_cast<CefValueImpl*>(node.get());
  CefValueImpl::ScopedLockedValue scoped_value(impl);

  std::string json_string;
  if (base::JSONWriter::WriteWithOptions(
          *scoped_value.value(), GetJSONWriterOptions(options), &json_string)) {
    return json_string;
  }
  return CefString();
}
