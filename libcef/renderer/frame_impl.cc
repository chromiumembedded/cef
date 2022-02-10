// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/renderer/frame_impl.h"

#include "base/compiler_specific.h"

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

#include "libcef/common/app_manager.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/http_header_utils.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/string_util.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/render_frame_util.h"
#include "libcef/renderer/render_urlrequest_impl.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
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

namespace {

// Maximum number of times to retry the browser connection.
constexpr size_t kConnectionRetryMaxCt = 3U;

// Length of time to wait before initiating a browser connection retry.
constexpr auto kConnectionRetryDelay = base::Seconds(1);

// Length of time to wait for the browser connection ACK before timing out.
constexpr auto kConnectionTimeout = base::Seconds(4);

}  // namespace

CefFrameImpl::CefFrameImpl(CefBrowserImpl* browser,
                           blink::WebLocalFrame* frame,
                           int64_t frame_id)
    : browser_(browser), frame_(frame), frame_id_(frame_id) {}

CefFrameImpl::~CefFrameImpl() {}

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

void CefFrameImpl::Delete() {
  SendCommand("Delete");
}

void CefFrameImpl::SelectAll() {
  SendCommand("SelectAll");
}

void CefFrameImpl::ViewSource() {
  NOTREACHED() << "ViewSource cannot be called from the renderer process";
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

  if (!frame_)
    return;

  auto params = cef::mojom::RequestParams::New();
  static_cast<CefRequestImpl*>(request.get())->Get(params);
  LoadRequest(std::move(params));
}

void CefFrameImpl::LoadURL(const CefString& url) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_)
    return;

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

  if (frame_)
    return (frame_->Parent() == nullptr);
  return false;
}

bool CefFrameImpl::IsFocused() {
  CEF_REQUIRE_RT_RETURN(false);

  if (frame_ && frame_->View())
    return (frame_->View()->FocusedFrame() == frame_);
  return false;
}

CefString CefFrameImpl::GetName() {
  CefString name;
  CEF_REQUIRE_RT_RETURN(name);

  if (frame_)
    name = render_frame_util::GetName(frame_);
  return name;
}

int64 CefFrameImpl::GetIdentifier() {
  CEF_REQUIRE_RT_RETURN(0);

  return frame_id_;
}

CefRefPtr<CefFrame> CefFrameImpl::GetParent() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (frame_) {
    blink::WebFrame* parent = frame_->Parent();
    if (parent && parent->IsWebLocalFrame())
      return browser_->GetWebFrameImpl(parent->ToWebLocalFrame()).get();
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
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    return new CefV8ContextImpl(isolate, frame_->MainWorldScriptContext());
  } else {
    return nullptr;
  }
}

void CefFrameImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_)
    return;

  // Create a CefDOMDocumentImpl object that is valid only for the scope of this
  // method.
  CefRefPtr<CefDOMDocumentImpl> documentImpl;
  const blink::WebDocument& document = frame_->GetDocument();
  if (!document.IsNull())
    documentImpl = new CefDOMDocumentImpl(browser_, frame_);

  visitor->Visit(documentImpl.get());

  if (documentImpl.get())
    documentImpl->Detach();
}

CefRefPtr<CefURLRequest> CefFrameImpl::CreateURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (!request || !client || !frame_)
    return nullptr;

  CefRefPtr<CefRenderURLRequest> impl =
      new CefRenderURLRequest(this, request, client);
  if (impl->Start())
    return impl.get();
  return nullptr;
}

void CefFrameImpl::SendProcessMessage(CefProcessId target_process,
                                      CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_RT_RETURN_VOID();
  DCHECK_EQ(PID_BROWSER, target_process);
  DCHECK(message && message->IsValid());
  if (!message || !message->IsValid())
    return;

  SendToBrowserFrame(
      __FUNCTION__,
      base::BindOnce(
          [](CefRefPtr<CefProcessMessage> message,
             const BrowserFrameType& browser_frame) {
            auto impl = static_cast<CefProcessMessageImpl*>(message.get());
            browser_frame->SendMessage(impl->GetName(),
                                       impl->TakeArgumentList());
          },
          message));
}

