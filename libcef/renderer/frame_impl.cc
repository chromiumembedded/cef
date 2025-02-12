// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/renderer/frame_impl.h"

#include "build/build_config.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/include/cef_urlrequest.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/net/http_header_utils.h"
#include "cef/libcef/common/process_message_impl.h"
#include "cef/libcef/common/process_message_smr_impl.h"
#include "cef/libcef/common/request_impl.h"
#include "cef/libcef/common/string_util.h"
#include "cef/libcef/renderer/blink_glue.h"
#include "cef/libcef/renderer/browser_impl.h"
#include "cef/libcef/renderer/dom_document_impl.h"
#include "cef/libcef/renderer/render_frame_util.h"
#include "cef/libcef/renderer/thread_util.h"
#include "cef/libcef/renderer/v8_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace {

// Maximum number of times to retry the browser connection.
constexpr size_t kConnectionRetryMaxCt = 3U;

// Length of time to wait before initiating a browser connection retry. The
// short value is optimized for navigation-related disconnects (time delta
// between CefFrameImpl::OnDisconnect and CefFrameHostImpl::MaybeReAttach) which
// should take << 10ms in normal circumstances (reasonably fast machine, limited
// redirects). The long value is optimized for slower machines or navigations
// with many redirects to reduce overall failure rates. See related comments in
// CefFrameImpl::OnDisconnect.
constexpr auto kConnectionRetryDelayShort = base::Milliseconds(25);
constexpr auto kConnectionRetryDelayLong = base::Seconds(3);

std::string GetDebugString(blink::WebLocalFrame* frame) {
  return "frame " + render_frame_util::GetIdentifier(frame);
}

v8::Isolate* GetFrameIsolate(blink::WebLocalFrame* frame) {
  return frame->GetAgentGroupScheduler()->Isolate();
}

}  // namespace

CefFrameImpl::CefFrameImpl(CefBrowserImpl* browser, blink::WebLocalFrame* frame)
    : browser_(browser),
      frame_(frame),
      frame_debug_str_(GetDebugString(frame)) {}

CefFrameImpl::~CefFrameImpl() = default;

bool CefFrameImpl::IsValid() {
  CEF_REQUIRE_RT_RETURN(false);

  return (frame_ != nullptr);
}

void CefFrameImpl::Undo() {
  SendCommand("Undo");
}

void CefFrameImpl::Redo() {
  SendCommand("Redo");
}

void CefFrameImpl::Cut() {
  SendCommand("Cut");
}

void CefFrameImpl::Copy() {
  SendCommand("Copy");
}

void CefFrameImpl::Paste() {
  SendCommand("Paste");
}

void CefFrameImpl::PasteAndMatchStyle() {
  SendCommand("PasteAndMatchStyle");
}

void CefFrameImpl::Delete() {
  SendCommand("Delete");
}

void CefFrameImpl::SelectAll() {
  SendCommand("SelectAll");
}

void CefFrameImpl::ViewSource() {
  DCHECK(false) << "ViewSource cannot be called from the renderer process";
}

void CefFrameImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_) {
    CefString content;
    string_util::GetCefString(blink_glue::DumpDocumentMarkup(frame_), content);
    visitor->Visit(content);
  }
}

void CefFrameImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_) {
    CefString content;
    string_util::GetCefString(blink_glue::DumpDocumentText(frame_), content);
    visitor->Visit(content);
  }
}

void CefFrameImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_) {
    return;
  }

  auto params = cef::mojom::RequestParams::New();
  static_cast<CefRequestImpl*>(request.get())->Get(params);
  LoadRequest(std::move(params));
}

void CefFrameImpl::LoadURL(const CefString& url) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_) {
    return;
  }

  auto params = cef::mojom::RequestParams::New();
  params->url = GURL(url.ToString());
  params->method = "GET";
  LoadRequest(std::move(params));
}

