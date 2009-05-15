// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "context.h"
#include "browser_impl.h"
#include "request_impl.h"

#include "base/string_util.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/webframe.h"

using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURL;


CefBrowserImpl::CefBrowserImpl(CefWindowInfo& windowInfo, bool popup,
                               CefRefPtr<CefHandler> handler,
                               const std::wstring& url)
  : window_info_(windowInfo), is_popup_(popup), is_modal_(false),
  handler_(handler), webviewhost_(NULL), popuphost_(NULL), url_(url),
  unique_id_(0)
{
  delegate_ = new BrowserWebViewDelegate(this);
  nav_controller_.reset(new BrowserNavigationController(this));
}

CefBrowserImpl::~CefBrowserImpl()
{
  RemoveAllJSHandlers();
}

void CefBrowserImpl::GoBack()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
    &CefBrowserImpl::UIT_HandleAction, MENU_ID_NAV_BACK, TF_MAIN));
}

void CefBrowserImpl::GoForward()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_NAV_FORWARD, TF_MAIN));
}

void CefBrowserImpl::Reload()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_NAV_RELOAD, TF_MAIN));
}

void CefBrowserImpl::StopLoad()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_NAV_STOP, TF_MAIN));
}

void CefBrowserImpl::Undo(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_UNDO, targetFrame));
}

void CefBrowserImpl::Redo(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_REDO, targetFrame));
}

void CefBrowserImpl::Cut(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_CUT, targetFrame));
}

void CefBrowserImpl::Copy(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_COPY, targetFrame));
}

void CefBrowserImpl::Paste(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_PASTE, targetFrame));
}

void CefBrowserImpl::Delete(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_DELETE, targetFrame));
}

void CefBrowserImpl::SelectAll(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_SELECTALL, targetFrame));
}

void CefBrowserImpl::SetFocus(bool enable)
{
  if (_Context->RunningOnUIThread())
  {
    UIT_SetFocus(UIT_GetWebViewHost(), enable);
  }
  else
  {
    PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_SetFocus,
      UIT_GetWebViewHost(), enable));
  }
}

void CefBrowserImpl::Print(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_PRINT, targetFrame));
}

void CefBrowserImpl::ViewSource(TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction,
      MENU_ID_VIEWSOURCE, targetFrame));
}
void CefBrowserImpl::LoadRequest(CefRefPtr<CefRequest> request)
{
  DCHECK(request.get() != NULL);
  request->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadURLForRequestRef, request.get()));
}

void CefBrowserImpl::LoadURL(const std::wstring& url, const std::wstring& frame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadURLForFrame, url, frame));
}

void CefBrowserImpl::LoadString(const std::wstring& string,
                          const std::wstring& url)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadHTML, string, url));
}

void CefBrowserImpl::LoadStream(CefRefPtr<CefStreamReader> stream,
                          const std::wstring& url)
{
  DCHECK(stream.get() != NULL);
  stream->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadHTMLForStreamRef, stream.get(), url));
}

void CefBrowserImpl::ExecuteJavaScript(const std::wstring& jsCode, 
                                       const std::wstring& scriptUrl,
                                       int startLine,
                                       TargetFrame targetFrame)
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_ExecuteJavaScript, jsCode, scriptUrl, startLine,
      targetFrame));
}

bool CefBrowserImpl::AddJSHandler(const std::wstring& classname,
                                  CefRefPtr<CefJSHandler> handler)
{
  DCHECK(handler.get());
  bool rv = false;

  Lock();
  if(!HasJSHandler(classname)) {
    CefRefPtr<CefJSContainer> jscon(new CefJSContainer(this, handler));
    jscontainers_.insert(std::make_pair(classname, jscon));
    rv = true;
  }
  Unlock();

  return rv;
}

bool CefBrowserImpl::HasJSHandler(const std::wstring& classname)
{
  Lock();
  bool rv = (jscontainers_.find(classname) != jscontainers_.end());
  Unlock();

  return rv;
}

