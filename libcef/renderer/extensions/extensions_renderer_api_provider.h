// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_RENDERER_API_PROVIDER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_RENDERER_API_PROVIDER_H_

#include "extensions/renderer/extensions_renderer_api_provider.h"

namespace extensions {

// Provides capabilities for the set of extension APIs defined for the
// extensions shell.
class CefExtensionsRendererAPIProvider
    : public ExtensionsRendererAPIProvider {
 public:
  CefExtensionsRendererAPIProvider() = default;
  CefExtensionsRendererAPIProvider(
      const CefExtensionsRendererAPIProvider&) = delete;
  CefExtensionsRendererAPIProvider& operator=(
      const CefExtensionsRendererAPIProvider&) = delete;
  ~CefExtensionsRendererAPIProvider() override = default;

  void RegisterNativeHandlers(ModuleSystem* module_system,
                              NativeExtensionBindingsSystem* bindings_system,
                              V8SchemaRegistry* v8_schema_registry,
                              ScriptContext* context) const override;
  void AddBindingsSystemHooks(
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system) const override;
  void PopulateSourceMap(ResourceBundleSourceMap* source_map) const override;
  void EnableCustomElementAllowlist() const override;
  void RequireWebViewModules(ScriptContext* context) const override;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_RENDERER_API_PROVIDER_H_
