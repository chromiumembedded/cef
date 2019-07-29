// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/frame_host_impl.h"

#include "include/cef_request.h"
#include "include/cef_stream.h"
#include "include/cef_v8.h"
#include "include/test/cef_test_helpers.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/navigate_params.h"
#include "libcef/browser/net_service/browser_urlrequest_impl.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/task_runner_impl.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

namespace {

// Implementation of CommandResponseHandler for calling a CefStringVisitor.
class StringVisitHandler : public CefResponseManager::Handler {
 public:
  explicit StringVisitHandler(CefRefPtr<CefStringVisitor> visitor)
      : visitor_(visitor) {}
  void OnResponse(const Cef_Response_Params& params) override {
    visitor_->Visit(params.response);
  }

 private:
  CefRefPtr<CefStringVisitor> visitor_;

  IMPLEMENT_REFCOUNTING(StringVisitHandler);
};

// Implementation of CommandResponseHandler for calling ViewText().
class ViewTextHandler : public CefResponseManager::Handler {
 public:
  explicit ViewTextHandler(CefRefPtr<CefFrameHostImpl> frame) : frame_(frame) {}
  void OnResponse(const Cef_Response_Params& params) override {
    CefRefPtr<CefBrowser> browser = frame_->GetBrowser();
    if (browser.get()) {
      static_cast<CefBrowserHostImpl*>(browser.get())
          ->ViewText(params.response);
    }
  }

 private:
  CefRefPtr<CefFrameHostImpl> frame_;

  IMPLEMENT_REFCOUNTING(ViewTextHandler);
};

}  // namespace

CefFrameHostImpl::CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                                   bool is_main_frame,
                                   int64_t parent_frame_id)
    : is_main_frame_(is_main_frame),
      frame_id_(kInvalidFrameId),
      browser_info_(browser_info),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      parent_frame_id_(parent_frame_id) {
#if DCHECK_IS_ON()
  DCHECK(browser_info_);
  if (is_main_frame_) {
    DCHECK_EQ(parent_frame_id_, kInvalidFrameId);
  } else {
    DCHECK_GT(parent_frame_id_, 0);
  }
#endif
}

CefFrameHostImpl::CefFrameHostImpl(scoped_refptr<CefBrowserInfo> browser_info,
                                   content::RenderFrameHost* render_frame_host)
    : is_main_frame_(render_frame_host->GetParent() == nullptr),
      frame_id_(MakeFrameId(render_frame_host)),
      browser_info_(browser_info),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      url_(render_frame_host->GetLastCommittedURL().spec()),
      name_(render_frame_host->GetFrameName()),
      parent_frame_id_(is_main_frame_
                           ? kInvalidFrameId
                           : MakeFrameId(render_frame_host->GetParent())),
      render_frame_host_(render_frame_host),
      response_manager_(new CefResponseManager) {
  DCHECK(browser_info_);
}

CefFrameHostImpl::~CefFrameHostImpl() {}

void CefFrameHostImpl::SetRenderFrameHost(content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();

  base::AutoLock lock_scope(state_lock_);

  // We should not be detached.
  CHECK(browser_info_);
  // We should be the main frame.
  CHECK(is_main_frame_);

  render_frame_host_ = host;
  frame_id_ = MakeFrameId(host);
  url_ = host->GetLastCommittedURL().spec();
  name_ = host->GetFrameName();

  // Cancel any existing messages.
  response_manager_.reset(new CefResponseManager);
}

bool CefFrameHostImpl::IsValid() {
  return !!GetBrowserHostImpl();
}

void CefFrameHostImpl::Undo() {
  SendCommand("Undo", nullptr);
}

void CefFrameHostImpl::Redo() {
  SendCommand("Redo", nullptr);
}

void CefFrameHostImpl::Cut() {
  SendCommand("Cut", nullptr);
}

void CefFrameHostImpl::Copy() {
  SendCommand("Copy", nullptr);
}

void CefFrameHostImpl::Paste() {
  SendCommand("Paste", nullptr);
}

