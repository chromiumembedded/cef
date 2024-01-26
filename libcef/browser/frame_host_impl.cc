// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/frame_host_impl.h"

#include "include/cef_request.h"
#include "include/cef_stream.h"
#include "include/cef_v8.h"
#include "include/test/cef_test_helpers.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/net_service/browser_urlrequest_impl.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/url_util.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/process_message_smr_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/string_util.h"
#include "libcef/common/task_runner_impl.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

void StringVisitCallback(CefRefPtr<CefStringVisitor> visitor,
                         base::ReadOnlySharedMemoryRegion response) {
  string_util::ExecuteWithScopedCefString(
      std::move(response),
      base::BindOnce([](CefRefPtr<CefStringVisitor> visitor,
                        const CefString& str) { visitor->Visit(str); },
                     visitor));
}

void ViewTextCallback(CefRefPtr<CefFrameHostImpl> frame,
                      base::ReadOnlySharedMemoryRegion response) {
  if (auto browser = frame->GetBrowser()) {
    string_util::ExecuteWithScopedCefString(
        std::move(response),
        base::BindOnce(
            [](CefRefPtr<CefBrowser> browser, const CefString& str) {
              static_cast<CefBrowserHostBase*>(browser.get())->ViewText(str);
            },
            browser));
  }
}

using CefFrameHostImplCommand = void (CefFrameHostImpl::*)();
using WebContentsCommand = void (content::WebContents::*)();

void ExecWebContentsCommand(CefFrameHostImpl* fh,
                            CefFrameHostImplCommand fh_func,
                            WebContentsCommand wc_func,
                            const std::string& command) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(fh_func, fh));
    return;
  }
  auto rfh = fh->GetRenderFrameHost();
  if (rfh) {
    auto web_contents = content::WebContents::FromRenderFrameHost(rfh);
    if (web_contents) {
      std::invoke(wc_func, web_contents);
      return;
    }
  }
  fh->SendCommand(command);
}

#define EXEC_WEBCONTENTS_COMMAND(name)                  \
  ExecWebContentsCommand(this, &CefFrameHostImpl::name, \
                         &content::WebContents::name, #name);

}  // namespace

CefFrameHostImpl::CefFrameHostImpl(
    scoped_refptr<CefBrowserInfo> browser_info,
    std::optional<content::GlobalRenderFrameHostToken> parent_frame_token)
    : is_main_frame_(false),
      browser_info_(browser_info),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      parent_frame_token_(std::move(parent_frame_token)) {
#if DCHECK_IS_ON()
  DCHECK(browser_info_);
  DCHECK_EQ(is_main_frame_, !parent_frame_token_.has_value());
#endif
}

CefFrameHostImpl::CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                                   content::RenderFrameHost* render_frame_host)
    : is_main_frame_(render_frame_host->GetParent() == nullptr),
      frame_token_(render_frame_host->GetGlobalFrameToken()),
      browser_info_(browser_info),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      url_(render_frame_host->GetLastCommittedURL().spec()),
      name_(render_frame_host->GetFrameName()),
      parent_frame_token_(
          is_main_frame_
              ? std::optional<content::GlobalRenderFrameHostToken>()
              : render_frame_host->GetParent()->GetGlobalFrameToken()),
      render_frame_host_(render_frame_host) {
  DCHECK(browser_info_);
}

CefFrameHostImpl::~CefFrameHostImpl() {
  // Should have been Detached if not temporary.
  DCHECK(is_temporary() || !browser_info_);
  DCHECK(!render_frame_host_);
}

bool CefFrameHostImpl::IsValid() {
  return !!GetBrowserHostBase();
}

void CefFrameHostImpl::Undo() {
  EXEC_WEBCONTENTS_COMMAND(Undo);
}

void CefFrameHostImpl::Redo() {
  EXEC_WEBCONTENTS_COMMAND(Redo);
}

void CefFrameHostImpl::Cut() {
  EXEC_WEBCONTENTS_COMMAND(Cut);
}

void CefFrameHostImpl::Copy() {
  EXEC_WEBCONTENTS_COMMAND(Copy);
}

void CefFrameHostImpl::Paste() {
  EXEC_WEBCONTENTS_COMMAND(Paste);
}

void CefFrameHostImpl::Delete() {
  EXEC_WEBCONTENTS_COMMAND(Delete);
}

void CefFrameHostImpl::SelectAll() {
  EXEC_WEBCONTENTS_COMMAND(SelectAll);
}