void CefFrameImpl::ExecuteJavaScript(const CefString& jsCode,
                                     const CefString& scriptUrl,
                                     int startLine) {
  SendJavaScript(jsCode, scriptUrl, startLine);
}

bool CefFrameImpl::IsMain() {
  CEF_REQUIRE_RT_RETURN(false);

  if (frame_) {
    return (frame_->Parent() == nullptr);
  }
  return false;
}

bool CefFrameImpl::IsFocused() {
  CEF_REQUIRE_RT_RETURN(false);

  if (frame_ && frame_->View()) {
    return (frame_->View()->FocusedFrame() == frame_);
  }
  return false;
}

CefString CefFrameImpl::GetName() {
  CefString name;
  CEF_REQUIRE_RT_RETURN(name);

  if (frame_) {
    name = render_frame_util::GetName(frame_);
  }
  return name;
}

CefString CefFrameImpl::GetIdentifier() {
  CefString identifier;
  CEF_REQUIRE_RT_RETURN(identifier);

  if (frame_) {
    identifier = render_frame_util::GetIdentifier(frame_);
  }
  return identifier;
}

CefRefPtr<CefFrame> CefFrameImpl::GetParent() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (frame_) {
    blink::WebFrame* parent = frame_->Parent();
    if (parent && parent->IsWebLocalFrame()) {
      return browser_->GetWebFrameImpl(parent->ToWebLocalFrame()).get();
    }
  }

  return nullptr;
}

CefString CefFrameImpl::GetURL() {
  CefString url;
  CEF_REQUIRE_RT_RETURN(url);

  if (frame_) {
    GURL gurl = frame_->GetDocument().Url();
    url = gurl.spec();
  }
  return url;
}

CefRefPtr<CefBrowser> CefFrameImpl::GetBrowser() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  return browser_;
}

CefRefPtr<CefV8Context> CefFrameImpl::GetV8Context() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (frame_) {
    v8::Isolate* isolate = GetFrameIsolate(frame_);
    v8::HandleScope handle_scope(isolate);
    return new CefV8ContextImpl(isolate, frame_->MainWorldScriptContext());
  } else {
    return nullptr;
  }
}

void CefFrameImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_) {
    return;
  }

  // Create a CefDOMDocumentImpl object that is valid only for the scope of this
  // method.
  CefRefPtr<CefDOMDocumentImpl> documentImpl;
  const blink::WebDocument& document = frame_->GetDocument();
  if (!document.IsNull()) {
    documentImpl = new CefDOMDocumentImpl(browser_, frame_);
  }

  visitor->Visit(documentImpl.get());

  if (documentImpl.get()) {
    documentImpl->Detach();
  }
}

CefRefPtr<CefURLRequest> CefFrameImpl::CreateURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  DCHECK(false) << "CreateURLRequest cannot be called from the render process";
  return nullptr;
}

void CefFrameImpl::SendProcessMessage(CefProcessId target_process,
                                      CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_RT_RETURN_VOID();
  DCHECK_EQ(PID_BROWSER, target_process);
  DCHECK(message && message->IsValid());
  if (!message || !message->IsValid()) {
    return;
  }

  if (message->GetArgumentList() != nullptr) {
    // Invalidate the message object immediately by taking the argument list.
    auto argument_list =
        static_cast<CefProcessMessageImpl*>(message.get())->TakeArgumentList();
    SendToBrowserFrame(
        __FUNCTION__,
        base::BindOnce(
            [](const CefString& name, base::Value::List argument_list,
               const BrowserFrameType& render_frame) {
              render_frame->SendMessage(name, std::move(argument_list));
            },
            message->GetName(), std::move(argument_list)));
  } else {
    auto region =
        static_cast<CefProcessMessageSMRImpl*>(message.get())->TakeRegion();
    SendToBrowserFrame(
        __FUNCTION__,
        base::BindOnce(
            [](const CefString& name, base::WritableSharedMemoryRegion region,
               const BrowserFrameType& render_frame) {
              render_frame->SendSharedMemoryRegion(name, std::move(region));
            },
            message->GetName(), std::move(region)));
  }
}