void CefFrameHostImpl::Delete() {
  SendCommand("Delete", nullptr);
}

void CefFrameHostImpl::SelectAll() {
  SendCommand("SelectAll", nullptr);
}

void CefFrameHostImpl::ViewSource() {
  SendCommand("GetSource", new ViewTextHandler(this));
}

void CefFrameHostImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  SendCommand("GetSource", new StringVisitHandler(visitor));
}

void CefFrameHostImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  SendCommand("GetText", new StringVisitHandler(visitor));
}

void CefFrameHostImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  CefNavigateParams params(GURL(), ui::PAGE_TRANSITION_TYPED);
  static_cast<CefRequestImpl*>(request.get())->Get(params);
  Navigate(params);
}

void CefFrameHostImpl::LoadURL(const CefString& url) {
  LoadURLWithExtras(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                    std::string());
}

void CefFrameHostImpl::LoadString(const CefString& string,
                                  const CefString& url) {
  // Only known frame ids or kMainFrameId are supported.
  const auto frame_id = GetFrameId();
  if (frame_id < CefFrameHostImpl::kMainFrameId)
    return;

  Cef_Request_Params params;
  params.name = "load-string";
  params.user_initiated = false;
  params.request_id = -1;
  params.expect_response = false;

  params.arguments.AppendString(string);
  params.arguments.AppendString(url);

  Send(new CefMsg_Request(MSG_ROUTING_NONE, params));
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

int64 CefFrameHostImpl::GetIdentifier() {
  base::AutoLock lock_scope(state_lock_);
  return frame_id_;
}

CefRefPtr<CefFrame> CefFrameHostImpl::GetParent() {
  int64 parent_frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    if (is_main_frame_ || parent_frame_id_ == kInvalidFrameId)
      return nullptr;
    parent_frame_id = parent_frame_id_;
  }

  auto browser = GetBrowserHostImpl();
  if (browser)
    return browser->GetFrame(parent_frame_id);

  return nullptr;
}

CefString CefFrameHostImpl::GetURL() {
  base::AutoLock lock_scope(state_lock_);
  return url_;
}

CefRefPtr<CefBrowser> CefFrameHostImpl::GetBrowser() {
  return GetBrowserHostImpl().get();
}

CefRefPtr<CefV8Context> CefFrameHostImpl::GetV8Context() {
  NOTREACHED() << "GetV8Context cannot be called from the browser process";
  return nullptr;
}

void CefFrameHostImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  NOTREACHED() << "VisitDOM cannot be called from the browser process";
}

CefRefPtr<CefURLRequest> CefFrameHostImpl::CreateURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  if (!request || !client)
    return nullptr;

  if (!CefTaskRunnerImpl::GetCurrentTaskRunner()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  auto browser = GetBrowserHostImpl();
  if (!browser)
    return nullptr;

  auto request_context = browser->request_context();

  CefRefPtr<CefBrowserURLRequest> impl =
      new CefBrowserURLRequest(this, request, client, request_context);
  if (impl->Start())
    return impl.get();
  return nullptr;
}

void CefFrameHostImpl::SendProcessMessage(
    CefProcessId target_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK_EQ(PID_RENDERER, target_process);
  DCHECK(message.get());

  Cef_Request_Params params;
  CefProcessMessageImpl* impl =
      static_cast<CefProcessMessageImpl*>(message.get());
  if (!impl->CopyTo(params))
    return;

  DCHECK(!params.name.empty());

  params.user_initiated = true;
  params.request_id = -1;
  params.expect_response = false;

  Send(new CefMsg_Request(MSG_ROUTING_NONE, params));
}

void CefFrameHostImpl::SetFocused(bool focused) {
  base::AutoLock lock_scope(state_lock_);
  is_focused_ = focused;
}

void CefFrameHostImpl::RefreshAttributes() {
  CEF_REQUIRE_UIT();

  base::AutoLock lock_scope(state_lock_);
  if (!render_frame_host_)
    return;
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

  if (!is_main_frame_)
    parent_frame_id_ = MakeFrameId(render_frame_host_->GetParent());
}

