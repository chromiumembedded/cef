// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/extension_impl.h"

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/values_impl.h"

#include "extensions/common/extension.h"

CefExtensionImpl::CefExtensionImpl(const extensions::Extension* extension,
                                   CefRequestContext* loader_context,
                                   CefRefPtr<CefExtensionHandler> handler)
    : id_(extension->id()),
      path_(extension->path().value()),
      manifest_(
          new CefDictionaryValueImpl(extension->manifest()->value()->Clone(),
                                     /*read_only=*/true)),
      loader_context_(loader_context),
      handler_(handler) {}

CefString CefExtensionImpl::GetIdentifier() {
  return id_;
}

CefString CefExtensionImpl::GetPath() {
  return path_;
}

CefRefPtr<CefDictionaryValue> CefExtensionImpl::GetManifest() {
  return manifest_;
}

bool CefExtensionImpl::IsSame(CefRefPtr<CefExtension> that) {
  CefExtensionImpl* that_impl = static_cast<CefExtensionImpl*>(that.get());
  if (!that_impl) {
    return false;
  }

  // Maybe the same object.
  if (this == that_impl) {
    return true;
  }

  return id_ == that_impl->id_ && path_ == that_impl->path_ &&
         loader_context_ == that_impl->loader_context_;
}

CefRefPtr<CefExtensionHandler> CefExtensionImpl::GetHandler() {
  return handler_;
}

CefRefPtr<CefRequestContext> CefExtensionImpl::GetLoaderContext() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return nullptr;
  }

  return loader_context_;
}

bool CefExtensionImpl::IsLoaded() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  return !unloaded_;
}

void CefExtensionImpl::Unload() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefExtensionImpl::Unload, this));
    return;
  }

  // Will be NULL for internal extensions. They can't be unloaded.
  if (!loader_context_) {
    return;
  }

  if (unloaded_) {
    return;
  }

  // CefExtensionHandler callbacks triggered by UnloadExtension may check this
  // flag, so set it here.
  unloaded_ = true;

  [[maybe_unused]] const bool result =
      static_cast<CefRequestContextImpl*>(loader_context_)
          ->GetBrowserContext()
          ->UnloadExtension(id_);
  DCHECK(result);
}

void CefExtensionImpl::OnExtensionLoaded() {
  CEF_REQUIRE_UIT();
  if (handler_) {
    handler_->OnExtensionLoaded(this);
  }
}

void CefExtensionImpl::OnExtensionUnloaded() {
  CEF_REQUIRE_UIT();
  // Should not be called for internal extensions.
  DCHECK(loader_context_);

  unloaded_ = true;
  loader_context_ = nullptr;

  if (handler_) {
    handler_->OnExtensionUnloaded(this);
  }
}