void CefFrameHostImpl::ViewSource() {
  SendCommandWithResponse(
      "GetSource",
      base::BindOnce(&ViewTextCallback, CefRefPtr<CefFrameHostImpl>(this)));
}

void CefFrameHostImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  SendCommandWithResponse("GetSource",
                          base::BindOnce(&StringVisitCallback, visitor));
}

void CefFrameHostImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  SendCommandWithResponse("GetText",
                          base::BindOnce(&StringVisitCallback, visitor));
}

void CefFrameHostImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  auto params = cef::mojom::RequestParams::New();
  static_cast<CefRequestImpl*>(request.get())->Get(params);
  LoadRequest(std::move(params));
}

void CefFrameHostImpl::LoadURL(const CefString& url) {
  LoadURLWithExtras(url, content::Referrer(), kPageTransitionExplicit,
                    std::string());
}

void CefFrameHostImpl::ExecuteJavaScript(const CefString& jsCode,
                                         const CefString& scriptUrl,
                                         int startLine) {
  SendJavaScript(jsCode, scriptUrl, startLine);
}

bool CefFrameHostImpl::IsMain() {
  return is_main_frame_;
}

bool CefFrameHostImpl::IsFocused() {
  base::AutoLock lock_scope(state_lock_);
  return is_focused_;
}

CefString CefFrameHostImpl::GetName() {
  base::AutoLock lock_scope(state_lock_);
  return name_;
}

CefString CefFrameHostImpl::GetIdentifier() {
  if (!frame_token_) {
    return CefString();
  }
  return frame_util::MakeFrameIdentifier(*frame_token_);
}

CefRefPtr<CefFrame> CefFrameHostImpl::GetParent() {
  if (is_main_frame_) {
    return nullptr;
  }

  content::GlobalRenderFrameHostToken parent_frame_token;

  {
    base::AutoLock lock_scope(state_lock_);
    if (!parent_frame_token_) {
      return nullptr;
    }
    parent_frame_token = *parent_frame_token_;
  }

  if (auto browser = GetBrowserHostBase()) {
    return browser->GetFrameForGlobalToken(parent_frame_token);
  }

  return nullptr;
}

CefString CefFrameHostImpl::GetURL() {
  base::AutoLock lock_scope(state_lock_);
  return url_;
}

CefRefPtr<CefBrowser> CefFrameHostImpl::GetBrowser() {
  return GetBrowserHostBase().get();
}

CefRefPtr<CefV8Context> CefFrameHostImpl::GetV8Context() {
  DCHECK(false) << "GetV8Context cannot be called from the browser process";
  return nullptr;
}

void CefFrameHostImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  DCHECK(false) << "VisitDOM cannot be called from the browser process";
}

CefRefPtr<CefURLRequest> CefFrameHostImpl::CreateURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  if (!request || !client) {
    return nullptr;
  }

  if (!CefTaskRunnerImpl::GetCurrentTaskRunner()) {
    DCHECK(false) << "called on invalid thread";
    return nullptr;
  }

  auto browser = GetBrowserHostBase();
  if (!browser) {
    return nullptr;
  }

  auto request_context = browser->request_context();

  CefRefPtr<CefBrowserURLRequest> impl =
      new CefBrowserURLRequest(this, request, client, request_context);
  if (impl->Start()) {
    return impl.get();
  }
  return nullptr;
}

void CefFrameHostImpl::SendProcessMessage(
    CefProcessId target_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK_EQ(PID_RENDERER, target_process);
  DCHECK(message && message->IsValid());
  if (!message || !message->IsValid()) {
    return;
  }

  if (message->GetArgumentList() != nullptr) {
    // Invalidate the message object immediately by taking the argument list.
    auto argument_list =
        static_cast<CefProcessMessageImpl*>(message.get())->TakeArgumentList();
    SendToRenderFrame(
        __FUNCTION__,
        base::BindOnce(
            [](const CefString& name, base::Value::List argument_list,
               const RenderFrameType& render_frame) {
              render_frame->SendMessage(name, std::move(argument_list));
            },
            message->GetName(), std::move(argument_list)));
  } else {
    auto region =
        static_cast<CefProcessMessageSMRImpl*>(message.get())->TakeRegion();
    SendToRenderFrame(
        __FUNCTION__,
        base::BindOnce(
            [](const CefString& name, base::WritableSharedMemoryRegion region,
               const RenderFrameType& render_frame) {
              render_frame->SendSharedMemoryRegion(name, std::move(region));
            },
            message->GetName(), std::move(region)));
  }
}

