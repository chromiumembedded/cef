// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_IMPL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_IMPL_H_
#pragma once

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cef/include/cef_extension.h"

class CefRequestContextImpl;

namespace extensions {
class Extension;
}  // namespace extensions

namespace cef {

// Snapshot wrapper around a chromium extensions::Extension. The metadata fields
// (path, name, version, manifest, enabled/visible flags) are captured at
// construction so that accessors are safe to call from any thread. Mutating
// methods (SetEnabled, Uninstall) post to the UI thread and forward to the
// owning request context's CefExtensionRegistryObserver.
class CefExtensionImpl : public CefExtension {
 public:
  CefExtensionImpl(const extensions::Extension* extension,
                   bool enabled,
                   CefRefPtr<CefRequestContext> request_context);

  CefExtensionImpl(const CefExtensionImpl&) = delete;
  CefExtensionImpl& operator=(const CefExtensionImpl&) = delete;

  ~CefExtensionImpl() override;

  // CefExtension methods:
  CefString GetIdentifier() override;
  CefString GetPath() override;
  CefString GetName() override;
  CefString GetVersion() override;
  CefRefPtr<CefDictionaryValue> GetManifest() override;
  bool IsEnabled() override;
  bool ShouldShowInExtensionsUI() override;
  CefRefPtr<CefImage> GetActionIcon(CefRefPtr<CefBrowser> browser) override;
  bool IsValid() override;
  bool IsSame(CefRefPtr<CefExtension> that) override;
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  void SetEnabled(bool enable) override;
  void Uninstall() override;

 private:
  const std::string id_;
  const base::FilePath path_;
  const std::string name_;
  const std::string version_;
  const base::DictValue manifest_;
  const bool enabled_;
  const bool show_in_extensions_ui_;

  // Weak: the owning request context outlives an in-flight task only if it has
  // not been destroyed. Held as a CefRefPtr<CefRequestContext> so users of
  // GetRequestContext() get a public-API pointer; IsValid() checks the impl.
  CefRefPtr<CefRequestContext> request_context_;

  IMPLEMENT_REFCOUNTING(CefExtensionImpl);
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_IMPL_H_
