// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_RESPONSE_FILTER_WRAPPER_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_RESPONSE_FILTER_WRAPPER_H_

#include "include/cef_response_filter.h"

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace net_service {

// Create a filter handler that will read from |source_handle| and pass the data
// through |filter|. If filtering cannot be initialized then |source_handle|
// will be returned, otherwise a new handle for retrieving the filtered output
// will be returned. If filtering fails after initialization then
// |error_callback| will be executed.
mojo::ScopedDataPipeConsumerHandle CreateResponseFilterHandler(
    CefRefPtr<CefResponseFilter> filter,
    mojo::ScopedDataPipeConsumerHandle source_handle,
    base::OnceClosure error_callback);

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_RESPONSE_FILTER_WRAPPER_H_
