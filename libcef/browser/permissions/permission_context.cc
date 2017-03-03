// Copyright 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/permissions/permission_context.h"

#include "include/cef_client.h"
#include "include/cef_geolocation_handler.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/permissions/permission_util.h"
#include "libcef/browser/thread_util.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "device/geolocation/geolocation_provider.h"

namespace {

// Whether the permission should be restricted to secure origins.
bool IsRestrictedToSecureOrigins(content::PermissionType permission) {
  return false;
}

class CefGeolocationCallbackImpl : public CefGeolocationCallback {
 public:
  typedef CefPermissionContext::PermissionDecidedCallback CallbackType;

  explicit CefGeolocationCallbackImpl(const CallbackType& callback)
      : callback_(callback) {}

  void Continue(bool allow) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        if (allow) {
          device::GeolocationProvider::GetInstance()->
              UserDidOptIntoLocationServices();
        }

        callback_.Run(allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
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

CefPermissionContext::CefPermissionContext(CefBrowserContext* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
}

bool CefPermissionContext::SupportsPermission(
    content::PermissionType permission) {
  // Only Geolocation permissions are currently supported.
  return permission == content::PermissionType::GEOLOCATION;
}

void CefPermissionContext::RequestPermission(
    content::PermissionType permission,
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DecidePermission(permission,
                   web_contents,
                   id,
                   requesting_frame.GetOrigin(),
                   web_contents->GetLastCommittedURL().GetOrigin(),
                   callback);
}

void CefPermissionContext::CancelPermissionRequest(
    content::PermissionType permission,
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(permission == content::PermissionType::GEOLOCATION);

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForContents(web_contents);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefGeolocationHandler> handler =
          client->GetGeolocationHandler();
      if (handler.get())
        handler->OnCancelGeolocationPermission(browser.get(), id.request_id());
    }
  }
}

void CefPermissionContext::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  profile_->GetHostContentSettingsMap()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin),
      permission_util::PermissionTypeToContentSetting(permission),
      std::string(),
      CONTENT_SETTING_DEFAULT);
}

ContentSetting CefPermissionContext::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (IsRestrictedToSecureOrigins(permission) &&
      !content::IsOriginSecure(requesting_origin)) {
    return CONTENT_SETTING_BLOCK;
  }

  return profile_->GetHostContentSettingsMap()->GetContentSetting(
      requesting_origin,
      embedding_origin,
      permission_util::PermissionTypeToContentSetting(permission),
      std::string());
}

void CefPermissionContext::DecidePermission(
    content::PermissionType permission,
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid()) {
    NotifyPermissionSet(permission, id, requesting_origin, embedding_origin,
                        callback, false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  if (IsRestrictedToSecureOrigins(permission) &&
      !content::IsOriginSecure(requesting_origin)) {
    NotifyPermissionSet(permission, id, requesting_origin, embedding_origin,
                        callback, false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  ContentSetting content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_origin,
          embedding_origin,
          permission_util::PermissionTypeToContentSetting(permission),
          std::string());

  if (content_setting == CONTENT_SETTING_ALLOW ||
      content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(permission, id, requesting_origin, embedding_origin,
                        callback, false /* persist */, content_setting);
    return;
  }

  QueryPermission(
      permission, id, requesting_origin, embedding_origin,
      base::Bind(&CefPermissionContext::NotifyPermissionSet,
                 weak_ptr_factory_.GetWeakPtr(), permission, id,
                 requesting_origin, embedding_origin, callback,
                 false /* persist */));
}

void CefPermissionContext::QueryPermission(
    content::PermissionType permission,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const PermissionDecidedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(permission == content::PermissionType::GEOLOCATION);

  bool proceed = false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForFrame(id.render_process_id(),
                                             id.render_frame_id());
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
            browser.get(), requesting_origin.spec(), id.request_id(),
            callbackImpl.get());
        if (!proceed)
          callbackImpl->Disconnect();
      }
    }
  }

  if (!proceed) {
    // Disallow geolocation access by default.
    callback.Run(CONTENT_SETTING_BLOCK);
  }
}

void CefPermissionContext::NotifyPermissionSet(
    content::PermissionType permission,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (persist) {
    UpdateContentSetting(permission, requesting_origin, embedding_origin,
                         content_setting);
  }

  if (content_setting == CONTENT_SETTING_DEFAULT) {
    content_setting =
        profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
            permission_util::PermissionTypeToContentSetting(permission),
            nullptr);
  }

  DCHECK_NE(content_setting, CONTENT_SETTING_DEFAULT);
  callback.Run(content_setting);
}

void CefPermissionContext::UpdateContentSetting(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  profile_->GetHostContentSettingsMap()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin),
      permission_util::PermissionTypeToContentSetting(permission),
      std::string(),
      content_setting);
}
