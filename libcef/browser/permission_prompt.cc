// Copyright 2022 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/permission_prompt.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/features/runtime.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"

namespace permission_prompt {

namespace {

uint64_t g_next_prompt_id = 0;

using DelegateCallback =
    base::OnceCallback<void(cef_permission_request_result_t)>;

class CefPermissionPromptCallbackImpl : public CefPermissionPromptCallback {
 public:
  using CallbackType = base::OnceCallback<void(cef_permission_request_result_t,
                                               bool /*notify_delegate*/)>;

  explicit CefPermissionPromptCallbackImpl(CallbackType&& callback)
      : callback_(std::move(callback)) {}

  CefPermissionPromptCallbackImpl(const CefPermissionPromptCallbackImpl&) =
      delete;
  CefPermissionPromptCallbackImpl& operator=(
      const CefPermissionPromptCallbackImpl&) = delete;

  // Don't need to execute the callback in this destructor because this object
  // will always be kept alive until after the CefPermissionPrompt is destroyed,
  // and that object will disconnect/execute the callback in its destructor.
  ~CefPermissionPromptCallbackImpl() override = default;

  void Continue(cef_permission_request_result_t result) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        auto callback = base::BindOnce(std::move(callback_), result,
                                       /*notify_delegate=*/true);
        if (safe_to_run_sync_) {
          std::move(callback).Run();
        } else {
          CEF_POST_TASK(CEF_UIT, std::move(callback));
        }
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefPermissionPromptCallbackImpl::Continue,
                                   this, result));
    }
  }

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }
  bool IsDisconnected() const { return callback_.is_null(); }

  void MarkSafeToRunSync() { safe_to_run_sync_ = true; }

 private:
  // Callback execution from inside CreatePermissionPromptImpl must be async,
  // otherwise PermissionRequestManager state will be incorrect.
  bool safe_to_run_sync_ = false;

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefPermissionPromptCallbackImpl);
};

// Implementation based on PermissionPromptAndroid.
class CefPermissionPrompt : public permissions::PermissionPrompt {
 public:
  explicit CefPermissionPrompt(Delegate* delegate) : delegate_(delegate) {
    DCHECK(delegate_);
  }

  CefPermissionPrompt(const CefPermissionPrompt&) = delete;
  CefPermissionPrompt& operator=(const CefPermissionPrompt&) = delete;

  // Expect to be destroyed (and the UI needs to go) when:
  // 1. A navigation happens, tab/webcontents is being closed; with the current
  //    GetTabSwitchingBehavior() implementation, this instance survives the tab
  //    being backgrounded.
  // 2. The permission request is resolved (accept, deny, dismiss).
  // 3. A higher priority request comes in.
  ~CefPermissionPrompt() override {
    CEF_REQUIRE_UIT();
    if (callback_) {
      // If the callback is non-null at this point then we still need to execute
      // it in order to notify the client.
      auto callback = callback_->Disconnect();
      if (!callback.is_null()) {
        std::move(callback).Run(CEF_PERMISSION_RESULT_IGNORE,
                                /*notify_delegate=*/false);
      }
    }
  }

  // Used to associate the client callback when OnShowPermissionPrompt is
  // handled.
  void AttachClientCallback(
      CefRefPtr<CefPermissionPromptCallbackImpl> callback) {
    DCHECK(callback);
    callback_ = callback;
    callback_->MarkSafeToRunSync();
  }