std::unique_ptr<blink::WebURLLoader> CefFrameImpl::CreateURLLoader() {
  CEF_REQUIRE_RT();
  if (!frame_)
    return nullptr;

  if (!url_loader_factory_) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame) {
      url_loader_factory_ = render_frame->CreateURLLoaderFactory();
    }
  }
  if (!url_loader_factory_)
    return nullptr;

  return url_loader_factory_->CreateURLLoader(
      blink::WebURLRequest(),
      blink_glue::CreateResourceLoadingTaskRunnerHandle(frame_),
      blink_glue::CreateResourceLoadingMaybeUnfreezableTaskRunnerHandle(frame_),
      /*keep_alive_handle=*/mojo::NullRemote(),
      blink::WebBackForwardCacheLoaderHelper());
}

std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
CefFrameImpl::CreateResourceLoadInfoNotifierWrapper() {
  CEF_REQUIRE_RT();
  if (frame_) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame)
      return render_frame->CreateResourceLoadInfoNotifierWrapper();
  }
  return nullptr;
}

void CefFrameImpl::OnAttached() {
  // Called indirectly from RenderFrameCreated.
  ConnectBrowserFrame();
}

void CefFrameImpl::OnWasShown() {
  if (browser_connection_state_ == ConnectionState::DISCONNECTED) {
    // Reconnect a frame that has exited the bfcache.
    ConnectBrowserFrame();
  }
}

void CefFrameImpl::OnDidFinishLoad() {
  // Ignore notifications from the embedded frame hosting a mime-type plugin.
  // We'll eventually receive a notification from the owner frame.
  if (blink_glue::HasPluginFrameOwner(frame_))
    return;

  blink::WebDocumentLoader* dl = frame_->GetDocumentLoader();
  const int http_status_code = dl->GetResponse().HttpStatusCode();

  SendToBrowserFrame(__FUNCTION__,
                     base::BindOnce(
                         [](const GURL& url, int http_status_code,
                            const BrowserFrameType& browser_frame) {
                           browser_frame->DidFinishFrameLoad(url,
                                                             http_status_code);
                         },
                         dl->GetUrl(), http_status_code));

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
  // Match the behavior in ChromeRenderFrameObserver::DraggableRegionsChanged.
  // Only the main frame is allowed to control draggable regions, to avoid other
  // frames manipulate the regions in the browser process.
  if (frame_->Parent() != nullptr)
    return;

  blink::WebVector<blink::WebDraggableRegion> webregions =
      frame_->GetDocument().DraggableRegions();
  std::vector<cef::mojom::DraggableRegionEntryPtr> regions;
  if (!webregions.empty()) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);

    regions.reserve(webregions.size());
    for (const auto& webregion : webregions) {
      auto region = cef::mojom::DraggableRegionEntry::New(webregion.bounds,
                                                          webregion.draggable);
      render_frame->ConvertViewportToWindow(&region->bounds);
      regions.push_back(std::move(region));
    }
  }

  using RegionsArg =
      absl::optional<std::vector<cef::mojom::DraggableRegionEntryPtr>>;
  RegionsArg regions_arg =
      regions.empty() ? absl::nullopt : absl::make_optional(std::move(regions));

  SendToBrowserFrame(
      __FUNCTION__,
      base::BindOnce(
          [](RegionsArg regions_arg, const BrowserFrameType& browser_frame) {
            browser_frame->UpdateDraggableRegions(std::move(regions_arg));
          },
          std::move(regions_arg)));
}

void CefFrameImpl::OnContextCreated() {
  context_created_ = true;

  CHECK(frame_);
  while (!queued_context_actions_.empty()) {
    auto& action = queued_context_actions_.front();
    std::move(action.second).Run(frame_);
    queued_context_actions_.pop();
  }
}

