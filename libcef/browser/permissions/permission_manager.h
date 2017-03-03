// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
#define CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_

#include "libcef/browser/permissions/permission_context.h"

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/permission_manager.h"

class CefBrowserContext;

namespace content {
enum class PermissionType;
class WebContents;
};  // namespace content

// Implementation based on chrome/browser/permissions/permission_manager.h
class CefPermissionManager : public KeyedService,
                             public content::PermissionManager,
                             public content_settings::Observer {
 public:
  explicit CefPermissionManager(CefBrowserContext* profile);
  ~CefPermissionManager() override;

  // content::CefPermissionManager implementation.
  int RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
      override;
  int RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(
          const std::vector<blink::mojom::PermissionStatus>&)>& callback)
      override;
  void CancelPermissionRequest(int request_id) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
      override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  class PendingRequest;
  using PendingRequestsMap = IDMap<std::unique_ptr<PendingRequest>>;

  struct Subscription;
  using SubscriptionsMap = IDMap<std::unique_ptr<Subscription>>;

  // Called when a permission was decided for a given PendingRequest. The
  // PendingRequest is identified by its |request_id| and the permission is
  // identified by its |permission_id|. If the PendingRequest contains more than
  // one permission, it will wait for the remaining permissions to be resolved.
  // When all the permissions have been resolved, the PendingRequest's callback
  // is run.
  void OnPermissionsRequestResponseStatus(
      int request_id,
      int permission_id,
      blink::mojom::PermissionStatus status);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  CefBrowserContext* profile_;
  PendingRequestsMap pending_requests_;
  SubscriptionsMap subscriptions_;
  CefPermissionContext context_;

  base::WeakPtrFactory<CefPermissionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefPermissionManager);
};

#endif // CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