void CefFrameHostImpl::Navigate(const CefNavigateParams& params) {
  CefMsg_LoadRequest_Params request;
  request.url = params.url;
  if (!request.url.is_valid()) {
    LOG(ERROR) << "Invalid URL passed to CefFrameHostImpl::Navigate: "
               << params.url;
    return;
  }

  request.method = params.method;
  request.referrer = params.referrer.url;
  request.referrer_policy =
      CefRequestImpl::BlinkReferrerPolicyToNetReferrerPolicy(
          params.referrer.policy);
  request.site_for_cookies = params.site_for_cookies;
  request.headers = params.headers;
  request.load_flags = params.load_flags;
  request.upload_data = params.upload_data;

  Send(new CefMsg_LoadRequest(MSG_ROUTING_NONE, request));

  auto browser = GetBrowserHostImpl();
  if (browser)
    browser->OnSetFocus(FOCUS_SOURCE_NAVIGATION);
}

void CefFrameHostImpl::LoadURLWithExtras(const std::string& url,
                                         const content::Referrer& referrer,
                                         ui::PageTransition transition,
                                         const std::string& extra_headers) {
  // Only known frame ids or kMainFrameId are supported.
  const auto frame_id = GetFrameId();
  if (frame_id < CefFrameHostImpl::kMainFrameId)
    return;

  if (frame_id == CefFrameHostImpl::kMainFrameId) {
    // Load via the browser using NavigationController.
    auto browser = GetBrowserHostImpl();
    if (browser) {
      browser->LoadMainFrameURL(url, referrer, transition, extra_headers);
    }
  } else {
    CefNavigateParams params(GURL(url), transition);
    params.referrer = referrer;
    params.headers = extra_headers;
    Navigate(params);
  }
}

void CefFrameHostImpl::SendCommand(
    const std::string& command,
    CefRefPtr<CefResponseManager::Handler> responseHandler) {
  DCHECK(!command.empty());

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefFrameHostImpl::SendCommand, this,
                                          command, responseHandler));
    return;
  }

  // Only known frame ids or kMainFrameId are supported.
  const auto frame_id = GetFrameId();
  if (frame_id < CefFrameHostImpl::kMainFrameId)
    return;

  TRACE_EVENT2("cef", "CefFrameHostImpl::SendCommand", "frame_id", frame_id,
               "needsResponse", responseHandler.get() ? 1 : 0);
  Cef_Request_Params params;
  params.name = "execute-command";
  params.user_initiated = false;

  if (responseHandler.get()) {
    params.request_id = response_manager_->RegisterHandler(responseHandler);
    params.expect_response = true;
  } else {
    params.request_id = -1;
    params.expect_response = false;
  }

  params.arguments.AppendString(command);

  Send(new CefMsg_Request(MSG_ROUTING_NONE, params));
}

void CefFrameHostImpl::SendCode(
    bool is_javascript,
    const std::string& code,
    const std::string& script_url,
    int script_start_line,
    CefRefPtr<CefResponseManager::Handler> responseHandler) {
  DCHECK(!code.empty());
  DCHECK_GE(script_start_line, 0);

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefFrameHostImpl::SendCode, this,
                                          is_javascript, code, script_url,
                                          script_start_line, responseHandler));
    return;
  }

  // Only known frame ids or kMainFrameId are supported.
  auto frame_id = GetFrameId();
  if (frame_id < CefFrameHostImpl::kMainFrameId)
    return;

  TRACE_EVENT2("cef", "CefFrameHostImpl::SendCommand", "frame_id", frame_id,
               "needsResponse", responseHandler.get() ? 1 : 0);
  Cef_Request_Params params;
  params.name = "execute-code";
  params.user_initiated = false;

  if (responseHandler.get()) {
    params.request_id = response_manager_->RegisterHandler(responseHandler);
    params.expect_response = true;
  } else {
    params.request_id = -1;
    params.expect_response = false;
  }

  params.arguments.AppendBoolean(is_javascript);
  params.arguments.AppendString(code);
  params.arguments.AppendString(script_url);
  params.arguments.AppendInteger(script_start_line);

  Send(new CefMsg_Request(MSG_ROUTING_NONE, params));
}