void CefFrameImpl::OnDetached() {
  // Called when this frame has been detached from the view. This *will* be
  // called for child frames when a parent frame is detached.
  // The browser may hold the last reference to |this|. Take a reference here to
  // keep |this| alive until after this method returns.
  CefRefPtr<CefFrameImpl> self = this;

  frame_ = nullptr;

  browser_->FrameDetached(frame_id_);

  OnBrowserFrameDisconnect();

  browser_ = nullptr;
  url_loader_factory_.reset();

  // In case we never attached.
  while (!queued_browser_actions_.empty()) {
    auto& action = queued_browser_actions_.front();
    LOG(WARNING) << action.first << " sent to detached frame "
                 << frame_util::GetFrameDebugString(frame_id_)
                 << " will be ignored";
    queued_browser_actions_.pop();
  }

  // In case we're destroyed without the context being created.
  while (!queued_context_actions_.empty()) {
    auto& action = queued_context_actions_.front();
    LOG(WARNING) << action.first << " sent to detached frame "
                 << frame_util::GetFrameDebugString(frame_id_)
                 << " will be ignored";
    queued_context_actions_.pop();
  }
}

void CefFrameImpl::ExecuteOnLocalFrame(const std::string& function_name,
                                       LocalFrameAction action) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!context_created_) {
    queued_context_actions_.push(
        std::make_pair(function_name, std::move(action)));
    return;
  }

  if (frame_) {
    std::move(action).Run(frame_);
  } else {
    LOG(WARNING) << function_name << " sent to detached frame "
                 << frame_util::GetFrameDebugString(frame_id_)
                 << " will be ignored";
  }
}

void CefFrameImpl::ConnectBrowserFrame() {
  DCHECK(browser_connection_state_ == ConnectionState::DISCONNECTED ||
         browser_connection_state_ == ConnectionState::RECONNECT_PENDING);

  // Don't attempt to connect an invalid or bfcache'd frame. If a bfcache'd
  // frame returns to active status a reconnect will be triggered via
  // OnWasShown().
  if (!frame_ || blink_glue::IsInBackForwardCache(frame_)) {
    browser_connection_state_ = ConnectionState::DISCONNECTED;
    browser_connect_timer_.Stop();
    LOG(INFO) << "Connection retry canceled for frame "
              << frame_util::GetFrameDebugString(frame_id_);
    return;
  }

  if (browser_connect_retry_ct_ > 0) {
    LOG(INFO) << "Connection retry " << browser_connect_retry_ct_ << "/"
              << kConnectionRetryMaxCt << " for frame "
              << frame_util::GetFrameDebugString(frame_id_);
  }

  browser_connection_state_ = ConnectionState::CONNECTION_PENDING;
  browser_connect_timer_.Start(FROM_HERE, kConnectionTimeout, this,
                               &CefFrameImpl::OnBrowserFrameTimeout);

  auto& browser_frame = GetBrowserFrame(/*expect_acked=*/false);
  CHECK(browser_frame);

  // If the channel is working we should get a call to FrameAttachedAck().
  // Otherwise, OnBrowserFrameDisconnect() should be called to retry the
  // connection.
  browser_frame->FrameAttached(receiver_.BindNewPipeAndPassRemote(),
                               browser_connect_retry_ct_ > 0);
  receiver_.set_disconnect_handler(
      base::BindOnce(&CefFrameImpl::OnBrowserFrameDisconnect, this));
}

const mojo::Remote<cef::mojom::BrowserFrame>& CefFrameImpl::GetBrowserFrame(
    bool expect_acked) {
  DCHECK_EQ(expect_acked,
            browser_connection_state_ == ConnectionState::CONNECTION_ACKED);

  if (!browser_frame_.is_bound()) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame) {
      // Triggers creation of a CefBrowserFrame in the browser process.
      render_frame->GetBrowserInterfaceBroker()->GetInterface(
          browser_frame_.BindNewPipeAndPassReceiver());
      browser_frame_.set_disconnect_handler(
          base::BindOnce(&CefFrameImpl::OnBrowserFrameDisconnect, this));
    }
  }
  return browser_frame_;
}

void CefFrameImpl::OnBrowserFrameTimeout() {
  LOG(ERROR) << "Connection timeout for frame "
             << frame_util::GetFrameDebugString(frame_id_);
  OnBrowserFrameDisconnect();
}