void CefFrameHostImpl::SetFocused(bool focused) {
  CEF_REQUIRE_UIT();
  base::AutoLock lock_scope(state_lock_);
  is_focused_ = focused;
}

void CefFrameHostImpl::RefreshAttributes() {
  CEF_REQUIRE_UIT();

  base::AutoLock lock_scope(state_lock_);
  if (!render_frame_host_) {
    return;
  }
  url_ = render_frame_host_->GetLastCommittedURL().spec();

  // Use the assigned name if it is non-empty. This represents the name property
  // on the frame DOM element. If the assigned name is empty, revert to the
  // internal unique name. This matches the logic in render_frame_util::GetName.
  name_ = render_frame_host_->GetFrameName();
  if (name_.empty()) {
    const auto node = content::FrameTreeNode::GloballyFindByID(
        render_frame_host_->GetFrameTreeNodeId());
    if (node) {
      name_ = node->unique_name();
    }
  }

  if (!is_main_frame_) {
    parent_frame_token_ =
        render_frame_host_->GetParent()->GetGlobalFrameToken();
  }
}

void CefFrameHostImpl::NotifyMoveOrResizeStarted() {
  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->MoveOrResizeStarted();
                    }));
}

void CefFrameHostImpl::LoadRequest(cef::mojom::RequestParamsPtr params) {
  if (!url_util::FixupGURL(params->url)) {
    return;
  }

  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce(
                        [](cef::mojom::RequestParamsPtr params,
                           const RenderFrameType& render_frame) {
                          render_frame->LoadRequest(std::move(params));
                        },
                        std::move(params)));

  auto browser = GetBrowserHostBase();
  if (browser) {
    browser->OnSetFocus(FOCUS_SOURCE_NAVIGATION);
  }
}

void CefFrameHostImpl::LoadURLWithExtras(const std::string& url,
                                         const content::Referrer& referrer,
                                         ui::PageTransition transition,
                                         const std::string& extra_headers) {
  // Any necessary fixup will occur in LoadRequest.
  GURL gurl = url_util::MakeGURL(url, /*fixup=*/false);

  if (is_main_frame_) {
    // Load via the browser using NavigationController.
    auto browser = GetBrowserHostBase();
    if (browser) {
      content::OpenURLParams params(
          gurl, referrer, WindowOpenDisposition::CURRENT_TAB, transition,
          /*is_renderer_initiated=*/false);
      params.extra_headers = extra_headers;
      params.user_gesture = false;

      browser->LoadMainFrameURL(params);
    }
  } else {
    auto params = cef::mojom::RequestParams::New();
    params->url = gurl;
    params->referrer =
        blink::mojom::Referrer::New(referrer.url, referrer.policy);
    params->headers = extra_headers;
    LoadRequest(std::move(params));
  }
}

void CefFrameHostImpl::SendCommand(const std::string& command) {
  DCHECK(!command.empty());
  SendToRenderFrame(__FUNCTION__, base::BindOnce(
                                      [](const std::string& command,
                                         const RenderFrameType& render_frame) {
                                        render_frame->SendCommand(command);
                                      },
                                      command));
}

void CefFrameHostImpl::SendCommandWithResponse(
    const std::string& command,
    cef::mojom::RenderFrame::SendCommandWithResponseCallback
        response_callback) {
  DCHECK(!command.empty());
  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::string& command,
             cef::mojom::RenderFrame::SendCommandWithResponseCallback
                 response_callback,
             const RenderFrameType& render_frame) {
            render_frame->SendCommandWithResponse(command,
                                                  std::move(response_callback));
          },
          command, std::move(response_callback)));
}

void CefFrameHostImpl::SendJavaScript(const std::u16string& jsCode,
                                      const std::string& scriptUrl,
                                      int startLine) {
  if (jsCode.empty()) {
    return;
  }
  if (startLine <= 0) {
    // A value of 0 is v8::Message::kNoLineNumberInfo in V8. There is code in
    // V8 that will assert on that value (e.g. V8StackTraceImpl::Frame::Frame
    // if a JS exception is thrown) so make sure |startLine| > 0.
    startLine = 1;
  }

  SendToRenderFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::u16string& jsCode, const std::string& scriptUrl,
             int startLine, const RenderFrameType& render_frame) {
            render_frame->SendJavaScript(jsCode, scriptUrl, startLine);
          },
          jsCode, scriptUrl, startLine));
}

