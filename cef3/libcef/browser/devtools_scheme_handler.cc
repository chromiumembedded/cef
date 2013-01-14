// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/devtools_scheme_handler.h"
#include <string>
#include "libcef/browser/internal_scheme_handler.h"
#include "base/string_util.h"
#include "grit/devtools_resources_map.h"

namespace scheme {

const char kChromeDevToolsScheme[] = "chrome-devtools";
const char kChromeDevToolsHost[] = "devtools";
const char kChromeDevToolsURL[] = "chrome-devtools://devtools/";

namespace {

class Delegate : public InternalHandlerDelegate {
 public:
  Delegate() {}

  virtual bool OnRequest(CefRefPtr<CefRequest> request,
                         Action* action) OVERRIDE {
    GURL url = GURL(request->GetURL().ToString());
    std::string path = url.path();
    if (path.length() > 0)
      path = path.substr(1);

    for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
      if (base::strcasecmp(kDevtoolsResources[i].name,
                           path.c_str()) == 0) {
        action->resource_id = kDevtoolsResources[i].value;
        return true;
      }
    }

    return false;
  }
};

}  // namespace

void RegisterChromeDevToolsHandler() {
  CefRegisterSchemeHandlerFactory(
      kChromeDevToolsScheme,
      kChromeDevToolsHost,
      CreateInternalHandlerFactory(
          make_scoped_ptr<InternalHandlerDelegate>(new Delegate())));
}

}  // namespace scheme
