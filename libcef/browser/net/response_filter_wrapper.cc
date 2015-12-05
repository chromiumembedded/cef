// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/response_filter_wrapper.h"

#include "base/logging.h"

// FilterType is used for logging purposes only.
CefResponseFilterWrapper::CefResponseFilterWrapper(
    CefRefPtr<CefResponseFilter> cef_filter,
    bool has_other_filters)
    : Filter(Filter::FILTER_TYPE_UNSUPPORTED),
      cef_filter_(cef_filter),
      has_other_filters_(has_other_filters) {
  DCHECK(cef_filter_.get());
}

CefResponseFilterWrapper::~CefResponseFilterWrapper() {
}

net::Filter::FilterStatus CefResponseFilterWrapper::ReadFilteredData(
    char* dest_buffer,
    int* dest_len) {
  if (!dest_buffer || !dest_len || *dest_len <= 0)
    return net::Filter::FILTER_ERROR;

  size_t data_in_size = static_cast<size_t>(stream_data_len_);
  size_t data_in_read = 0;
  size_t data_out_size = static_cast<size_t>(*dest_len);
  size_t data_out_write = 0;

  cef_response_filter_status_t cef_status = cef_filter_->Filter(
      next_stream_data_, data_in_size, data_in_read,
      dest_buffer, data_out_size, data_out_write);

  // Return early if there's an error.
  if (cef_status == RESPONSE_FILTER_ERROR)
    return net::Filter::FILTER_ERROR;

  // Normalize the out values.
  if (data_in_read > data_in_size) {
    LOG(ERROR) <<
        "potential buffer overflow; data_in_read exceeds data_in_size";
    data_in_read = data_in_size;
  }
  if (data_out_write > data_out_size) {
    LOG(ERROR) <<
        "potential buffer overflow; data_out_write exceeds data_out_size";
    data_out_write = data_out_size;
  }

  // Output the number of bytes written.
  *dest_len = static_cast<int>(data_out_write);

  if (data_in_size - data_in_read > 0U) {
    // There are bytes left so adjust the stream pointer and return FILTER_OK.
    next_stream_data_ += data_in_read;
    stream_data_len_ -= static_cast<int>(data_in_read);
    return Filter::FILTER_OK;
  }

  // No bytes left. Might need more data or might be done.
  // If |has_other_filters_| is true then we must return FILTER_NEED_MORE_DATA
  // or additional data will not be sent.
  next_stream_data_ = nullptr;
  stream_data_len_ = 0;
  if (cef_status == RESPONSE_FILTER_NEED_MORE_DATA || has_other_filters_)
    return Filter::FILTER_NEED_MORE_DATA;
  return Filter::FILTER_DONE;
}
