// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SOURCE_STREAM_H_
#define CEF_LIBCEF_BROWSER_NET_SOURCE_STREAM_H_

#include "include/cef_response_filter.h"

#include "base/macros.h"
#include "net/filter/filter_source_stream.h"

class CefSourceStream : public net::FilterSourceStream {
 public:
  CefSourceStream(CefRefPtr<CefResponseFilter> cef_filter,
                  std::unique_ptr<net::SourceStream> upstream);

  int FilterData(net::IOBuffer* output_buffer,
                 int output_buffer_size,
                 net::IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool upstream_eof_reached) override;
  std::string GetTypeAsString() const override;

 private:
  CefRefPtr<CefResponseFilter> cef_filter_;

  cef_response_filter_status_t last_status_ = RESPONSE_FILTER_NEED_MORE_DATA;

  DISALLOW_COPY_AND_ASSIGN(CefSourceStream);
};

#endif  // CEF_LIBCEF_BROWSER_NET_SOURCE_STREAM_H_