  // Used to tie Delegate access to this object's lifespan.
  DelegateCallback MakeDelegateCallback() {
    return base::BindOnce(&CefPermissionPrompt::NotifyDelegate,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // PermissionPrompt methods:
  bool UpdateAnchor() override { return true; }
  TabSwitchingBehavior GetTabSwitchingBehavior() override {
    return TabSwitchingBehavior::kKeepPromptAlive;
  }
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override {
    return permissions::PermissionPromptDisposition::CUSTOM_MODAL_DIALOG;
  }
  absl::optional<gfx::Rect> GetViewBoundsInScreen() const override {
    return absl::nullopt;
  }
  bool ShouldFinalizeRequestAfterDecided() const override { return true; }

 private:
  // We don't expose AcceptThisTime() because it's a special case for
  // Geolocation (see DCHECK in PrefProvider::SetWebsiteSetting).
  void NotifyDelegate(cef_permission_request_result_t result) {
    switch (result) {
      case CEF_PERMISSION_RESULT_ACCEPT:
        delegate_->Accept();
        break;
      case CEF_PERMISSION_RESULT_DENY:
        delegate_->Deny();
        break;
      case CEF_PERMISSION_RESULT_DISMISS:
        delegate_->Dismiss();
        break;
      case CEF_PERMISSION_RESULT_IGNORE:
        delegate_->Ignore();
        break;
    }
  }

  // |delegate_| is the PermissionRequestManager, which owns this object.
  const raw_ptr<Delegate> delegate_;

  CefRefPtr<CefPermissionPromptCallbackImpl> callback_;

  base::WeakPtrFactory<CefPermissionPrompt> weak_ptr_factory_{this};
};

// |notify_delegate| will be false if called from the CefPermissionPrompt
// destructor.
void ExecuteResult(CefRefPtr<CefBrowserHostBase> browser,
                   uint64_t prompt_id,
                   DelegateCallback delegate_callback,
                   cef_permission_request_result_t result,
                   bool notify_delegate) {
  CEF_REQUIRE_UIT();

  if (auto client = browser->GetClient()) {
    if (auto handler = client->GetPermissionHandler()) {
      handler->OnDismissPermissionPrompt(browser, prompt_id, result);
    }
  }

  if (notify_delegate) {
    // Will be a no-op if this executes after the CefPermissionPrompt was
    // destroyed.
    std::move(delegate_callback).Run(result);
  }
}

cef_permission_request_types_t GetCefRequestType(
    permissions::RequestType type) {
  switch (type) {
    case permissions::RequestType::kAccessibilityEvents:
      return CEF_PERMISSION_TYPE_ACCESSIBILITY_EVENTS;
    case permissions::RequestType::kArSession:
      return CEF_PERMISSION_TYPE_AR_SESSION;
    case permissions::RequestType::kCameraPanTiltZoom:
      return CEF_PERMISSION_TYPE_CAMERA_PAN_TILT_ZOOM;
    case permissions::RequestType::kCameraStream:
      return CEF_PERMISSION_TYPE_CAMERA_STREAM;
    case permissions::RequestType::kCapturedSurfaceControl:
      return CEF_PERMISSION_TYPE_CAPTURED_SURFACE_CONTROL;
    case permissions::RequestType::kClipboard:
      return CEF_PERMISSION_TYPE_CLIPBOARD;
    case permissions::RequestType::kDiskQuota:
      return CEF_PERMISSION_TYPE_DISK_QUOTA;
    case permissions::RequestType::kLocalFonts:
      return CEF_PERMISSION_TYPE_LOCAL_FONTS;
    case permissions::RequestType::kGeolocation:
      return CEF_PERMISSION_TYPE_GEOLOCATION;
    case permissions::RequestType::kIdleDetection:
      return CEF_PERMISSION_TYPE_IDLE_DETECTION;
    case permissions::RequestType::kMicStream:
      return CEF_PERMISSION_TYPE_MIC_STREAM;
    case permissions::RequestType::kMidi:
      return CEF_PERMISSION_TYPE_MIDI;
    case permissions::RequestType::kMidiSysex:
      return CEF_PERMISSION_TYPE_MIDI_SYSEX;
    case permissions::RequestType::kMultipleDownloads:
      return CEF_PERMISSION_TYPE_MULTIPLE_DOWNLOADS;
    case permissions::RequestType::kNotifications:
      return CEF_PERMISSION_TYPE_NOTIFICATIONS;
#if BUILDFLAG(IS_WIN)
    case permissions::RequestType::kProtectedMediaIdentifier:
      return CEF_PERMISSION_TYPE_PROTECTED_MEDIA_IDENTIFIER;
#endif
    case permissions::RequestType::kRegisterProtocolHandler:
      return CEF_PERMISSION_TYPE_REGISTER_PROTOCOL_HANDLER;
    case permissions::RequestType::kStorageAccess:
      return CEF_PERMISSION_TYPE_STORAGE_ACCESS;
    case permissions::RequestType::kTopLevelStorageAccess:
      return CEF_PERMISSION_TYPE_TOP_LEVEL_STORAGE_ACCESS;
    case permissions::RequestType::kVrSession:
      return CEF_PERMISSION_TYPE_VR_SESSION;
    case permissions::RequestType::kWindowManagement:
      return CEF_PERMISSION_TYPE_WINDOW_MANAGEMENT;
    case permissions::RequestType::kFileSystemAccess:
      return CEF_PERMISSION_TYPE_FILE_SYSTEM_ACCESS;
  }

  DCHECK(false);
  return CEF_PERMISSION_TYPE_NONE;
}

uint32_t GetRequestedPermissions(
    permissions::PermissionPrompt::Delegate* delegate) {
  uint32_t permissions = CEF_PERMISSION_TYPE_NONE;
  for (const auto& request : delegate->Requests()) {
    permissions |= GetCefRequestType(request->request_type());
  }
  return permissions;
}

std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPromptImpl(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate,
    bool* default_handling) {
  CEF_REQUIRE_UIT();

  if (auto browser = CefBrowserHostBase::GetBrowserForContents(web_contents)) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetPermissionHandler()) {
        auto permission_prompt =
            std::make_unique<CefPermissionPrompt>(delegate);

        const auto prompt_id = ++g_next_prompt_id;
        auto callback =
            base::BindOnce(&ExecuteResult, browser, prompt_id,
                           permission_prompt->MakeDelegateCallback());

        CefRefPtr<CefPermissionPromptCallbackImpl> callbackImpl(
            new CefPermissionPromptCallbackImpl(std::move(callback)));
        bool handled = handler->OnShowPermissionPrompt(
            browser, prompt_id, delegate->GetRequestingOrigin().spec(),
            GetRequestedPermissions(delegate), callbackImpl.get());

        if (callbackImpl->IsDisconnected() || handled) {
          // Callback execution will be async.
          LOG_IF(ERROR, !handled)
              << "Should return true from OnShowPermissionPrompt when "
                 "executing the callback";
          *default_handling = false;
          permission_prompt->AttachClientCallback(callbackImpl);
          return permission_prompt;
        } else {
          // Proceed with default handling. |callback| is discarded without
          // execution.
          callback = callbackImpl->Disconnect();
        }
      }
    }
  }

  if (cef::IsAlloyRuntimeEnabled()) {
    LOG(INFO) << "Implement OnShowPermissionPrompt to override default IGNORE "
                 "handling of permission prompts.";
  }

  // Proceed with default handling. This will be IGNORE with the Alloy runtime
  // and default UI prompt with the Chrome runtime.
  *default_handling = true;
  return nullptr;
}

}  // namespace

void RegisterCreateCallback() {
  SetCreatePermissionPromptFunction(&CreatePermissionPromptImpl);
}

}  // namespace permission_prompt