CefRefPtr<CefJSHandler> CefBrowserImpl::GetJSHandler(const std::wstring& classname)
{
  CefRefPtr<CefJSHandler> handler;

  Lock();
  JSContainerMap::const_iterator it = jscontainers_.find(classname);
  if(it != jscontainers_.end())
    handler = it->second->GetHandler();
  Unlock();

  return handler;
}

bool CefBrowserImpl::RemoveJSHandler(const std::wstring& classname)
{
  bool rv = false;
  
  Lock();
  JSContainerMap::iterator it = jscontainers_.find(classname);
  if(it != jscontainers_.end()) {
    jscontainers_.erase(it);
    rv = true;
  }
  Unlock();

  return rv;
}

void CefBrowserImpl::RemoveAllJSHandlers()
{
  Lock();
  jscontainers_.clear();
  Unlock();
}

bool CefBrowserImpl::IsPopup()
{
  Lock();
  bool popup = is_popup_;
  Unlock();
  return popup;
}

CefRefPtr<CefHandler> CefBrowserImpl::GetHandler()
{
  return handler_;
}

std::wstring CefBrowserImpl::GetURL()
{
  Lock();
  std::wstring url = url_;
  Unlock();
  return url;
}

void CefBrowserImpl::SetURL(const std::wstring& url)
{
  Lock();
  url_ = url;
  Unlock();
}

bool CefBrowser::CreateBrowser(CefWindowInfo& windowInfo, bool popup,
                               CefRefPtr<CefHandler> handler,
                               const std::wstring& url)
{
  if(!_Context.get())
    return false;

  std::wstring newUrl = url;

  if(handler.get())
  {
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    CefHandler::RetVal rv =
        handler->HandleBeforeCreated(NULL, windowInfo, popup, handler, newUrl);
    if(rv == RV_HANDLED)
      return false;
  }

  CefRefPtr<CefBrowserImpl> browser(
      new CefBrowserImpl(windowInfo, popup, handler, newUrl));
  PostTask(FROM_HERE, NewRunnableMethod(browser.get(),
      &CefBrowserImpl::UIT_CreateBrowser));
  return true;
}

CefRefPtr<CefBrowser> CefBrowser::CreateBrowserSync(CefWindowInfo& windowInfo,
                                                    bool popup,
                                                    CefRefPtr<CefHandler> handler,
                                                    const std::wstring& url)
{
  if(!_Context.get() || !_Context->RunningOnUIThread())
    return NULL;

  std::wstring newUrl = url;
  CefRefPtr<CefBrowser> alternateBrowser;

  if(handler.get())
  {
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    CefHandler::RetVal rv = handler->HandleBeforeCreated(NULL, windowInfo,
        popup, handler, newUrl);
    if(rv == RV_HANDLED)
      return false;
  }

  CefRefPtr<CefBrowser> browser(
      new CefBrowserImpl(windowInfo, popup, handler, newUrl));

  static_cast<CefBrowserImpl*>(browser.get())->UIT_CreateBrowser();

  return browser;
}

void CefBrowserImpl::UIT_LoadURL(const std::wstring& url)
{
  REQUIRE_UIT();
  UIT_LoadURLForRequest(url, std::wstring(), std::wstring(), NULL,
      WebRequest::HeaderMap());
}

void CefBrowserImpl::UIT_LoadURLForFrame(const std::wstring& url,
                                         const std::wstring& frame_name)
{
  REQUIRE_UIT();
  UIT_LoadURLForRequest(url, frame_name, std::wstring(), NULL,
      WebRequest::HeaderMap());
}

void CefBrowserImpl::UIT_LoadURLForRequestRef(CefRequest* request)
{
  REQUIRE_UIT();
    
  std::wstring url = request->GetURL();
  std::wstring frame_name = request->GetFrame();
  std::wstring method = request->GetMethod();

  CefRequestImpl *impl = static_cast<CefRequestImpl*>(request);

  scoped_refptr<net::UploadData> upload_data;
  CefRefPtr<CefPostData> postdata = impl->GetPostData();
  if(postdata.get())
  {
    upload_data = new net::UploadData();
    static_cast<CefPostDataImpl*>(postdata.get())->Get(*upload_data.get());
  }

  WebRequest::HeaderMap headers;
  impl->GetHeaderMap(headers);

  UIT_LoadURLForRequest(url, frame_name, method, upload_data.get(),
    headers);

  request->Release();
}

