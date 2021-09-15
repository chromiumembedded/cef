// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/renderer/frame_impl.h"

#include "base/compiler_specific.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
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

  if (!frame_)
    return;

  auto& browser_frame = GetBrowserFrame();
  if (!browser_frame)
    return;

  auto impl = static_cast<CefProcessMessageImpl*>(message.get());
  browser_frame->SendMessage(impl->GetName(), impl->TakeArgumentList());
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

void CefFrameImpl::OnAttached(service_manager::BinderRegistry* registry) {
  // Called indirectly from RenderFrameCreated.
  registry->AddInterface(base::BindRepeating(
      &CefFrameImpl::BindRenderFrameReceiver, weak_ptr_factory_.GetWeakPtr()));

  auto& browser_frame = GetBrowserFrame();
  if (browser_frame) {
    browser_frame->FrameAttached();
  }
}

void CefFrameImpl::OnDidFinishLoad() {
  // Ignore notifications from the embedded frame hosting a mime-type plugin.
  // We'll eventually receive a notification from the owner frame.
  if (blink_glue::HasPluginFrameOwner(frame_))
    return;

  blink::WebDocumentLoader* dl = frame_->GetDocumentLoader();
  const int http_status_code = dl->GetResponse().HttpStatusCode();
  auto& browser_frame = GetBrowserFrame();
  if (browser_frame) {
    browser_frame->DidFinishFrameLoad(dl->GetUrl(), http_status_code);
  }

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

  auto& browser_frame = GetBrowserFrame();
  if (browser_frame) {
    browser_frame->UpdateDraggableRegions(
        regions.empty() ? absl::nullopt
                        : absl::make_optional(std::move(regions)));
  }
}

void CefFrameImpl::OnContextCreated() {
  context_created_ = true;

  CHECK(frame_);
  while (!queued_actions_.empty()) {
    auto& action = queued_actions_.front();
    std::move(action.second).Run(frame_);
    queued_actions_.pop();
  }
}

void CefFrameImpl::OnDetached() {
  // Called when this frame has been detached from the view. This *will* be
  // called for child frames when a parent frame is detached.
  // The browser may hold the last reference to |this|. Take a reference here to
  // keep |this| alive until after this method returns.
  CefRefPtr<CefFrameImpl> self = this;

  browser_->FrameDetached(frame_id_);

  receivers_.Clear();
  browser_frame_.reset();
  browser_ = nullptr;
  frame_ = nullptr;
  url_loader_factory_.reset();

  // In case we're destroyed without the context being created.
  while (!queued_actions_.empty()) {
    auto& action = queued_actions_.front();
    LOG(WARNING) << action.first << " sent to detached frame "
                 << frame_util::GetFrameDebugString(frame_id_)
                 << " will be ignored";
    queued_actions_.pop();
  }
}

void CefFrameImpl::ExecuteOnLocalFrame(const std::string& function_name,
                                       LocalFrameAction action) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!context_created_) {
    queued_actions_.push(std::make_pair(function_name, std::move(action)));
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

const mojo::Remote<cef::mojom::BrowserFrame>& CefFrameImpl::GetBrowserFrame() {
  if (!browser_frame_.is_bound()) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame) {
      // Triggers creation of a CefBrowserFrame in the browser process.
      render_frame->GetBrowserInterfaceBroker()->GetInterface(
          browser_frame_.BindNewPipeAndPassReceiver());
    }
  }
  return browser_frame_;
}

void CefFrameImpl::BindRenderFrameReceiver(
    mojo::PendingReceiver<cef::mojom::RenderFrame> receiver) {
  receivers_.Add(this, std::move(receiver));
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
             int32_t startLine, blink::WebLocalFrame* frame) {
            frame->ExecuteScript(
                blink::WebScriptSource(blink::WebString::FromUTF16(jsCode),
                                       GURL(scriptUrl), startLine));
          },
          jsCode, scriptUrl, startLine));
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
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
