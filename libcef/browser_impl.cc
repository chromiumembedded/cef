// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "context.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "request_impl.h"

#include "base/utf_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/glue_serialize.h"

using WebKit::WebFrame;
using WebKit::WebHTTPBody;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using WebKit::WebView;
using webkit_glue::StdStringToWebString;
using webkit_glue::WebStringToStdString;


CefBrowserImpl::CefBrowserImpl(CefWindowInfo& windowInfo, bool popup,
                               CefRefPtr<CefHandler> handler)
  : window_info_(windowInfo), is_popup_(popup), is_modal_(false),
  handler_(handler), webviewhost_(NULL), popuphost_(NULL), unique_id_(0),
  frame_main_(NULL)
{
  delegate_.reset(new BrowserWebViewDelegate(this));
  popup_delegate_.reset(new BrowserWebViewDelegate(this));
  nav_controller_.reset(new BrowserNavigationController(this));
}

void CefBrowserImpl::GoBack()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
    &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_BACK));
}

void CefBrowserImpl::GoForward()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_FORWARD));
}

void CefBrowserImpl::Reload()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_RELOAD));
}

void CefBrowserImpl::StopLoad()
{
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_STOP));
}

void CefBrowserImpl::SetFocus(bool enable)
{
  if (_Context->RunningOnUIThread())
  {
    UIT_SetFocus(GetWebViewHost(), enable);
  }
  else
  {
    PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_SetFocus,
      GetWebViewHost(), enable));
  }
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

CefRefPtr<CefFrame> CefBrowserImpl::GetMainFrame()
{
  return GetCefFrame(GetWebView()->mainFrame());
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFocusedFrame()
{
  return GetCefFrame(GetWebView()->focusedFrame());
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFrame(const std::wstring& name)
{
  WebFrame* frame = GetWebView()->findFrameByName(name);
  if(frame)
    return GetCefFrame(frame);
  return NULL;
}

void CefBrowserImpl::GetFrameNames(std::vector<std::wstring>& names)
{
  WebView* view = GetWebView();
  WebFrame* main_frame = view->mainFrame();
  WebFrame* it = main_frame;
  do {
    if(it != main_frame)
      names.push_back(UTF16ToWideHack(it->name()));
    it = it->traverseNext(true);
  } while (it != main_frame);
}

CefRefPtr<CefFrame> CefBrowserImpl::GetCefFrame(WebFrame* frame)
{
  CefRefPtr<CefFrame> cef_frame;
  Lock();

  WebView *view = GetWebView();
  if(view) {
    if(frame == view->mainFrame()) {
      // Use or create the single main frame reference.
      if(frame_main_ == NULL)
        frame_main_ = new CefFrameImpl(this, std::wstring());
      cef_frame = frame_main_;
    } else {
      // Locate or create the appropriate named reference.
      std::wstring name = UTF16ToWideHack(frame->name());
      DCHECK(!name.empty());
      FrameMap::const_iterator it = frames_.find(name);
      if(it != frames_.end())
        cef_frame = it->second;
      else {
        cef_frame = new CefFrameImpl(this, name);
        frames_.insert(std::make_pair(name, cef_frame.get()));
      }
    }
  }

  Unlock();
  return cef_frame;
}

void CefBrowserImpl::RemoveCefFrame(const std::wstring& name)
{
  Lock();

  if(name.empty())
    frame_main_ = NULL;
  else {
    FrameMap::iterator it = frames_.find(name);
    if(it != frames_.end())
      frames_.erase(it);
  }

  Unlock();
}

WebFrame* CefBrowserImpl::GetWebFrame(CefRefPtr<CefFrame> frame)
{
  std::wstring name = frame->GetName();
  if(name.empty())
    return GetWebView()->mainFrame();
  return GetWebView()->findFrameByName(name);
}

void CefBrowserImpl::Undo(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_UNDO, frame.get()));
}

void CefBrowserImpl::Redo(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_REDO, frame.get()));
}

void CefBrowserImpl::Cut(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_CUT, frame.get()));
}

void CefBrowserImpl::Copy(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_COPY, frame.get()));
}

void CefBrowserImpl::Paste(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_PASTE, frame.get()));
}

void CefBrowserImpl::Delete(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_DELETE, frame.get()));
}

void CefBrowserImpl::SelectAll(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_SELECTALL, frame.get()));
}


void CefBrowserImpl::Print(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_PRINT, frame.get()));
}

void CefBrowserImpl::ViewSource(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_VIEWSOURCE, frame.get()));
}

