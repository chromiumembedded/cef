// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PERMISSION_MANAGER_H_
#define CEF_LIBCEF_BROWSER_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "content/public/browser/permission_manager.h"

class CefPermissionManager : public content::PermissionManager {
 public:
  CefPermissionManager();
  ~CefPermissionManager() override;

  // PermissionManager implementation.
  int RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void CancelPermissionRequest(int request_id) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  content::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void RegisterPermissionUsage(content::PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  struct PendingRequest;
  using PendingRequestsMap = IDMap<PendingRequest, IDMapOwnPointer>;
  PendingRequestsMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(CefPermissionManager);
};

#endif // CEF_LIBCEF_BROWSER_PERMISSION_MANAGER_H_