void CefFrameImpl::OnWasShown() {
  if (browser_connection_state_ == ConnectionState::DISCONNECTED &&
      did_commit_provisional_load_) {
    // Reconnect a frame that has exited the bfcache. We ignore temporary
    // frames that have never called DidCommitProvisionalLoad.
    ConnectBrowserFrame(ConnectReason::WAS_SHOWN);
  }
}

void CefFrameImpl::OnDidCommitProvisionalLoad() {
  did_commit_provisional_load_ = true;
  if (browser_connection_state_ == ConnectionState::DISCONNECTED) {
    // Connect after RenderFrameImpl::DidCommitNavigation has potentially
    // reset the BrowserInterfaceBroker in the browser process. See related
    // comments in OnDisconnect.
    ConnectBrowserFrame(ConnectReason::DID_COMMIT);
  }
  MaybeInitializeScriptContext();
}

void CefFrameImpl::OnDidFinishLoad() {
  // Ignore notifications from the embedded frame hosting a mime-type plugin.
  // We'll eventually receive a notification from the owner frame.
  if (blink_glue::HasPluginFrameOwner(frame_)) {
    return;
  }

  if (!blink::RuntimeEnabledFeatures::BackForwardCacheEnabled() && IsMain()) {
    // Refresh draggable regions. Otherwise, we may not receive updated regions
    // after navigation because LocalFrameView::UpdateDocumentAnnotatedRegion
    // lacks sufficient context. When bfcache is disabled we use this method
    // instead of DidStopLoading() because it provides more accurate timing.
    OnDraggableRegionsChanged();
  }

  blink::WebDocumentLoader* dl = frame_->GetDocumentLoader();
  const int http_status_code = dl->GetWebResponse().HttpStatusCode();

  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app) {
    CefRefPtr<CefRenderProcessHandler> handler = app->GetRenderProcessHandler();
    if (handler) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler) {
        load_handler->OnLoadEnd(browser_, this, http_status_code);
      }
    }
  }
}

void CefFrameImpl::OnDraggableRegionsChanged() {
  // Only the main frame is allowed to control draggable regions, to avoid other
  // frames trying to manipulate the regions in the browser process.
  if (frame_->Parent() != nullptr) {
    return;
  }

  auto webregions = frame_->GetDocument().DraggableRegions();
  std::vector<cef::mojom::DraggableRegionEntryPtr> regions;
  if (!webregions.empty()) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);

    regions.reserve(webregions.size());
    for (const auto& webregion : webregions) {
      auto region = cef::mojom::DraggableRegionEntry::New(
          render_frame->ConvertViewportToWindow(webregion.bounds),
          webregion.draggable);
      regions.emplace_back(std::move(region));
    }
  }

  using RegionsArg =
      std::optional<std::vector<cef::mojom::DraggableRegionEntryPtr>>;
  RegionsArg regions_arg =
      regions.empty() ? std::nullopt : std::make_optional(std::move(regions));

  SendToBrowserFrame(
      __FUNCTION__,
      base::BindOnce(
          [](RegionsArg regions_arg, const BrowserFrameType& browser_frame) {
            browser_frame->UpdateDraggableRegions(std::move(regions_arg));
          },
          std::move(regions_arg)));
}

void CefFrameImpl::OnContextCreated(v8::Local<v8::Context> context) {
  context_created_ = true;

  CHECK(frame_);
  while (!queued_context_actions_.empty()) {
    auto& action = queued_context_actions_.front();
    std::move(action.second).Run(frame_);
    queued_context_actions_.pop();
  }

  execution_context_lifecycle_state_observer_ =
      blink_glue::RegisterExecutionContextLifecycleStateObserver(context, this);
}

