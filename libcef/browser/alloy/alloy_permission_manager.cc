// Copyright 2022 The Chromium Embedded Framework Authors. Portions copyright
// 2015 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/alloy/alloy_permission_manager.h"

#include "base/callback.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace {

bool IsAllowed(blink::PermissionType permission) {
  return permission == blink::PermissionType::WINDOW_PLACEMENT;
}

blink::mojom::PermissionStatus GetPermissionStatusFromType(
    blink::PermissionType permission) {
  return IsAllowed(permission) ? blink::mojom::PermissionStatus::GRANTED
                               : blink::mojom::PermissionStatus::DENIED;
}

}  // namespace

void AlloyPermissionManager::RequestPermission(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }
  std::move(callback).Run(GetPermissionStatusFromType(permission));
}

void AlloyPermissionManager::RequestPermissions(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        permissions.size(), blink::mojom::PermissionStatus::DENIED));
    return;
  }
  std::vector<blink::mojom::PermissionStatus> result;
  for (const auto& permission : permissions) {
    result.push_back(GetPermissionStatusFromType(permission));
  }
  std::move(callback).Run(result);
}

void AlloyPermissionManager::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        permissions.size(), blink::mojom::PermissionStatus::DENIED));
    return;
  }
  std::vector<blink::mojom::PermissionStatus> result;
  for (const auto& permission : permissions) {
    result.push_back(GetPermissionStatusFromType(permission));
  }
  std::move(callback).Run(result);
}

blink::mojom::PermissionStatus AlloyPermissionManager::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return GetPermissionStatusFromType(permission);
}

blink::mojom::PermissionStatus
AlloyPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsNestedWithinFencedFrame())
    return blink::mojom::PermissionStatus::DENIED;
  return GetPermissionStatusFromType(permission);
}

blink::mojom::PermissionStatus
AlloyPermissionManager::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return GetPermissionStatusFromType(permission);
}

void AlloyPermissionManager::ResetPermission(blink::PermissionType permission,
                                             const GURL& requesting_origin,
                                             const GURL& embedding_origin) {}

AlloyPermissionManager::SubscriptionId
AlloyPermissionManager::SubscribePermissionStatusChange(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return SubscriptionId();
}

void AlloyPermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}