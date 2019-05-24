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
#include "libcef/common/response_manager.h"

#include "base/synchronization/lock.h"
#include "ui/base/page_transition_types.h"

namespace content {
class RenderFrameHost;
struct Referrer;
}  // namespace content

namespace IPC {
class Message;
}

class GURL;

struct Cef_DraggableRegion_Params;
struct Cef_Request_Params;
struct Cef_Response_Params;
class CefBrowserInfo;
class CefBrowserHostImpl;
struct CefNavigateParams;

// Implementation of CefFrame. CefFrameHostImpl objects should always be created
// or retrieved via CefBrowerInfo.
class CefFrameHostImpl : public CefFrame {
 public:
  // Create a temporary frame.
  CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                   bool is_main_frame,
                   int64_t parent_frame_id);

  // Create a frame backed by a RFH and owned by CefBrowserInfo.
  CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                   content::RenderFrameHost* render_frame_host);

  // Update an existing main frame object.
  void SetRenderFrameHost(content::RenderFrameHost* host);

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
  void LoadString(const CefString& string, const CefString& url) override;
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

  void SetFocused(bool focused);
  void RefreshAttributes();

  // Navigate as specified by the |params| argument.
  void Navigate(const CefNavigateParams& params);

  // Load the specified URL.
  void LoadURLWithExtras(const std::string& url,
                         const content::Referrer& referrer,
                         ui::PageTransition transition,
                         const std::string& extra_headers);

  // Send a command to the renderer for execution.
  void SendCommand(const std::string& command,
                   CefRefPtr<CefResponseManager::Handler> responseHandler);

  // Send code to the renderer for execution.
  void SendCode(bool is_javascript,
                const std::string& code,
                const std::string& script_url,
                int script_start_line,
                CefRefPtr<CefResponseManager::Handler> responseHandler);

  // Send JavaScript to the renderer for execution.
  void SendJavaScript(const std::string& jsCode,
                      const std::string& scriptUrl,
                      int startLine);

  // Called from CefBrowserHostImpl::DidStopLoading.
  void MaybeSendDidStopLoading();

  // Called from CefBrowserHostImpl::OnMessageReceived.
  bool OnMessageReceived(const IPC::Message& message);

  void ExecuteJavaScriptWithUserGestureForTests(const CefString& javascript);

  // Returns the RFH associated with this frame. Must be called on the UI
  // thread.
  content::RenderFrameHost* GetRenderFrameHost() const;

  // Owned frame objects will be detached explicitly when the associated
  // RenderFrame is deleted. Temporary frame objects will be detached
  // implicitly via CefBrowserInfo::browser() returning nullptr.
  void Detach();

  static int64_t MakeFrameId(const content::RenderFrameHost* host);
  static int64_t MakeFrameId(int32_t render_process_id,
                             int32_t render_routing_id);

  static const int64_t kMainFrameId;
  static const int64_t kFocusedFrameId;
  static const int64_t kUnspecifiedFrameId;
  static const int64_t kInvalidFrameId;

 private:
  int64 GetFrameId() const;
  CefRefPtr<CefBrowserHostImpl> GetBrowserHostImpl() const;

  // OnMessageReceived message handlers.
  void OnAttached();
  void OnFocused();
  void OnDidFinishLoad(const GURL& validated_url, int http_status_code);
  void OnUpdateDraggableRegions(
      const std::vector<Cef_DraggableRegion_Params>& regions);
  void OnRequest(const Cef_Request_Params& params);
  void OnResponse(const Cef_Response_Params& params);
  void OnResponseAck(int request_id);

  // Send a message to the RenderFrameHost associated with this frame.
  void Send(IPC::Message* message);

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

  bool is_attached_ = false;

  // Qeueud messages to send when the renderer process attaches.
  std::queue<std::unique_ptr<IPC::Message>> queued_messages_;

  // Manages response registrations.
  std::unique_ptr<CefResponseManager> response_manager_;

  IMPLEMENT_REFCOUNTING(CefFrameHostImpl);
  DISALLOW_COPY_AND_ASSIGN(CefFrameHostImpl);
};

#endif  // CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