void CefBrowserImpl::UIT_LoadURLForRequest(const std::wstring& url,
                                           const std::wstring& frame_name,
                                           const std::wstring& method,
                                           net::UploadData *upload_data,
                                           const WebRequest::HeaderMap& headers)
{
  REQUIRE_UIT();
  
  if (url.empty())
      return;

  GURL gurl(url);

  if (!gurl.is_valid() && !gurl.has_scheme()) {
    // Try to add "http://" at the beginning
    gurl = GURL(std::wstring(L"http://") + url);
    if (!gurl.is_valid())
      return;
  }

  nav_controller_->LoadEntry(new BrowserNavigationEntry(
      -1, gurl, std::wstring(), frame_name, method, upload_data, headers));
}

void CefBrowserImpl::UIT_LoadHTML(const std::wstring& html,
                                  const std::wstring& url)
{
  REQUIRE_UIT();
    
  GURL gurl(url);

  if (!gurl.is_valid() && !gurl.has_scheme()) {
    // Try to add "http://" at the beginning
    gurl = GURL(std::wstring(L"http://") + url);
    if (!gurl.is_valid())
      return;
  }

  UIT_GetWebView()->GetMainFrame()->LoadHTMLString(WideToUTF8(html), gurl);
}

void CefBrowserImpl::UIT_LoadHTMLForStreamRef(CefStreamReader* stream,
                                              const std::wstring& url)
{
  REQUIRE_UIT();
    
  GURL gurl(url);

  if (!gurl.is_valid() && !gurl.has_scheme()) {
    // Try to add "http://" at the beginning
    gurl = GURL(std::wstring(L"http://") + url);
    if (!gurl.is_valid())
      return;
  }

  // read all of the stream data into a std::string.
  std::stringstream ss;
  char buff[BUFFER_SIZE];
  size_t read;
  do {
    read = stream->Read(buff, sizeof(char), BUFFER_SIZE-1);
    if(read > 0) {
      buff[read] = 0;
      ss << buff;
    }
  }
  while(read > 0);

  UIT_GetWebView()->GetMainFrame()->LoadHTMLString(ss.str(), gurl);

  stream->Release();
}

void CefBrowserImpl::UIT_ExecuteJavaScript(const std::wstring& js_code, 
                                           const std::wstring& script_url,
                                           int start_line,
                                           TargetFrame targetFrame)
{
  REQUIRE_UIT();

  WebFrame* frame;
  if(targetFrame == TF_FOCUSED)
    frame = UIT_GetWebView()->GetFocusedFrame();
  else
    frame = UIT_GetWebView()->GetMainFrame();

  frame->ExecuteScript(
      WebScriptSource(WebString(js_code), WebURL(GURL(script_url)),
                      start_line));
}

void CefBrowserImpl::UIT_GoBackOrForward(int offset)
{
  REQUIRE_UIT();
  nav_controller_->GoToOffset(offset);
}

void CefBrowserImpl::UIT_Reload()
{
  REQUIRE_UIT();
  nav_controller_->Reload();
}

