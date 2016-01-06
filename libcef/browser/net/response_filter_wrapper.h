// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CEF_LIBCEF_BROWSER_NET_RESPONSE_FILTER_WRAPPER_H_
#define CEF_LIBCEF_BROWSER_NET_RESPONSE_FILTER_WRAPPER_H_

#include "include/cef_response_filter.h"

#include "base/macros.h"
#include "net/filter/filter.h"

class CefResponseFilterWrapper : public net::Filter {
 public:
  CefResponseFilterWrapper(CefRefPtr<CefResponseFilter> cef_filter,
                           bool has_other_filters);
  ~CefResponseFilterWrapper() override;

  // Decodes the pre-filter data and writes the output into the dest_buffer
  // passed in.
  // The function returns FilterStatus. See filter.h for its description.
  //
  // Upon entry, *dest_len is the total size (in number of chars) of the
  // destination buffer. Upon exit, *dest_len is the actual number of chars
  // written into the destination buffer.
  //
  // This function will fail if there is no pre-filter data in the
  // stream_buffer_. On the other hand, *dest_len can be 0 upon successful
  // return. For example, the internal filter may process some pre-filter data
  // but not produce output yet.
  FilterStatus ReadFilteredData(char* dest_buffer, int* dest_len) override;

 private:
  CefRefPtr<CefResponseFilter> cef_filter_;
  const bool has_other_filters_;

  DISALLOW_COPY_AND_ASSIGN(CefResponseFilterWrapper);
};

#endif  // CEF_LIBCEF_BROWSER_NET_RESPONSE_FILTER_WRAPPER_H_
