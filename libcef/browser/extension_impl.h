// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSION_IMPL_H_
#define CEF_LIBCEF_BROWSER_EXTENSION_IMPL_H_
#pragma once

#include <memory>

#include "include/cef_extension.h"
#include "include/cef_extension_handler.h"
#include "include/cef_request_context.h"

namespace extensions {
class Extension;
}

// CefNavigationEntry implementation
class CefExtensionImpl : public CefExtension {
 public:
  CefExtensionImpl(const extensions::Extension* extension,
                   CefRequestContext* loader_context,
                   CefRefPtr<CefExtensionHandler> handler);

  CefExtensionImpl(const CefExtensionImpl&) = delete;
  CefExtensionImpl& operator=(const CefExtensionImpl&) = delete;

  // CefExtension methods.
  CefString GetIdentifier() override;
  CefString GetPath() override;
  CefRefPtr<CefDictionaryValue> GetManifest() override;
  bool IsSame(CefRefPtr<CefExtension> that) override;
  CefRefPtr<CefExtensionHandler> GetHandler() override;
  CefRefPtr<CefRequestContext> GetLoaderContext() override;
  bool IsLoaded() override;
  void Unload() override;

  void OnExtensionLoaded();
  void OnExtensionUnloaded();

  // Use this instead of the GetLoaderContext version during
  // CefRequestContext destruction.
  CefRequestContext* loader_context() const { return loader_context_; }

 private:
  CefString id_;
  CefString path_;
  CefRefPtr<CefDictionaryValue> manifest_;

  CefRequestContext* loader_context_;
  CefRefPtr<CefExtensionHandler> handler_;

  // Only accessed on the UI thread.
  bool unloaded_ = false;

  IMPLEMENT_REFCOUNTING(CefExtensionImpl);
};

#endif  // CEF_LIBCEF_BROWSER_EXTENSION_IMPL_H_
