// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/source_stream.h"

#include <utility>

#include "net/base/io_buffer.h"

// Use TYPE_INVALID so that URLRequestJob::NotifyHeadersComplete() doesn't
// assume that the "content-length" header is accurate.
CefSourceStream::CefSourceStream(
    CefRefPtr<CefResponseFilter> cef_filter,
    std::unique_ptr<net::SourceStream> upstream)
  : net::FilterSourceStream(net::SourceStream::TYPE_INVALID,
                            std::move(upstream)),
    cef_filter_(cef_filter) {
}

int CefSourceStream::FilterData(net::IOBuffer* output_buffer,
                                int output_buffer_size,
                                net::IOBuffer* input_buffer,
                                int input_buffer_size,
                                int* consumed_bytes,
                                bool upstream_eof_reached) {
  if (!output_buffer || output_buffer_size <= 0)
    return net::ERR_CONTENT_DECODING_FAILED;

  if (input_buffer_size == 0 && last_status_ == RESPONSE_FILTER_DONE) {
    // No more input data. Respect the client's desire to be done with
    // outputting data.
    *consumed_bytes = 0;
    return 0;
  }

  size_t data_in_size = static_cast<size_t>(input_buffer_size);
  size_t data_in_read = 0;
  size_t data_out_size = static_cast<size_t>(output_buffer_size);
  size_t data_out_written = 0;

  last_status_ = cef_filter_->Filter(
      data_in_size > 0 ? input_buffer->data() : nullptr,
      data_in_size, data_in_read,
      output_buffer->data(), data_out_size, data_out_written);

  // Return early if there's an error.
  if (last_status_ == RESPONSE_FILTER_ERROR)
    return net::ERR_CONTENT_DECODING_FAILED;

  // Validate the out values.
  if (data_in_read > data_in_size) {
    LOG(ERROR) << "potential buffer overflow; data_in_read > data_in_size";
    return net::ERR_CONTENT_DECODING_FAILED;
  }
  if (data_out_written > data_out_size) {
    LOG(ERROR) << "potential buffer overflow; data_out_written > data_out_size";
    return net::ERR_CONTENT_DECODING_FAILED;
  }

  // If FilterData() returns 0, *|consumed_bytes| must be equal to
  // |input_buffer_size|.
  if (data_out_written == 0 && data_in_read != data_in_size) {
    LOG(ERROR) << "when no data is written all input must be consumed; "
                  "data_out_written == 0 && data_in_read != data_in_size";
    return net::ERR_CONTENT_DECODING_FAILED;
  }

  *consumed_bytes = static_cast<int>(data_in_read);

  // Output the number of bytes written.
  return static_cast<int>(data_out_written);
}

std::string CefSourceStream::GetTypeAsString() const {
  return "cef_filter";
}