void CefFrameImpl::OnContextReleased() {
  execution_context_lifecycle_state_observer_.reset();
}

void CefFrameImpl::OnDetached() {
  // Called when this frame has been detached from the view. This *will* be
  // called for child frames when a parent frame is detached.
  // The browser may hold the last reference to |this|. Take a reference here to
  // keep |this| alive until after this method returns.
  CefRefPtr<CefFrameImpl> self = this;

  browser_->FrameDetached(frame_);
  frame_ = nullptr;

  OnDisconnect(DisconnectReason::DETACHED, 0, std::string(), MOJO_RESULT_OK);

  browser_ = nullptr;

  // In case we never attached.
  while (!queued_browser_actions_.empty()) {
    auto& action = queued_browser_actions_.front();
    LOG(WARNING) << action.first << " sent to detached " << frame_debug_str_
                 << " will be ignored";
    queued_browser_actions_.pop();
  }

  // In case we're destroyed without the context being created.
  while (!queued_context_actions_.empty()) {
    auto& action = queued_context_actions_.front();
    LOG(WARNING) << action.first << " sent to detached " << frame_debug_str_
                 << " will be ignored";
    queued_context_actions_.pop();
  }
}

void CefFrameImpl::ExecuteOnLocalFrame(const std::string& function_name,
                                       LocalFrameAction action) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!context_created_) {
    queued_context_actions_.emplace(function_name, std::move(action));
    MaybeInitializeScriptContext();
    return;
  }

  if (frame_) {
    std::move(action).Run(frame_);
  } else {
    LOG(WARNING) << function_name << " sent to detached " << frame_debug_str_
                 << " will be ignored";
  }
}

void CefFrameImpl::ConnectBrowserFrame(ConnectReason reason) {
  DCHECK(browser_connection_state_ == ConnectionState::DISCONNECTED ||
         browser_connection_state_ == ConnectionState::RECONNECT_PENDING);

  if (VLOG_IS_ON(1)) {
    std::string reason_str;
    switch (reason) {
      case ConnectReason::DID_COMMIT:
        reason_str = "DID_COMMIT";
        break;
      case ConnectReason::WAS_SHOWN:
        reason_str = "WAS_SHOWN";
        break;
      case ConnectReason::RETRY:
        reason_str = base::StringPrintf(
            "RETRY %zu/%zu", browser_connect_retry_ct_, kConnectionRetryMaxCt);
        break;
    }
    DVLOG(1) << __func__ << ": " << frame_debug_str_
             << " connection request (reason=" << reason_str << ")";
  }

  browser_connect_timer_.Stop();

  // Don't attempt to connect an invalid or bfcache'd frame. If a bfcache'd
  // frame returns to active status a reconnect will be triggered via
  // OnWasShown().
  if (!frame_ || attach_denied_ || blink_glue::IsInBackForwardCache(frame_)) {
    browser_connection_state_ = ConnectionState::DISCONNECTED;
    DVLOG(1) << __func__ << ": " << frame_debug_str_
             << " connection retry canceled (reason="
             << (frame_ ? (attach_denied_ ? "ATTACH_DENIED" : "BFCACHED")
                        : "INVALID")
             << ")";
    return;
  }

  browser_connection_state_ = ConnectionState::CONNECTION_PENDING;

  auto& browser_frame = GetBrowserFrame(/*expect_acked=*/false);
  CHECK(browser_frame);

  // True if this connection is a retry or if the frame just exited the
  // BackForwardCache.
  const bool reattached =
      browser_connect_retry_ct_ > 0 || reason == ConnectReason::WAS_SHOWN;

  // If the channel is working we should get a call to FrameAttachedAck().
  // Otherwise, OnDisconnect() should be called to retry the
  // connection.
  browser_frame->FrameAttached(receiver_.BindNewPipeAndPassRemote(),
                               reattached);
  receiver_.set_disconnect_with_reason_and_result_handler(
      base::BindOnce(&CefFrameImpl::OnRenderFrameDisconnect, this));
}

