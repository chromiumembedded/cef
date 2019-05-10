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

#include "libcef/common/cef_messages.h"
#include "libcef/common/net/http_header_utils.h"
#include "libcef/common/request_impl.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/render_frame_util.h"
#include "libcef/renderer/render_urlrequest_impl.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"

#include "content/renderer/render_frame_impl.h"
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

using blink::WebString;

CefFrameImpl::CefFrameImpl(CefBrowserImpl* browser,
                           blink::WebLocalFrame* frame,
                           int64_t frame_id)
    : browser_(browser), frame_(frame), frame_id_(frame_id) {}

CefFrameImpl::~CefFrameImpl() {}

bool CefFrameImpl::IsValid() {
  CEF_REQUIRE_RT_RETURN(false);

  return (frame_ != NULL);
}

void CefFrameImpl::Undo() {
  ExecuteCommand("Undo");
}

void CefFrameImpl::Redo() {
  ExecuteCommand("Redo");
}

void CefFrameImpl::Cut() {
  ExecuteCommand("Cut");
}

void CefFrameImpl::Copy() {
  ExecuteCommand("Copy");
}

void CefFrameImpl::Paste() {
  ExecuteCommand("Paste");
}

void CefFrameImpl::Delete() {
  ExecuteCommand("Delete");
}

void CefFrameImpl::SelectAll() {
  ExecuteCommand("SelectAll");
}

void CefFrameImpl::ViewSource() {
  NOTREACHED() << "ViewSource cannot be called from the renderer process";
}

void CefFrameImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_) {
    const CefString& content =
        std::string(blink::WebFrameContentDumper::DumpAsMarkup(frame_).Utf8());
    visitor->Visit(content);
  }
}

void CefFrameImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (frame_) {
    const CefString& content = blink_glue::DumpDocumentText(frame_);
    visitor->Visit(content);
  }
}

void CefFrameImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!browser_)
    return;

  CefMsg_LoadRequest_Params params;
  params.url = GURL(std::string(request->GetURL()));
  params.method = request->GetMethod();
  params.frame_id = frame_id_;
  params.site_for_cookies =
      GURL(std::string(request->GetFirstPartyForCookies()));

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);
  if (!headerMap.empty())
    params.headers = HttpHeaderUtils::GenerateHeaders(headerMap);

  CefRefPtr<CefPostData> postData = request->GetPostData();
  if (postData.get()) {
    CefPostDataImpl* impl = static_cast<CefPostDataImpl*>(postData.get());
    params.upload_data = new net::UploadData();
    impl->Get(*params.upload_data.get());
  }

  params.load_flags = request->GetFlags();

  browser_->LoadRequest(params);
}

void CefFrameImpl::LoadURL(const CefString& url) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!browser_)
    return;

  CefMsg_LoadRequest_Params params;
  params.url = GURL(url.ToString());
  params.method = "GET";
  params.frame_id = frame_id_;

  browser_->LoadRequest(params);
}

void CefFrameImpl::LoadString(const CefString& string, const CefString& url) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (frame_) {
    GURL gurl = GURL(url.ToString());
    content::RenderFrame::FromWebFrame(frame_)->LoadHTMLString(
        string.ToString(), gurl, "UTF-8", GURL(),
        false /* replace_current_item */);
  }
}

void CefFrameImpl::ExecuteJavaScript(const CefString& jsCode,
                                     const CefString& scriptUrl,
                                     int startLine) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (jsCode.empty())
    return;
  if (startLine < 1)
    startLine = 1;

  if (frame_) {
    GURL gurl = GURL(scriptUrl.ToString());
    frame_->ExecuteScript(blink::WebScriptSource(
        WebString::FromUTF16(jsCode.ToString16()), gurl, startLine));
  }
}

bool CefFrameImpl::IsMain() {
  CEF_REQUIRE_RT_RETURN(false);

  if (frame_)
    return (frame_->Parent() == NULL);
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
  CEF_REQUIRE_RT_RETURN(NULL);

  if (frame_) {
    blink::WebFrame* parent = frame_->Parent();
    if (parent && parent->IsWebLocalFrame())
      return browser_->GetWebFrameImpl(parent->ToWebLocalFrame()).get();
  }

  return NULL;
}

CefString CefFrameImpl::GetURL() {
  CefString url;
  CEF_REQUIRE_RT_RETURN(url);

  if (frame_) {
    GURL gurl = frame_->GetDocument().Url();
    if (gurl.is_empty()) {
      // For popups the main document URL will be empty during loading. Return
      // the provisional document URL instead.
      blink::WebDocumentLoader* loader = frame_->GetProvisionalDocumentLoader();
      if (loader)
        gurl = loader->GetUrl();
    }
    url = gurl.spec();
  }
  return url;
}

CefRefPtr<CefBrowser> CefFrameImpl::GetBrowser() {
  CEF_REQUIRE_RT_RETURN(NULL);

  return browser_;
}

CefRefPtr<CefV8Context> CefFrameImpl::GetV8Context() {
  CEF_REQUIRE_RT_RETURN(NULL);

  if (frame_) {
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    return new CefV8ContextImpl(isolate, frame_->MainWorldScriptContext());
  } else {
    return NULL;
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
  CEF_REQUIRE_RT_RETURN(NULL);

  if (!request || !client || !frame_)
    return NULL;

  CefRefPtr<CefRenderURLRequest> impl =
      new CefRenderURLRequest(this, request, client);
  if (impl->Start())
    return impl.get();
  return NULL;
}

blink::WebURLLoaderFactory* CefFrameImpl::GetURLLoaderFactory() {
  CEF_REQUIRE_RT();
  if (!url_loader_factory_ && frame_) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame) {
      url_loader_factory_ = render_frame->CreateURLLoaderFactory();
    }
  }
  return url_loader_factory_.get();
}

void CefFrameImpl::Detach() {
  browser_ = NULL;
  frame_ = NULL;
  url_loader_factory_.reset();
}

void CefFrameImpl::ExecuteCommand(const std::string& command) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_)
    frame_->ExecuteCommand(WebString::FromUTF8(command));
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