bool CefBrowserImpl::UIT_Navigate(const BrowserNavigationEntry& entry,
                                  bool reload)
{
  REQUIRE_UIT();
    
  WebRequestCachePolicy cache_policy;
  if (reload) {
    cache_policy = WebRequestReloadIgnoringCacheData;
  } else if (entry.GetPageID() != -1) {
    cache_policy = WebRequestReturnCacheDataElseLoad;
  } else {
    cache_policy = WebRequestUseProtocolCachePolicy;
  }

  scoped_ptr<WebRequest> request(WebRequest::Create(entry.GetURL()));
  request->SetCachePolicy(cache_policy);
  // If we are reloading, then WebKit will use the state of the current page.
  // Otherwise, we give it the state to navigate to.
  if (!reload)
    request->SetHistoryState(entry.GetContentState());
    
  request->SetExtraData(
      new BrowserExtraRequestData(entry.GetPageID()));

  if(entry.GetMethod().size() > 0)
    request->SetHttpMethod(WideToUTF8(entry.GetMethod()));

  if(entry.GetHeaders().size() > 0)
    request->SetHttpHeaders(entry.GetHeaders());
	
  if(entry.GetUploadData())
  {
    if(request->GetHttpMethod() == "GET" || request->GetHttpMethod() == "HEAD")
      request->SetHttpMethod("POST");
    if(request->GetHttpHeaderValue("Content-Type").size() == 0) {
      request->SetHttpHeaderValue(
        "Content-Type", "application/x-www-form-urlencoded");
    }
    request->SetUploadData(*entry.GetUploadData());
  }

  // Get the right target frame for the entry.
  WebFrame* frame = UIT_GetWebView()->GetMainFrame();
  if (!entry.GetTargetFrame().empty())
    frame = UIT_GetWebView()->GetFrameWithName(entry.GetTargetFrame());
  // TODO(mpcomplete): should we clear the target frame, or should
  // back/forward navigations maintain the target frame?

  frame->LoadRequest(request.get());
  // Restore focus to the main frame prior to loading new request.
  // This makes sure that we don't have a focused iframe. Otherwise, that
  // iframe would keep focus when the SetFocus called immediately after
  // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
  // for more details).
  UIT_GetWebView()->SetFocusedFrame(frame);
  UIT_SetFocus(UIT_GetWebViewHost(), true);

  return true;
}

void CefBrowserImpl::UIT_BindJSObjectsToWindow(WebFrame* frame)
{
  REQUIRE_UIT();
    
  Lock();
  JSContainerMap::const_iterator it = jscontainers_.begin();
  for(; it != jscontainers_.end(); ++it)
    it->second->BindToJavascript(frame, it->first);
  Unlock();
}

CefRefPtr<CefBrowserImpl> CefBrowserImpl::UIT_CreatePopupWindow(const std::wstring& url)
{
  REQUIRE_UIT();
    
  CefWindowInfo info;
  info.SetAsPopup(NULL, url.c_str());
  CefRefPtr<CefHandler> handler = handler_;
  std::wstring newUrl = url;

  if(handler_.get())
  {
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    CefHandler::RetVal rv =
        handler_->HandleBeforeCreated(this, info, true, handler, newUrl);
    if(rv == RV_HANDLED)
      return NULL;
  }

  CefRefPtr<CefBrowserImpl> browser(
      new CefBrowserImpl(info, true, handler, newUrl));
  browser->UIT_CreateBrowser();

  return browser;
}

void CefBrowserImpl::UIT_Show(WebView* webview,
                              WindowOpenDisposition disposition)
{
  REQUIRE_UIT();
  delegate_->Show(webview, disposition);
}

void CefBrowserImpl::UIT_HandleAction(CefHandler::MenuId menuId,
                                      TargetFrame target)
{
  REQUIRE_UIT();
  
  WebFrame* frame;
  if(target == TF_FOCUSED)
    frame = UIT_GetWebView()->GetFocusedFrame();
  else
    frame = UIT_GetWebView()->GetMainFrame();

  switch(menuId)
  {
    case MENU_ID_NAV_BACK:
      UIT_GoBackOrForward(-1);
      break;
    case MENU_ID_NAV_FORWARD:
      UIT_GoBackOrForward(1);
      break;
    case MENU_ID_NAV_RELOAD:
      UIT_Reload();
      break;
    case MENU_ID_NAV_STOP:
      UIT_GetWebView()->StopLoading();
      break;
    case MENU_ID_UNDO:
      frame->Undo();
      break;
    case MENU_ID_REDO:
      frame->Redo();
      break;
    case MENU_ID_CUT:
      frame->Cut();
      break;
    case MENU_ID_COPY:
      frame->Copy();
      break;
    case MENU_ID_PASTE:
      frame->Paste();
      break;
    case MENU_ID_DELETE:
      frame->Delete();
      break;
    case MENU_ID_SELECTALL:
      frame->SelectAll();
      break;
    case MENU_ID_PRINT:
      UIT_PrintPages(frame);
      break;
    case MENU_ID_VIEWSOURCE:
      UIT_ViewDocumentString(frame);
      break;
  }
}