const mojo::Remote<cef::mojom::BrowserFrame>& CefFrameImpl::GetBrowserFrame(
    bool expect_acked) {
  DCHECK_EQ(expect_acked,
            browser_connection_state_ == ConnectionState::CONNECTION_ACKED);

  if (!browser_frame_.is_bound()) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame) {
      // Triggers creation of a CefBrowserFrame in the browser process.
      render_frame->GetBrowserInterfaceBroker().GetInterface(
          browser_frame_.BindNewPipeAndPassReceiver());
      browser_frame_.set_disconnect_with_reason_and_result_handler(
          base::BindOnce(&CefFrameImpl::OnBrowserFrameDisconnect, this));
    }
  }
  return browser_frame_;
}

void CefFrameImpl::OnBrowserFrameDisconnect(uint32_t custom_reason,
                                            const std::string& description,
                                            MojoResult error_result) {
  OnDisconnect(DisconnectReason::BROWSER_FRAME_DISCONNECT, custom_reason,
               description, error_result);
}

void CefFrameImpl::OnRenderFrameDisconnect(uint32_t custom_reason,
                                           const std::string& description,
                                           MojoResult error_result) {
  OnDisconnect(DisconnectReason::RENDER_FRAME_DISCONNECT, custom_reason,
               description, error_result);
}

// static
std::string CefFrameImpl::GetDisconnectDebugString(
    ConnectionState connection_state,
    bool frame_is_valid,
    bool frame_is_main,
    DisconnectReason reason,
    uint32_t custom_reason,
    const std::string& description,
    MojoResult error_result) {
  std::string reason_str;
  switch (reason) {
    case DisconnectReason::DETACHED:
      reason_str = "DETACHED";
      break;
    case DisconnectReason::RENDER_FRAME_DISCONNECT:
      reason_str = "RENDER_FRAME_DISCONNECT";
      break;
    case DisconnectReason::BROWSER_FRAME_DISCONNECT:
      reason_str = "BROWSER_FRAME_DISCONNECT";
      break;
  };

  std::string state_str;
  switch (connection_state) {
    case ConnectionState::DISCONNECTED:
      state_str = "DISCONNECTED";
      break;
    case ConnectionState::CONNECTION_PENDING:
      state_str = "CONNECTION_PENDING";
      break;
    case ConnectionState::CONNECTION_ACKED:
      state_str = "CONNECTION_ACKED";
      break;
    case ConnectionState::RECONNECT_PENDING:
      state_str = "RECONNECT_PENDING";
      break;
  }

  if (!frame_is_valid) {
    state_str += ", FRAME_INVALID";
  } else if (frame_is_main) {
    state_str += ", MAIN_FRAME";
  } else {
    state_str += ", SUB_FRAME";
  }

  if (custom_reason !=
      static_cast<uint32_t>(frame_util::ResetReason::kNoReason)) {
    state_str += ", custom_reason=" + base::NumberToString(custom_reason);
  }

  if (!description.empty()) {
    state_str += ", description=" + description;
  }

  if (error_result != MOJO_RESULT_OK) {
    state_str += ", error_result=" + base::NumberToString(error_result);
  }

  return "(reason=" + reason_str + ", current_state=" + state_str + ")";
}