void CefBrowserImpl::LoadRequest(CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefRequest> request)
{
  DCHECK(request.get() != NULL);
  frame->AddRef();
  request->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadURLForRequestRef, frame.get(), request.get()));
}

void CefBrowserImpl::LoadURL(CefRefPtr<CefFrame> frame,
                             const std::wstring& url)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadURL, frame.get(), url));
}

void CefBrowserImpl::LoadString(CefRefPtr<CefFrame> frame,
                                const std::wstring& string,
                                const std::wstring& url)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadHTML, frame.get(), string, url));
}

void CefBrowserImpl::LoadStream(CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefStreamReader> stream,
                                const std::wstring& url)
{
  DCHECK(stream.get() != NULL);
  frame->AddRef();
  stream->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadHTMLForStreamRef, frame.get(), stream.get(),
      url));
}

void CefBrowserImpl::ExecuteJavaScript(CefRefPtr<CefFrame> frame,
                                       const std::wstring& jsCode, 
                                       const std::wstring& scriptUrl,
                                       int startLine)
{
  frame->AddRef();
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_ExecuteJavaScript, frame.get(), jsCode, scriptUrl,
      startLine));
}

std::wstring CefBrowserImpl::GetURL(CefRefPtr<CefFrame> frame)
{
  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    std::string spec = web_frame->url().spec();
    return UTF8ToWide(spec);
  }
  return std::wstring();
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
      new CefBrowserImpl(windowInfo, popup, handler));
  PostTask(FROM_HERE, NewRunnableMethod(browser.get(),
      &CefBrowserImpl::UIT_CreateBrowser, newUrl));
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

  CefRefPtr<CefBrowser> browser(new CefBrowserImpl(windowInfo, popup, handler));
  static_cast<CefBrowserImpl*>(browser.get())->UIT_CreateBrowser(newUrl);

  return browser;
}

void CefBrowserImpl::UIT_LoadURL(CefFrame* frame,
                                 const std::wstring& url)
{
  UIT_LoadURLForRequest(frame, url, std::wstring(), WebHTTPBody(),
      CefRequest::HeaderMap());
}

void CefBrowserImpl::UIT_LoadURLForRequestRef(CefFrame* frame,
                                              CefRequest* request)
{
  std::wstring url = request->GetURL();
  std::wstring method = request->GetMethod();

  CefRequestImpl *impl = static_cast<CefRequestImpl*>(request);

  WebHTTPBody upload_data;
  CefRefPtr<CefPostData> postdata = impl->GetPostData();
  if(postdata.get()) {
    upload_data.initialize();
    static_cast<CefPostDataImpl*>(postdata.get())->Get(upload_data);
  }

  CefRequest::HeaderMap headers;
  impl->GetHeaderMap(headers);

  UIT_LoadURLForRequest(frame, url, method, upload_data, headers);

  request->Release();
}

void CefBrowserImpl::UIT_LoadURLForRequest(CefFrame* frame,
                                         const std::wstring& url,
                                         const std::wstring& method,
                                         const WebKit::WebHTTPBody& upload_data,
                                         const CefRequest::HeaderMap& headers)
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

  nav_controller_->LoadEntry(
      new BrowserNavigationEntry(-1, gurl, std::wstring(), frame->GetName(),
          method, upload_data, headers));

  frame->Release();
}

void CefBrowserImpl::UIT_LoadHTML(CefFrame* frame,
                                  const std::wstring& html,
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

  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame)
    web_frame->loadHTMLString(WideToUTF8(html), gurl);

  frame->Release();
}

void CefBrowserImpl::UIT_LoadHTMLForStreamRef(CefFrame* frame,
                                              CefStreamReader* stream,
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

  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame)
    web_frame->loadHTMLString(ss.str(), gurl);

  stream->Release();
  frame->Release();
}

