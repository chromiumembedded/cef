// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_INTERNAL_SCHEME_HANDLER_H_
#define CEF_LIBCEF_BROWSER_INTERNAL_SCHEME_HANDLER_H_
#pragma once

#include <string>
#include "include/cef_scheme.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"

namespace scheme {

// All methods will be called on the browser process IO thread.
class InternalHandlerDelegate {
 public:
  class Action {
   public:
    Action();

    // Set to the appropriate value or leave empty to have it determined based
    // on the file extension.
    std::string mime_type;

    // Option 1: Provide a stream for the resource contents. Set |stream_size|
    // to the stream size or to -1 if unknown.
    CefRefPtr<CefStreamReader> stream;
    int stream_size;

    // Option 2: Specify a resource id to load static content.
    int resource_id;

    // Option 3: Redirect to the specified URL.
    GURL redirect_url;
  };

  virtual ~InternalHandlerDelegate() {}

  // Populate |action| and return true if the request was handled.
  virtual bool OnRequest(CefRefPtr<CefRequest> request,
                         Action* action) = 0;
};

// Create an internal scheme handler factory. The factory will take ownership of
// |delegate|.
CefRefPtr<CefSchemeHandlerFactory> CreateInternalHandlerFactory(
    scoped_ptr<InternalHandlerDelegate> delegate);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_INTERNAL_SCHEME_HANDLER_H_
