// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/permissions/permission_manager.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/permissions/permission_util.h"

#include "base/callback.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace {

// Helper method to convert ContentSetting to PermissionStatus.
PermissionStatus ContentSettingToPermissionStatus(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_SESSION_ONLY:
      return PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return PermissionStatus::ASK;
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return PermissionStatus::DENIED;
}

// Helper method to convert PermissionStatus to ContentSetting.
ContentSetting PermissionStatusToContentSetting(PermissionStatus status) {
  switch (status) {
    case PermissionStatus::GRANTED:
      return CONTENT_SETTING_ALLOW;
    case PermissionStatus::DENIED:
      return CONTENT_SETTING_BLOCK;
    case PermissionStatus::ASK:
      return CONTENT_SETTING_ASK;
  }

  NOTREACHED();
  return CONTENT_SETTING_BLOCK;
}

// Wrap a callback taking a PermissionStatus to pass it as a callback taking a
// ContentSetting.
void ContentSettingToPermissionStatusCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    ContentSetting setting) {
  callback.Run(ContentSettingToPermissionStatus(setting));
}

// Returns whether the permission has a constant PermissionStatus value (i.e.
// always approved or always denied).
bool IsConstantPermission(PermissionType type) {
  switch (type) {
    case PermissionType::MIDI:
      return true;
    default:
      return false;
  }
}

void PermissionRequestResponseCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  callback.Run(vector[0]);
}

// Function used for handling permission types which do not change their
// value i.e. they are always approved or always denied etc.
// CONTENT_SETTING_DEFAULT is returned if the permission needs further handling.
// This function should only be called when IsConstantPermission has returned
// true for the PermissionType.
ContentSetting GetContentSettingForConstantPermission(PermissionType type) {
  DCHECK(IsConstantPermission(type));
  switch (type) {
    case PermissionType::MIDI:
      return CONTENT_SETTING_ALLOW;
    default:
      return CONTENT_SETTING_DEFAULT;
  }
}

PermissionStatus GetPermissionStatusForConstantPermission(PermissionType type) {
  return ContentSettingToPermissionStatus(
      GetContentSettingForConstantPermission(type));
}

}  // anonymous namespace

class CefPermissionManager::PendingRequest {
 public:
  PendingRequest(content::RenderFrameHost* render_frame_host,
                 const std::vector<PermissionType> permissions,
                 const base::Callback<void(
                     const std::vector<PermissionStatus>&)>& callback)
    : render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      callback_(callback),
      permissions_(permissions),
      results_(permissions.size(), PermissionStatus::DENIED),
      remaining_results_(permissions.size()) {
  }

  void SetPermissionStatus(int permission_id, PermissionStatus status) {
    DCHECK(!IsComplete());

    results_[permission_id] = status;
    --remaining_results_;
  }

  bool IsComplete() const {
    return remaining_results_ == 0;
  }

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

  const base::Callback<void(const std::vector<PermissionStatus>&)>
  callback() const {
    return callback_;
  }

  std::vector<PermissionType> permissions() const {
    return permissions_;
  }

  std::vector<PermissionStatus> results() const {
    return results_;
  }

 private:
  int render_process_id_;
  int render_frame_id_;
  const base::Callback<void(const std::vector<PermissionStatus>&)> callback_;
  std::vector<PermissionType> permissions_;
  std::vector<PermissionStatus> results_;
  size_t remaining_results_;
};

struct CefPermissionManager::Subscription {
  PermissionType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  base::Callback<void(PermissionStatus)> callback;
  ContentSetting current_value;
};

CefPermissionManager::CefPermissionManager(CefBrowserContext* profile)
    : profile_(profile),
      context_(profile),
      weak_ptr_factory_(this) {
}

CefPermissionManager::~CefPermissionManager() {
  if (!subscriptions_.IsEmpty())
    profile_->GetHostContentSettingsMap()->RemoveObserver(this);
}

int CefPermissionManager::RequestPermission(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(PermissionStatus)>& callback) {
  return RequestPermissions(
      std::vector<PermissionType>(1, permission),
      render_frame_host,
      requesting_origin,
      user_gesture,
      base::Bind(&PermissionRequestResponseCallbackWrapper, callback));
}

int CefPermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(
        const std::vector<PermissionStatus>&)>& callback) {
  if (permissions.empty()) {
    callback.Run(std::vector<PermissionStatus>());
    return kNoPendingOperation;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();

  std::unique_ptr<PendingRequest> pending_request =
      base::MakeUnique<PendingRequest>(
          render_frame_host, permissions, callback);
  int request_id = pending_requests_.Add(std::move(pending_request));

  const PermissionRequestID request(render_frame_host, request_id);

  for (size_t i = 0; i < permissions.size(); ++i) {
    const PermissionType permission = permissions[i];

    if (IsConstantPermission(permission) ||
        !context_.SupportsPermission(permission)) {
      OnPermissionsRequestResponseStatus(request_id, i,
          GetPermissionStatus(permission, requesting_origin, embedding_origin));
      continue;
    }

    context_.RequestPermission(
        permission, web_contents, request, requesting_origin,
        base::Bind(&ContentSettingToPermissionStatusCallbackWrapper,
            base::Bind(
                &CefPermissionManager::OnPermissionsRequestResponseStatus,
                weak_ptr_factory_.GetWeakPtr(), request_id, i)));
  }

  // The request might have been resolved already.
  if (!pending_requests_.Lookup(request_id))
    return kNoPendingOperation;

  return request_id;
}

void CefPermissionManager::OnPermissionsRequestResponseStatus(
    int request_id,
    int permission_id,
    PermissionStatus status) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  pending_request->SetPermissionStatus(permission_id, status);

  if (!pending_request->IsComplete())
    return;

  pending_request->callback().Run(pending_request->results());
  pending_requests_.Remove(request_id);
}

void CefPermissionManager::CancelPermissionRequest(int request_id) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  if (!pending_request)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(pending_request->render_process_id(),
                                       pending_request->render_frame_id());
  DCHECK(render_frame_host);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  const PermissionRequestID request(pending_request->render_process_id(),
                                    pending_request->render_frame_id(),
                                    request_id);
  for (PermissionType permission : pending_request->permissions()) {
    if (!context_.SupportsPermission(permission))
      continue;
    context_.CancelPermissionRequest(permission, web_contents, request);
  }
  pending_requests_.Remove(request_id);
}

void CefPermissionManager::ResetPermission(PermissionType permission,
                                           const GURL& requesting_origin,
                                           const GURL& embedding_origin) {
  if (!context_.SupportsPermission(permission))
    return;
  context_.ResetPermission(permission, requesting_origin, embedding_origin);
}

PermissionStatus CefPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (IsConstantPermission(permission))
    return GetPermissionStatusForConstantPermission(permission);

  if (!context_.SupportsPermission(permission))
    return PermissionStatus::DENIED;

  return ContentSettingToPermissionStatus(
      context_.GetPermissionStatus(permission, requesting_origin,
                                   embedding_origin));
}

int CefPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  if (subscriptions_.IsEmpty())
    profile_->GetHostContentSettingsMap()->AddObserver(this);

  std::unique_ptr<Subscription> subscription = base::MakeUnique<Subscription>();
  subscription->permission = permission;
  subscription->requesting_origin = requesting_origin;
  subscription->embedding_origin = embedding_origin;
  subscription->callback = callback;

  subscription->current_value = PermissionStatusToContentSetting(
      GetPermissionStatus(permission,
                          subscription->requesting_origin,
                          subscription->embedding_origin));

  return subscriptions_.Add(std::move(subscription));
}

void CefPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
  // Whether |subscription_id| is known will be checked by the Remove() call.
  subscriptions_.Remove(subscription_id);

  if (subscriptions_.IsEmpty())
    profile_->GetHostContentSettingsMap()->RemoveObserver(this);
}

void CefPermissionManager::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  std::list<base::Closure> callbacks;

  for (SubscriptionsMap::iterator iter(&subscriptions_);
       !iter.IsAtEnd(); iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (permission_util::PermissionTypeToContentSetting(
            subscription->permission) != content_type) {
      continue;
    }

    if (primary_pattern.IsValid() &&
        !primary_pattern.Matches(subscription->requesting_origin))
      continue;
    if (secondary_pattern.IsValid() &&
        !secondary_pattern.Matches(subscription->embedding_origin))
      continue;

    ContentSetting new_value = PermissionStatusToContentSetting(
        GetPermissionStatus(subscription->permission,
                            subscription->requesting_origin,
                            subscription->embedding_origin));
    if (subscription->current_value == new_value)
      continue;

    subscription->current_value = new_value;

    // Add the callback to |callbacks| which will be run after the loop to
    // prevent re-entrance issues.
    callbacks.push_back(
        base::Bind(subscription->callback,
                   ContentSettingToPermissionStatus(new_value)));
  }

  for (const auto& callback : callbacks)
    callback.Run();
}