void CefFrameImpl::OnBrowserFrameDisconnect() {
  // Ignore multiple calls in close proximity (which may occur if both
  // |browser_frame_| and |receiver_| disconnect). |frame_| will be nullptr
  // when called from/after OnDetached().
  if (frame_ &&
      browser_connection_state_ == ConnectionState::RECONNECT_PENDING) {
    return;
  }

  browser_frame_.reset();
  receiver_.reset();
  browser_connection_state_ = ConnectionState::DISCONNECTED;
  browser_connect_timer_.Stop();

  // Only retry if the frame is still valid.
  if (frame_) {
    if (browser_connect_retry_ct_++ < kConnectionRetryMaxCt) {
      // Retry after a delay in case the frame is currently navigating, being
      // destroyed, or entering the bfcache. In the navigation case the retry
      // will likely succeed. In the destruction case the retry will be
      // ignored/canceled due to OnDetached(). In the bfcache case the status
      // may not be updated immediately, so we allow the reconnect timer to
      // trigger and check the status in ConnectBrowserFrame() instead.
      browser_connection_state_ = ConnectionState::RECONNECT_PENDING;
      browser_connect_timer_.Start(FROM_HERE, kConnectionRetryDelay, this,
                                   &CefFrameImpl::ConnectBrowserFrame);
    } else {
      // Trigger a crash in official builds.
      LOG(FATAL) << "Connection retry failure for frame "
                 << frame_util::GetFrameDebugString(frame_id_);
    }
  }
}

void CefFrameImpl::SendToBrowserFrame(const std::string& function_name,
                                      BrowserFrameAction action) {
  if (!frame_) {
    // We've been detached.
    LOG(WARNING) << function_name << " sent to detached frame "
                 << frame_util::GetFrameDebugString(frame_id_)
                 << " will be ignored";
    return;
  }

  if (browser_connection_state_ != ConnectionState::CONNECTION_ACKED) {
    // Queue actions until we're notified by the browser that it's ready to
    // handle them.
    queued_browser_actions_.push(
        std::make_pair(function_name, std::move(action)));
    return;
  }

  auto& browser_frame = GetBrowserFrame();
  CHECK(browser_frame);

  std::move(action).Run(browser_frame);
}

void CefFrameImpl::FrameAttachedAck() {
  // Sent from the browser process in response to ConnectBrowserFrame() sending
  // FrameAttached().
  CHECK_EQ(ConnectionState::CONNECTION_PENDING, browser_connection_state_);
  browser_connection_state_ = ConnectionState::CONNECTION_ACKED;
  browser_connect_retry_ct_ = 0;
  browser_connect_timer_.Stop();

  auto& browser_frame = GetBrowserFrame();
  CHECK(browser_frame);

  while (!queued_browser_actions_.empty()) {
    std::move(queued_browser_actions_.front().second).Run(browser_frame);
    queued_browser_actions_.pop();
  }
}

void CefFrameImpl::SendMessage(const std::string& name, base::Value arguments) {
  if (auto app = CefAppManager::Get()->GetApplication()) {
    if (auto handler = app->GetRenderProcessHandler()) {
      auto& list_value = base::Value::AsListValue(arguments);
      CefRefPtr<CefProcessMessageImpl> message(new CefProcessMessageImpl(
          name, std::move(const_cast<base::ListValue&>(list_value)),
          /*read_only=*/true));
      handler->OnProcessMessageReceived(browser_, this, PID_BROWSER,
                                        message.get());
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

            if (base::LowerCaseEqualsASCII(command, "getsource")) {
              response = blink_glue::DumpDocumentMarkup(frame);
            } else if (base::LowerCaseEqualsASCII(command, "gettext")) {
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

  // Refresh draggable regions. Otherwise, we may not receive updated regions
  // after navigation because LocalFrameView::UpdateDocumentAnnotatedRegion
  // lacks sufficient context.
  OnDraggableRegionsChanged();
}

void CefFrameImpl::MoveOrResizeStarted() {
  if (frame_) {
    auto web_view = frame_->View();
    if (web_view)
      web_view->CancelPagePopup();
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