void CefFrameHostImpl::MaybeSendDidStopLoading() {
  auto rfh = GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  // We only want to notify for the highest-level LocalFrame in this frame's
  // renderer process subtree. If this frame has a parent in the same process
  // then the notification will be sent via the parent instead.
  auto rfh_parent = rfh->GetParent();
  if (rfh_parent && rfh_parent->GetProcess() == rfh->GetProcess()) {
    return;
  }

  SendToRenderFrame(__FUNCTION__,
                    base::BindOnce([](const RenderFrameType& render_frame) {
                      render_frame->DidStopLoading();
                    }));
}

void CefFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const CefString& javascript) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &CefFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests, this,
            javascript));
    return;
  }

  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (rfh) {
    rfh->ExecuteJavaScriptWithUserGestureForTests(javascript,
                                                  base::NullCallback());
  }
}

content::RenderFrameHost* CefFrameHostImpl::GetRenderFrameHost() const {
  CEF_REQUIRE_UIT();
  return render_frame_host_;
}

bool CefFrameHostImpl::IsSameFrame(content::RenderFrameHost* frame_host) const {
  CEF_REQUIRE_UIT();
  // Shortcut in case the RFH objects match.
  if (render_frame_host_ == frame_host) {
    return true;
  }

  // Frame tokens should match even if we're currently detached or the RFH
  // object has changed.
  return frame_token_ && *frame_token_ == frame_host->GetGlobalFrameToken();
}

bool CefFrameHostImpl::IsDetached() const {
  return !GetRenderFrameHost();
}

bool CefFrameHostImpl::Detach(DetachReason reason) {
  CEF_REQUIRE_UIT();

  if (VLOG_IS_ON(1)) {
    std::string reason_str;
    switch (reason) {
      case DetachReason::RENDER_FRAME_DELETED:
        reason_str = "RENDER_FRAME_DELETED";
        break;
      case DetachReason::NEW_MAIN_FRAME:
        reason_str = "NEW_MAIN_FRAME";
        break;
      case DetachReason::BROWSER_DESTROYED:
        reason_str = "BROWSER_DESTROYED";
        break;
    };

    VLOG(1) << GetDebugString() << " detached (reason=" << reason_str
            << ", is_connected=" << render_frame_.is_bound() << ")";
  }

  // May be called multiple times (e.g. from CefBrowserInfo SetMainFrame and
  // RemoveFrame).
  bool first_detach = false;

  // Should not be called for temporary frames.
  DCHECK(!is_temporary());

  {
    base::AutoLock lock_scope(state_lock_);
    if (browser_info_) {
      first_detach = true;
      browser_info_ = nullptr;
    }
  }

  // In case we never attached, clean up.
  while (!queued_renderer_actions_.empty()) {
    queued_renderer_actions_.pop();
  }

  if (render_frame_.is_bound()) {
    render_frame_->FrameDetached();
  }

  render_frame_.reset();
  render_frame_host_ = nullptr;

  return first_detach;
}

void CefFrameHostImpl::MaybeReAttach(
    scoped_refptr<CefBrowserInfo> browser_info,
    content::RenderFrameHost* render_frame_host) {
  CEF_REQUIRE_UIT();
  if (render_frame_.is_bound() && render_frame_host_ == render_frame_host) {
    // Nothing to do here.
    return;
  }

  // We expect that Detach() was called previously.
  CHECK(!is_temporary());
  CHECK(!render_frame_.is_bound());
  CHECK(!render_frame_host_);

  // The RFH may change but the frame token should remain the same.
  CHECK(*frame_token_ == render_frame_host->GetGlobalFrameToken());

  {
    base::AutoLock lock_scope(state_lock_);
    browser_info_ = browser_info;
  }

  render_frame_host_ = render_frame_host;
  RefreshAttributes();

  // We expect a reconnect to be triggered via FrameAttached().
}

// This equates to (TT_EXPLICIT | TT_DIRECT_LOAD_FLAG).
const ui::PageTransition CefFrameHostImpl::kPageTransitionExplicit =
    static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

scoped_refptr<CefBrowserInfo> CefFrameHostImpl::GetBrowserInfo() const {
  base::AutoLock lock_scope(state_lock_);
  return browser_info_;
}

CefRefPtr<CefBrowserHostBase> CefFrameHostImpl::GetBrowserHostBase() const {
  if (auto browser_info = GetBrowserInfo()) {
    return browser_info->browser();
  }
  return nullptr;
}

