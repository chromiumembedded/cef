// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
#define CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
#pragma once

#include <memory>
#include <queue>
#include <string>

#include "include/cef_frame.h"

#include "base/synchronization/lock.h"
#include "cef/libcef/common/mojom/cef.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/page_transition_types.h"

namespace content {
class RenderFrameHost;
struct Referrer;
}  // namespace content

class GURL;

class CefBrowserInfo;
class CefBrowserHostBase;

// Implementation of CefFrame. CefFrameHostImpl objects should always be created
// or retrieved via CefBrowerInfo.
class CefFrameHostImpl : public CefFrame, public cef::mojom::BrowserFrame {
 public:
  // Create a temporary sub-frame.
  CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                   int64_t parent_frame_id);

  // Create a frame backed by a RFH and owned by CefBrowserInfo.
  CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                   content::RenderFrameHost* render_frame_host);

  CefFrameHostImpl(const CefFrameHostImpl&) = delete;
  CefFrameHostImpl& operator=(const CefFrameHostImpl&) = delete;

  ~CefFrameHostImpl() override;

  // CefFrame methods
  bool IsValid() override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void Paste() override;
  void Delete() override;
  void SelectAll() override;
  void ViewSource() override;
  void GetSource(CefRefPtr<CefStringVisitor> visitor) override;
  void GetText(CefRefPtr<CefStringVisitor> visitor) override;
  void LoadRequest(CefRefPtr<CefRequest> request) override;
  void LoadURL(const CefString& url) override;
  void ExecuteJavaScript(const CefString& jsCode,
                         const CefString& scriptUrl,
                         int startLine) override;
  bool IsMain() override;
  bool IsFocused() override;
  CefString GetName() override;
  int64 GetIdentifier() override;
  CefRefPtr<CefFrame> GetParent() override;
  CefString GetURL() override;
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefV8Context> GetV8Context() override;
  void VisitDOM(CefRefPtr<CefDOMVisitor> visitor) override;
  CefRefPtr<CefURLRequest> CreateURLRequest(
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefURLRequestClient> client) override;
  void SendProcessMessage(CefProcessId target_process,
                          CefRefPtr<CefProcessMessage> message) override;

  bool is_temporary() const { return frame_id_ == kInvalidFrameId; }

  void SetFocused(bool focused);
  void RefreshAttributes();

  // Notification that a move or resize of the renderer's containing window has
  // started. Used on Windows and Linux with the Alloy runtime.
  void NotifyMoveOrResizeStarted();

  // Load the specified request.
  void LoadRequest(cef::mojom::RequestParamsPtr params);

  // Load the specified URL.
  void LoadURLWithExtras(const std::string& url,
                         const content::Referrer& referrer,
                         ui::PageTransition transition,
                         const std::string& extra_headers);

  // Send a command to the renderer for execution.
  void SendCommand(const std::string& command);
  void SendCommandWithResponse(
      const std::string& command,
      cef::mojom::RenderFrame::SendCommandWithResponseCallback
          response_callback);

  // Send JavaScript to the renderer for execution.
  void SendJavaScript(const std::u16string& jsCode,
                      const std::string& scriptUrl,
                      int startLine);

  // Called from CefBrowserHostBase::DidStopLoading.
  void MaybeSendDidStopLoading();

  void ExecuteJavaScriptWithUserGestureForTests(const CefString& javascript);

  // Returns the RFH associated with this frame. Must be called on the UI
  // thread.
  content::RenderFrameHost* GetRenderFrameHost() const;

  // Owned frame objects will be detached explicitly when the associated
  // RenderFrame is deleted. Temporary frame objects will be detached
  // implicitly via CefBrowserInfo::browser() returning nullptr. Returns true
  // if this was the first call to Detach() for the frame.
  bool Detach();

  // A frame has swapped to active status from prerendering or the back-forward
  // cache. We may need to re-attach if the RFH has changed. See
  // https://crbug.com/1179502#c8 for additional background.
  void MaybeReAttach(scoped_refptr<CefBrowserInfo> browser_info,
                     content::RenderFrameHost* render_frame_host);

  // cef::mojom::BrowserFrame methods forwarded from CefBrowserFrame.
  void SendMessage(const std::string& name, base::Value arguments) override;
  void FrameAttached(mojo::PendingRemote<cef::mojom::RenderFrame> render_frame,
                     bool reattached) override;
  void DidFinishFrameLoad(const GURL& validated_url,
                          int32_t http_status_code) override;
  void UpdateDraggableRegions(
      absl::optional<std::vector<cef::mojom::DraggableRegionEntryPtr>> regions)
      override;

  static const int64_t kMainFrameId;
  static const int64_t kFocusedFrameId;
  static const int64_t kUnspecifiedFrameId;
  static const int64_t kInvalidFrameId;

  // PageTransition type for explicit navigations. This must pass the check in
  // ContentBrowserClient::IsExplicitNavigation for debug URLs (HandleDebugURL)
  // to work as expected.
  static const ui::PageTransition kPageTransitionExplicit;

 private:
  int64 GetFrameId() const;
  scoped_refptr<CefBrowserInfo> GetBrowserInfo() const;
  CefRefPtr<CefBrowserHostBase> GetBrowserHostBase() const;

  // Send an action to the remote RenderFrame. This will queue the action if the
  // remote frame is not yet attached.
  using RenderFrameType = mojo::Remote<cef::mojom::RenderFrame>;
  using RenderFrameAction = base::OnceCallback<void(const RenderFrameType&)>;
  void SendToRenderFrame(const std::string& function_name,
                         RenderFrameAction action);

  void OnRenderFrameDisconnect();

  const bool is_main_frame_;

  // The following members may be read/modified from any thread. All access must
  // be protected by |state_lock_|.
  mutable base::Lock state_lock_;
  int64 frame_id_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  bool is_focused_;
  CefString url_;
  CefString name_;
  int64 parent_frame_id_;

  // The following members are only accessed on the UI thread.
  content::RenderFrameHost* render_frame_host_ = nullptr;

  std::queue<std::pair<std::string, RenderFrameAction>>
      queued_renderer_actions_;

  mojo::Remote<cef::mojom::RenderFrame> render_frame_;

  IMPLEMENT_REFCOUNTING(CefFrameHostImpl);
};

#endif  // CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
