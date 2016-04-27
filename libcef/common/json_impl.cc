// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_parser.h"
#include "libcef/common/values_impl.h"

#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

namespace {

int GetJSONReaderOptions(cef_json_parser_options_t options) {
  int op = base::JSON_PARSE_RFC;
  if (options & JSON_PARSER_ALLOW_TRAILING_COMMAS)
    op |= base::JSON_ALLOW_TRAILING_COMMAS;
  return op;
}

int GetJSONWriterOptions(cef_json_writer_options_t options) {
  int op = 0;
  if (op & JSON_WRITER_OMIT_BINARY_VALUES)
    op |= base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES;
  if (op & JSON_WRITER_OMIT_DOUBLE_TYPE_PRESERVATION)
    op |= base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION;
  if (op & JSON_WRITER_PRETTY_PRINT)
    op |= base::JSONWriter::OPTIONS_PRETTY_PRINT;
  return op;
}

}  // namespace

CefRefPtr<CefValue> CefParseJSON(const CefString& json_string,
                                 cef_json_parser_options_t options) {
  const std::string& json = json_string.ToString();
  std::unique_ptr<base::Value> parse_result =
      base::JSONReader::Read(json, GetJSONReaderOptions(options));
  if (parse_result)
    return new CefValueImpl(parse_result.release());
  return NULL;
}

CefRefPtr<CefValue> CefParseJSONAndReturnError(
    const CefString& json_string,
    cef_json_parser_options_t options,
    cef_json_parser_error_t& error_code_out,
    CefString& error_msg_out) {
  const std::string& json = json_string.ToString();

  int error_code;
  std::string error_msg;
  std::unique_ptr<base::Value> parse_result = base::JSONReader::ReadAndReturnError(
      json, GetJSONReaderOptions(options), &error_code, &error_msg);
  if (parse_result)
    return new CefValueImpl(parse_result.release());

  error_code_out = static_cast<cef_json_parser_error_t>(error_code);
  error_msg_out = error_msg;
  return NULL;
}

CefString CefWriteJSON(CefRefPtr<CefValue> node,
                       cef_json_writer_options_t options) {
  if (!node.get() || !node->IsValid())
    return CefString();

  CefValueImpl* impl = static_cast<CefValueImpl*>(node.get());
  CefValueImpl::ScopedLockedValue scoped_value(impl);

  std::string json_string;
  if (base::JSONWriter::WriteWithOptions(*scoped_value.value(),
                                         GetJSONWriterOptions(options),
                                         &json_string)) {
    return json_string;
  }
  return CefString();
}
