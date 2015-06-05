// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/permission_manager.h"

#include "include/cef_client.h"
#include "include/cef_geolocation_handler.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/callback.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/browser/permission_type.h"

namespace {

class CefGeolocationCallbackImpl : public CefGeolocationCallback {
 public:
  typedef base::Callback<void(content::PermissionStatus)> CallbackType;

  explicit CefGeolocationCallbackImpl(const CallbackType& callback)
      : callback_(callback) {}

  void Continue(bool allow) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        if (allow) {
          content::GeolocationProvider::GetInstance()->
              UserDidOptIntoLocationServices();
        }

        callback_.Run(allow ? content::PERMISSION_STATUS_GRANTED :
                              content::PERMISSION_STATUS_DENIED);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefGeolocationCallbackImpl::Continue, this, allow));
    }
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefGeolocationCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefGeolocationCallbackImpl);
};

}  // namespace

CefPermissionManager::CefPermissionManager()
    : PermissionManager() {
}

CefPermissionManager::~CefPermissionManager() {
}

void CefPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    int request_id,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  CEF_REQUIRE_UIT();

  if (permission != content::PermissionType::GEOLOCATION) {
    callback.Run(content::PERMISSION_STATUS_DENIED);
    return;
  }

  bool proceed = false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForHost(render_frame_host);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefGeolocationHandler> handler =
          client->GetGeolocationHandler();
      if (handler.get()) {
        CefRefPtr<CefGeolocationCallbackImpl> callbackImpl(
            new CefGeolocationCallbackImpl(callback));

        // Notify the handler.
        proceed = handler->OnRequestGeolocationPermission(
            browser.get(), requesting_origin.spec(), request_id,
            callbackImpl.get());
        if (!proceed)
          callbackImpl->Disconnect();
      }
    }
  }

  if (!proceed) {
    // Disallow geolocation access by default.
    callback.Run(content::PERMISSION_STATUS_DENIED);
  }
}

void CefPermissionManager::CancelPermissionRequest(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    int request_id,
    const GURL& requesting_origin) {
  CEF_REQUIRE_UIT();

  if (permission != content::PermissionType::GEOLOCATION)
    return;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForHost(render_frame_host);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefGeolocationHandler> handler =
          client->GetGeolocationHandler();
      if (handler.get()) {
        handler->OnCancelGeolocationPermission(browser.get(),
                                               requesting_origin.spec(),
                                               request_id);
      }
    }
  }
}

void CefPermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

content::PermissionStatus CefPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return content::PERMISSION_STATUS_DENIED;
}

void CefPermissionManager::RegisterPermissionUsage(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

int CefPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  return -1;
}

void CefPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}