void CefFrameHostImpl::SendJavaScript(const std::string& jsCode,
                                      const std::string& scriptUrl,
                                      int startLine) {
  if (jsCode.empty())
    return;
  if (startLine <= 0) {
    // A value of 0 is v8::Message::kNoLineNumberInfo in V8. There is code in
    // V8 that will assert on that value (e.g. V8StackTraceImpl::Frame::Frame
    // if a JS exception is thrown) so make sure |startLine| > 0.
    startLine = 1;
  }

  SendCode(true, jsCode, scriptUrl, startLine, nullptr);
}

void CefFrameHostImpl::MaybeSendDidStopLoading() {
  auto rfh = GetRenderFrameHost();
  if (!rfh)
    return;

  // We only want to notify for the highest-level LocalFrame in this frame's
  // renderer process subtree. If this frame has a parent in the same process
  // then the notification will be sent via the parent instead.
  auto rfh_parent = rfh->GetParent();
  if (rfh_parent && rfh_parent->GetProcess() == rfh->GetProcess()) {
    return;
  }

  Send(new CefMsg_DidStopLoading(MSG_ROUTING_NONE));
}

bool CefFrameHostImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefFrameHostImpl, message)
    IPC_MESSAGE_HANDLER(CefHostMsg_FrameAttached, OnAttached)
    IPC_MESSAGE_HANDLER(CefHostMsg_FrameFocused, OnFocused)
    IPC_MESSAGE_HANDLER(CefHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(CefHostMsg_UpdateDraggableRegions,
                        OnUpdateDraggableRegions)
    IPC_MESSAGE_HANDLER(CefHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(CefHostMsg_Response, OnResponse)
    IPC_MESSAGE_HANDLER(CefHostMsg_ResponseAck, OnResponseAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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
  if (rfh)
    rfh->ExecuteJavaScriptWithUserGestureForTests(javascript);
}

content::RenderFrameHost* CefFrameHostImpl::GetRenderFrameHost() const {
  CEF_REQUIRE_UIT();
  return render_frame_host_;
}

void CefFrameHostImpl::Detach() {
  CEF_REQUIRE_UIT();

  {
    base::AutoLock lock_scope(state_lock_);
    browser_info_ = nullptr;
  }

  // In case we never attached, clean up.
  while (!queued_messages_.empty()) {
    queued_messages_.pop();
  }

  response_manager_.reset();
  render_frame_host_ = nullptr;
}

// static
int64_t CefFrameHostImpl::MakeFrameId(const content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();
  auto host_nonconst = const_cast<content::RenderFrameHost*>(host);
  return MakeFrameId(host_nonconst->GetProcess()->GetID(),
                     host_nonconst->GetRoutingID());
}

// static
int64_t CefFrameHostImpl::MakeFrameId(int32_t render_process_id,
                                      int32_t render_routing_id) {
  return frame_util::MakeFrameId(render_process_id, render_routing_id);
}

// kMainFrameId must be -1 to align with renderer expectations.
const int64_t CefFrameHostImpl::kMainFrameId = -1;
const int64_t CefFrameHostImpl::kFocusedFrameId = -2;
const int64_t CefFrameHostImpl::kUnspecifiedFrameId = -3;
const int64_t CefFrameHostImpl::kInvalidFrameId = -4;

int64 CefFrameHostImpl::GetFrameId() const {
  base::AutoLock lock_scope(state_lock_);
  return is_main_frame_ ? kMainFrameId : frame_id_;
}

CefRefPtr<CefBrowserHostImpl> CefFrameHostImpl::GetBrowserHostImpl() const {
  base::AutoLock lock_scope(state_lock_);
  if (browser_info_)
    return browser_info_->browser();
  return nullptr;
}

void CefFrameHostImpl::OnAttached() {
  if (!is_attached_) {
    is_attached_ = true;
    while (!queued_messages_.empty()) {
      Send(queued_messages_.front().release());
      queued_messages_.pop();
    }
  }
}

void CefFrameHostImpl::OnFocused() {
  if (!IsFocused()) {
    // Calls back to SetFocused(true) after updating state on the previously
    // focused frame.
    auto browser = GetBrowserHostImpl();
    if (browser)
      browser->OnFrameFocused(this);
  }
}

void CefFrameHostImpl::OnDidFinishLoad(const GURL& validated_url,
                                       int http_status_code) {
  auto browser = GetBrowserHostImpl();
  if (browser)
    browser->OnDidFinishLoad(this, validated_url, http_status_code);
}

void CefFrameHostImpl::OnUpdateDraggableRegions(
    const std::vector<Cef_DraggableRegion_Params>& regions) {
  auto browser = GetBrowserHostImpl();
  if (!browser)
    return;

  CefRefPtr<CefDragHandler> handler;
  auto client = browser->GetClient();
  if (client)
    handler = client->GetDragHandler();
  if (!handler)
    return;

  std::vector<CefDraggableRegion> draggable_regions;
  draggable_regions.reserve(regions.size());

  std::vector<Cef_DraggableRegion_Params>::const_iterator it = regions.begin();
  for (; it != regions.end(); ++it) {
    const gfx::Rect& rect(it->bounds);
    const CefRect bounds(rect.x(), rect.y(), rect.width(), rect.height());
    draggable_regions.push_back(CefDraggableRegion(bounds, it->draggable));
  }

  handler->OnDraggableRegionsChanged(browser.get(), this, draggable_regions);
}

void CefFrameHostImpl::OnRequest(const Cef_Request_Params& params) {
  CEF_REQUIRE_UIT();

  bool success = false;
  std::string response;
  bool expect_response_ack = false;

  if (params.user_initiated) {
    auto browser = GetBrowserHostImpl();
    if (browser && browser->client()) {
      // Give the user a chance to handle the request.
      CefRefPtr<CefProcessMessageImpl> message(new CefProcessMessageImpl(
          const_cast<Cef_Request_Params*>(&params), false, true));
      success = browser->client()->OnProcessMessageReceived(
          browser.get(), this, PID_RENDERER, message.get());
      message->Detach(nullptr);
    }
  } else {
    // Invalid request.
    NOTREACHED();
  }

  if (params.expect_response) {
    DCHECK_GE(params.request_id, 0);

    // Send a response to the renderer.
    Cef_Response_Params response_params;
    response_params.request_id = params.request_id;
    response_params.success = success;
    response_params.response = response;
    response_params.expect_response_ack = expect_response_ack;
    Send(new CefMsg_Response(MSG_ROUTING_NONE, response_params));
  }
}

void CefFrameHostImpl::OnResponse(const Cef_Response_Params& params) {
  CEF_REQUIRE_UIT();

  response_manager_->RunHandler(params);
  if (params.expect_response_ack)
    Send(new CefMsg_ResponseAck(MSG_ROUTING_NONE, params.request_id));
}

void CefFrameHostImpl::OnResponseAck(int request_id) {
  response_manager_->RunAckHandler(request_id);
}

void CefFrameHostImpl::Send(IPC::Message* message) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(base::IgnoreResult(&CefFrameHostImpl::Send),
                                 this, message));
    return;
  }

  if (!render_frame_host_) {
    // Either we're a placeholder frame without a renderer representation, or
    // we've been detached.
    delete message;
    return;
  }

  if (!is_attached_) {
    // Queue messages until we're notified by the renderer that it's ready to
    // handle them.
    queued_messages_.push(base::WrapUnique(message));
    return;
  }

  message->set_routing_id(render_frame_host_->GetRoutingID());
  render_frame_host_->Send(message);
}

void CefExecuteJavaScriptWithUserGestureForTests(CefRefPtr<CefFrame> frame,
                                                 const CefString& javascript) {
  CefFrameHostImpl* impl = static_cast<CefFrameHostImpl*>(frame.get());
  if (impl)
    impl->ExecuteJavaScriptWithUserGestureForTests(javascript);
}