void CefBrowserImpl::UIT_ExecuteJavaScript(CefFrame* frame,
                                           const std::wstring& js_code, 
                                           const std::wstring& script_url,
                                           int start_line)
{
  REQUIRE_UIT();

  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    web_frame->executeScript(
        WebScriptSource(WebString(js_code), WebURL(GURL(script_url)),
                        start_line));
  }

  frame->Release();
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

  // Get the right target frame for the entry.
  WebFrame* frame;
  if (!entry.GetTargetFrame().empty())
    frame = GetWebView()->findFrameByName(entry.GetTargetFrame());
  else
    frame = GetWebView()->mainFrame();
  // TODO(mpcomplete): should we clear the target frame, or should
  // back/forward navigations maintain the target frame?

  // A navigation resulting from loading a javascript URL should not be
  // treated as a browser initiated event.  Instead, we want it to look as if
  // the page initiated any load resulting from JS execution.
  if (!entry.GetURL().SchemeIs("javascript")) {
    delegate_->set_pending_extra_data(
        new BrowserExtraData(entry.GetPageID()));
  }

  // If we are reloading, then WebKit will use the state of the current page.
  // Otherwise, we give it the state to navigate to.
  if (reload) {
    frame->reload();
  } else if (!entry.GetContentState().empty()) {
    DCHECK(entry.GetPageID() != -1);
    frame->loadHistoryItem(
        webkit_glue::HistoryItemFromString(entry.GetContentState()));
  } else {
    DCHECK(entry.GetPageID() == -1);
    WebURLRequest request(entry.GetURL());

    if(entry.GetMethod().size() > 0) {
      request.setHTTPMethod(
          StdStringToWebString(WideToUTF8(entry.GetMethod())));
    }

    if(entry.GetHeaders().size() > 0)
      CefRequestImpl::SetHeaderMap(entry.GetHeaders(), request);

    if(!entry.GetUploadData().isNull())
    {
      std::string method = WebStringToStdString(request.httpMethod());
      if(method == "GET" || method == "HEAD") {
        request.setHTTPMethod(StdStringToWebString("POST"));
      }
      if(request.httpHeaderField(StdStringToWebString("Content-Type")).length()
          == 0) {
        request.setHTTPHeaderField(
            StdStringToWebString("Content-Type"),
            StdStringToWebString("application/x-www-form-urlencoded"));
      }
      request.setHTTPBody(entry.GetUploadData());
    }

    frame->loadRequest(request);
  }

  // In case LoadRequest failed before DidCreateDataSource was called.
  delegate_->set_pending_extra_data(NULL);

  if (handler_.get() && handler_->HandleSetFocus(this, false) == RV_CONTINUE) {
    // Restore focus to the main frame prior to loading new request.
    // This makes sure that we don't have a focused iframe. Otherwise, that
    // iframe would keep focus when the SetFocus called immediately after
    // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
    // for more details).
    // TODO(cef): The above comment may be wrong, or the below call to
    // setFocusedFrame() may be unnecessary or in the wrong place.  See this
    // thread for additional details:
    // http://groups.google.com/group/chromium-dev/browse_thread/thread/42bcd31b59e3a168
    GetWebView()->setFocusedFrame(frame);
    UIT_SetFocus(GetWebViewHost(), true);
  }

  return true;
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

  CefRefPtr<CefBrowserImpl> browser(new CefBrowserImpl(info, true, handler));
  browser->UIT_CreateBrowser(newUrl);

  return browser;
}

void CefBrowserImpl::UIT_Show(WebKit::WebNavigationPolicy policy)
{
  REQUIRE_UIT();
  delegate_->show(policy);
}

void CefBrowserImpl::UIT_HandleActionView(CefHandler::MenuId menuId)
{
  return UIT_HandleAction(menuId, NULL);
}

void CefBrowserImpl::UIT_HandleAction(CefHandler::MenuId menuId,
                                      CefFrame* frame)
{
  REQUIRE_UIT();

  WebFrame* web_frame = NULL;
  if(frame)
    web_frame = GetWebFrame(frame);

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
      GetWebView()->mainFrame()->stopLoading();
      break;
    case MENU_ID_UNDO:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("Undo"));
      break;
    case MENU_ID_REDO:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("Redo"));
      break;
    case MENU_ID_CUT:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("Cut"));
      break;
    case MENU_ID_COPY:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("Copy"));
      break;
    case MENU_ID_PASTE:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("Paste"));
      break;
    case MENU_ID_DELETE:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("Delete"));
      break;
    case MENU_ID_SELECTALL:
      if(web_frame)
        web_frame->executeCommand(WebString::fromUTF8("SelectAll"));
      break;
    case MENU_ID_PRINT:
      if(web_frame)
        UIT_PrintPages(web_frame);
      break;
    case MENU_ID_VIEWSOURCE:
      if(web_frame)
        UIT_ViewDocumentString(web_frame);
      break;
  }

  if(frame)
    frame->Release();
}


// CefFrameImpl

bool CefFrameImpl::IsFocused()
{
  return (browser_->GetWebFrame(this) ==
          browser_->GetWebView()->focusedFrame());
}