void CefFrameHostImpl::SendToRenderFrame(const std::string& function_name,
                                         RenderFrameAction action) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefFrameHostImpl::SendToRenderFrame, this,
                                 function_name, std::move(action)));
    return;
  }

  if (is_temporary()) {
    LOG(WARNING) << function_name
                 << " sent to temporary subframe will be ignored.";
    return;
  } else if (!render_frame_host_) {
    // We've been detached.
    LOG(WARNING) << function_name << " sent to detached " << GetDebugString()
                 << " will be ignored";
    return;
  }

  if (!render_frame_.is_bound()) {
    // Queue actions until we're notified by the renderer that it's ready to
    // handle them.
    queued_renderer_actions_.emplace(function_name, std::move(action));
    return;
  }

  std::move(action).Run(render_frame_);
}

void CefFrameHostImpl::OnRenderFrameDisconnect() {
  CEF_REQUIRE_UIT();

  // Reconnect, if any, will be triggered via FrameAttached().
  render_frame_.reset();
}

void CefFrameHostImpl::SendMessage(const std::string& name,
                                   base::Value::List arguments) {
  if (auto browser = GetBrowserHostBase()) {
    if (auto client = browser->GetClient()) {
      CefRefPtr<CefProcessMessageImpl> message(
          new CefProcessMessageImpl(name, std::move(arguments),
                                    /*read_only=*/true));
      browser->GetClient()->OnProcessMessageReceived(
          browser.get(), this, PID_RENDERER, message.get());
    }
  }
}

void CefFrameHostImpl::SendSharedMemoryRegion(
    const std::string& name,
    base::WritableSharedMemoryRegion region) {
  if (auto browser = GetBrowserHostBase()) {
    if (auto client = browser->GetClient()) {
      CefRefPtr<CefProcessMessage> message(
          new CefProcessMessageSMRImpl(name, std::move(region)));
      browser->GetClient()->OnProcessMessageReceived(browser.get(), this,
                                                     PID_RENDERER, message);
    }
  }
}

void CefFrameHostImpl::FrameAttached(
    mojo::PendingRemote<cef::mojom::RenderFrame> render_frame_remote,
    bool reattached) {
  CEF_REQUIRE_UIT();
  CHECK(render_frame_remote);

  auto browser_info = GetBrowserInfo();
  if (!browser_info) {
    // Already Detached.
    return;
  }

  VLOG(1) << GetDebugString() << " " << (reattached ? "re" : "") << "connected";

  render_frame_.Bind(std::move(render_frame_remote));
  render_frame_.set_disconnect_handler(
      base::BindOnce(&CefFrameHostImpl::OnRenderFrameDisconnect, this));

  // Notify the renderer process that it can start sending messages.
  render_frame_->FrameAttachedAck();

  while (!queued_renderer_actions_.empty()) {
    std::move(queued_renderer_actions_.front().second).Run(render_frame_);
    queued_renderer_actions_.pop();
  }

  browser_info->MaybeExecuteFrameNotification(base::BindOnce(
      [](CefRefPtr<CefFrameHostImpl> self, bool reattached,
         CefRefPtr<CefFrameHandler> handler) {
        if (auto browser = self->GetBrowserHostBase()) {
          handler->OnFrameAttached(browser, self, reattached);
        }
      },
      CefRefPtr<CefFrameHostImpl>(this), reattached));
}

void CefFrameHostImpl::UpdateDraggableRegions(
    absl::optional<std::vector<cef::mojom::DraggableRegionEntryPtr>> regions) {
  auto browser = GetBrowserHostBase();
  if (!browser) {
    return;
  }

  std::vector<CefDraggableRegion> draggable_regions;
  if (regions) {
    draggable_regions.reserve(regions->size());

    for (const auto& region : *regions) {
      const auto& rect = region->bounds;
      const CefRect bounds(rect.x(), rect.y(), rect.width(), rect.height());
      draggable_regions.emplace_back(bounds, region->draggable);
    }
  }

  // Delegate to BrowserInfo so that current state is maintained with
  // cross-origin navigation.
  browser->browser_info()->MaybeNotifyDraggableRegionsChanged(
      browser, this, std::move(draggable_regions));
}

std::string CefFrameHostImpl::GetDebugString() const {
  return "frame " +
         (frame_token_ ? frame_util::GetFrameDebugString(*frame_token_)
                       : "(null)") +
         (is_main_frame_ ? " (main)" : " (sub)");
}

void CefExecuteJavaScriptWithUserGestureForTests(CefRefPtr<CefFrame> frame,
                                                 const CefString& javascript) {
  CefFrameHostImpl* impl = static_cast<CefFrameHostImpl*>(frame.get());
  if (impl) {
    impl->ExecuteJavaScriptWithUserGestureForTests(javascript);
  }
}