void CefFrameImpl::OnDisconnect(DisconnectReason reason,
                                uint32_t custom_reason,
                                const std::string& description,
                                MojoResult error_result) {
  // Ignore multiple calls in close proximity (which may occur if both
  // |browser_frame_| and |receiver_| disconnect). |frame_| will be nullptr
  // when called from/after OnDetached().
  if (frame_ &&
      browser_connection_state_ == ConnectionState::RECONNECT_PENDING) {
    return;
  }

  // Ignore additional calls if we're already disconnected. DETACHED,
  // RENDER_FRAME_DISCONNECT and/or BROWSER_FRAME_DISCONNECT may arrive in any
  // order.
  if (browser_connection_state_ == ConnectionState::DISCONNECTED) {
    return;
  }

  const auto connection_state = browser_connection_state_;
  const bool frame_is_valid = !!frame_;
  const bool frame_is_main = frame_ && frame_->IsOutermostMainFrame();
  DVLOG(1) << __func__ << ": " << frame_debug_str_ << " disconnected "
           << GetDisconnectDebugString(connection_state, frame_is_valid,
                                       frame_is_main, reason, custom_reason,
                                       description, error_result);

  browser_frame_.reset();
  receiver_.reset();
  browser_connection_state_ = ConnectionState::DISCONNECTED;

  // True if the frame was previously bound/connected and then intentionally
  // detached (Receiver::ResetWithReason called) from the browser process side.
  const bool connected_and_intentionally_detached =
      (reason == DisconnectReason::BROWSER_FRAME_DISCONNECT ||
       reason == DisconnectReason::RENDER_FRAME_DISCONNECT) &&
      custom_reason !=
          static_cast<uint32_t>(frame_util::ResetReason::kNoReason);

  // Don't retry if the frame is invalid or if the browser process has
  // intentionally detached.
  if (!frame_ || attach_denied_ || connected_and_intentionally_detached) {
    return;
  }

  // True if the connection was closed (binding declined) from the browser
  // process side. This can occur during navigation or if a matching
  // RenderFrameHost is not currently available (like for bfcache'd frames).
  // When navigating there is a race in the browser process between
  // BrowserInterfaceBrokerImpl::GetInterface and RenderFrameHostImpl::
  // DidCommitNavigation. The connection will be closed if the GetInterface call
  // from the renderer is still in-flight when DidCommitNavigation calls
  // |broker_receiver_.reset()|. If, however, the GetInterface call arrives
  // first (BrowserInterfaceBrokerImpl::GetInterface called and the
  // PendingReceiver bound) then the binding will be successful and remain
  // connected until the connection is closed for some other reason (like the
  // Receiver being reset or the renderer process terminating).
  const bool connection_binding_declined =
      (reason == DisconnectReason::BROWSER_FRAME_DISCONNECT ||
       reason == DisconnectReason::RENDER_FRAME_DISCONNECT) &&
      error_result == MOJO_RESULT_FAILED_PRECONDITION;

  if (browser_connect_retry_ct_++ < kConnectionRetryMaxCt) {
    DVLOG(1) << __func__ << ": " << frame_debug_str_
             << " connection retry scheduled (" << browser_connect_retry_ct_
             << "/" << kConnectionRetryMaxCt << ")";
    if (!browser_connect_retry_log_.empty()) {
      browser_connect_retry_log_ += "; ";
    }
    browser_connect_retry_log_ += GetDisconnectDebugString(
        connection_state, frame_is_valid, frame_is_main, reason, custom_reason,
        description, error_result);

    // Use a shorter delay for the first retry attempt after the browser process
    // intentionally declines the connection. This will improve load performance
    // in normal circumstances (reasonably fast machine and navigations with
    // limited redirects).
    const auto retry_delay =
        connection_binding_declined && browser_connect_retry_ct_ == 1
            ? kConnectionRetryDelayShort
            : kConnectionRetryDelayLong;

    // Retry after a delay in case the frame is currently navigating or entering
    // the bfcache. In the navigation case the retry will likely succeed. In the
    // bfcache case the status may not be updated immediately, so we allow the
    // reconnect timer to trigger and then check the status in
    // ConnectBrowserFrame() instead.
    browser_connection_state_ = ConnectionState::RECONNECT_PENDING;
    browser_connect_timer_.Start(
        FROM_HERE, retry_delay,
        base::BindOnce(&CefFrameImpl::ConnectBrowserFrame, this,
                       ConnectReason::RETRY));
    return;
  }

  DVLOG(1) << __func__ << ": " << frame_debug_str_
           << " connection retry limit exceeded";

  // Don't crash on retry failures in cases where the browser process has
  // intentionally declined the connection and we have never been previously
  // connected. Also don't crash for sub-frame connection failures as those are
  // less likely to be important functionally. We still crash for other main
  // frame connection errors or in cases where a previously connected main frame
  // was disconnected without first being intentionally deleted or detached.
  const bool ignore_retry_failure =
      (connection_binding_declined && !ever_connected_) || !frame_is_main;

  // Trigger a crash in official builds.
  LOG_IF(FATAL, !ignore_retry_failure)
      << frame_debug_str_ << " connection retry failed "
      << GetDisconnectDebugString(connection_state, frame_is_valid,
                                  frame_is_main, reason, custom_reason,
                                  description, error_result)
      << ", prior disconnects: " << browser_connect_retry_log_;
}

