// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//

#include "include/cef_nplugin.h"
#include "include/capi/cef_nplugin_capi.h"
#include "include/cef_version.h"
#include "libcef_dll/cef_logging.h"

CEF_EXPORT int cef_build_revision() {
  return CEF_REVISION;
}

CEF_EXPORT int cef_register_plugin(const cef_plugin_info_t* plugin_info) {
  DCHECK(plugin_info);
  if (!plugin_info)
    return 0;

  return CefRegisterPlugin(*plugin_info);
}
