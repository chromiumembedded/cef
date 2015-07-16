// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace extensions {
namespace cef {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the CEF extensions code or otherwise required by CEF. See
// libcef/common/extensions/api/README.txt for additional details.
void EnsureBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