void CefFrameImpl::SendToBrowserFrame(const std::string& function_name,
                                      BrowserFrameAction action) {
  if (!frame_ || attach_denied_) {
    // We're detached.
    LOG(WARNING) << function_name << " sent to detached " << frame_debug_str_
                 << " will be ignored";
    return;
  }

  if (browser_connection_state_ != ConnectionState::CONNECTION_ACKED) {
    // Queue actions until we're notified by the browser that it's ready to
    // handle them.
    queued_browser_actions_.emplace(function_name, std::move(action));
    return;
  }

  auto& browser_frame = GetBrowserFrame();
  CHECK(browser_frame);

  std::move(action).Run(browser_frame);
}

void CefFrameImpl::MaybeInitializeScriptContext() {
  if (did_initialize_script_context_) {
    return;
  }

  if (!did_commit_provisional_load_) {
    // Too soon for context initialization.
    return;
  }

  if (queued_context_actions_.empty()) {
    // Don't need early context initialization. Avoid it due to performance
    // consequences.
    return;
  }

  did_initialize_script_context_ = true;

  // Explicitly force creation of the script context. This occurred implicitly
  // via DidCommitProvisionalLoad prior to https://crrev.com/5150754880a.
  // Otherwise, a script context may never be created for a frame that doesn't
  // contain JS code.
  v8::HandleScope handle_scope(GetFrameIsolate(frame_));
  frame_->MainWorldScriptContext();
}

void CefFrameImpl::FrameAttachedAck(bool allow) {
  // Sent from the browser process in response to ConnectBrowserFrame() sending
  // FrameAttached().
  CHECK_EQ(ConnectionState::CONNECTION_PENDING, browser_connection_state_);
  browser_connection_state_ = ConnectionState::CONNECTION_ACKED;
  browser_connect_retry_ct_ = 0;
  browser_connect_retry_log_.clear();

  DVLOG(1) << __func__ << ": " << frame_debug_str_
           << " connection acked allow=" << allow;

  if (!allow) {
    // This will be followed by a connection disconnect from the browser side.
    attach_denied_ = true;
    while (!queued_browser_actions_.empty()) {
      queued_browser_actions_.pop();
    }
    return;
  }

  ever_connected_ = true;

  auto& browser_frame = GetBrowserFrame();
  CHECK(browser_frame);

  while (!queued_browser_actions_.empty()) {
    std::move(queued_browser_actions_.front().second).Run(browser_frame);
    queued_browser_actions_.pop();
  }
}

void CefFrameImpl::SendMessage(const std::string& name,
                               base::Value::List arguments) {
  if (auto app = CefAppManager::Get()->GetApplication()) {
    if (auto handler = app->GetRenderProcessHandler()) {
      CefRefPtr<CefProcessMessageImpl> message(
          new CefProcessMessageImpl(name, std::move(arguments),
                                    /*read_only=*/true));
      handler->OnProcessMessageReceived(browser_, this, PID_BROWSER,
                                        message.get());
    }
  }
}

