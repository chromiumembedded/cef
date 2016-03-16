// Copyright 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "components/content_settings/core/common/content_settings.h"

class CefBrowserContext;

namespace content {
enum class PermissionType;
class WebContents;
};  // namespace content

// Based on chrome/browser/permissions/permission_context_base.h
class CefPermissionContext {
 public:
  explicit CefPermissionContext(CefBrowserContext* profile);

  using BrowserPermissionCallback = base::Callback<void(ContentSetting)>;
  using PermissionDecidedCallback = base::Callback<void(ContentSetting)>;

  // Returns true if support exists for querying the embedder about the
  // specified permission type.
  bool SupportsPermission(content::PermissionType permission);

  // The renderer is requesting permission to push messages.
  // When the answer to a permission request has been determined, |callback|
  // should be called with the result.
  void RequestPermission(content::PermissionType permission,
                         content::WebContents* web_contents,
                         const PermissionRequestID& id,
                         const GURL& requesting_frame,
                         const BrowserPermissionCallback& callback);

  // Withdraw an existing permission request, no op if the permission request
  // was already cancelled by some other means.
  void CancelPermissionRequest(content::PermissionType permission,
                               content::WebContents* web_contents,
                               const PermissionRequestID& id);

  // Resets the permission to its default value.
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin);

  // Returns whether the permission has been granted, denied...
  ContentSetting GetPermissionStatus(content::PermissionType permission,
                                     const GURL& requesting_origin,
                                     const GURL& embedding_origin) const;

 private:
  // Decide whether the permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an infobar.
  void DecidePermission(content::PermissionType permission,
                        content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        const BrowserPermissionCallback& callback);

  void QueryPermission(content::PermissionType permission,
                       const PermissionRequestID& id,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin,
                       const PermissionDecidedCallback& callback);

  void NotifyPermissionSet(content::PermissionType permission,
                           const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           ContentSetting content_setting);

  // Store the decided permission as a content setting.
  void UpdateContentSetting(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      ContentSetting content_setting);

  CefBrowserContext* profile_;

  base::WeakPtrFactory<CefPermissionContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefPermissionContext);
};

#endif // CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_H_