void CefFrameImpl::SendSharedMemoryRegion(
    const std::string& name,
    base::WritableSharedMemoryRegion region) {
  if (auto app = CefAppManager::Get()->GetApplication()) {
    if (auto handler = app->GetRenderProcessHandler()) {
      CefRefPtr<CefProcessMessage> message(
          new CefProcessMessageSMRImpl(name, std::move(region)));
      handler->OnProcessMessageReceived(browser_, this, PID_BROWSER, message);
    }
  }
}

void CefFrameImpl::SendCommand(const std::string& command) {
  ExecuteOnLocalFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::string& command, blink::WebLocalFrame* frame) {
            frame->ExecuteCommand(blink::WebString::FromUTF8(command));
          },
          command));
}

void CefFrameImpl::SendCommandWithResponse(
    const std::string& command,
    cef::mojom::RenderFrame::SendCommandWithResponseCallback callback) {
  ExecuteOnLocalFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::string& command,
             cef::mojom::RenderFrame::SendCommandWithResponseCallback callback,
             blink::WebLocalFrame* frame) {
            blink::WebString response;

            if (base::EqualsCaseInsensitiveASCII(command, "getsource")) {
              response = blink_glue::DumpDocumentMarkup(frame);
            } else if (base::EqualsCaseInsensitiveASCII(command, "gettext")) {
              response = blink_glue::DumpDocumentText(frame);
            }

            std::move(callback).Run(
                string_util::CreateSharedMemoryRegion(response));
          },
          command, std::move(callback)));
}

void CefFrameImpl::SendJavaScript(const std::u16string& jsCode,
                                  const std::string& scriptUrl,
                                  int32_t startLine) {
  ExecuteOnLocalFrame(
      __FUNCTION__,
      base::BindOnce(
          [](const std::u16string& jsCode, const std::string& scriptUrl,
             blink::WebLocalFrame* frame) {
            frame->ExecuteScript(blink::WebScriptSource(
                blink::WebString::FromUTF16(jsCode), GURL(scriptUrl)));
          },
          jsCode, scriptUrl));
}

void CefFrameImpl::LoadRequest(cef::mojom::RequestParamsPtr params) {
  ExecuteOnLocalFrame(
      __FUNCTION__,
      base::BindOnce(
          [](cef::mojom::RequestParamsPtr params, blink::WebLocalFrame* frame) {
            blink::WebURLRequest request;
            CefRequestImpl::Get(params, request);
            blink_glue::StartNavigation(frame, request);
          },
          std::move(params)));
}

void CefFrameImpl::DidStopLoading() {
  // We should only receive this notification for the highest-level LocalFrame
  // in this frame's in-process subtree. If there are multiple of these for
  // the same browser then the other occurrences will be discarded in
  // OnLoadingStateChange.
  browser_->OnLoadingStateChange(false);

  if (blink::RuntimeEnabledFeatures::BackForwardCacheEnabled()) {
    // Refresh draggable regions. Otherwise, we may not receive updated regions
    // after navigation because LocalFrameView::UpdateDocumentAnnotatedRegion
    // lacks sufficient context. When bfcache is enabled we can't rely on
    // OnDidFinishLoad() as the frame may not actually be reloaded.
    OnDraggableRegionsChanged();
  }
}

void CefFrameImpl::MoveOrResizeStarted() {
  if (frame_) {
    auto web_view = frame_->View();
    if (web_view) {
      web_view->CancelPagePopup();
    }
  }
}

void CefFrameImpl::ContextLifecycleStateChanged(
    blink::mojom::blink::FrameLifecycleState state) {
  if (state == blink::mojom::FrameLifecycleState::kFrozen && IsMain() &&
      blink_glue::IsInBackForwardCache(frame_)) {
    browser_->OnEnterBFCache();
  }
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
