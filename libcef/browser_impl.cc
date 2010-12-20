// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "request_impl.h"
#include "stream_impl.h"

#include "base/utf_string_conversions.h"
#include "base/waitable_event.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/plugins/webplugin_delegate.h"
#include "webkit/glue/plugins/webplugin_impl.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebHTTPBody;
using WebKit::WebPlugin;
using WebKit::WebPluginDocument;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using WebKit::WebView;

CefBrowserImpl::CefBrowserImpl(const CefWindowInfo& windowInfo,
                               const CefBrowserSettings& settings, bool popup,
                               CefRefPtr<CefHandler> handler)
  : window_info_(windowInfo), settings_(settings), is_popup_(popup),
    is_modal_(false), handler_(handler), webviewhost_(NULL), popuphost_(NULL),
    frame_main_(NULL), unique_id_(0)
{
  delegate_.reset(new BrowserWebViewDelegate(this));
  popup_delegate_.reset(new BrowserWebViewDelegate(this));
  nav_controller_.reset(new BrowserNavigationController(this));

  if (!file_system_root_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
    DCHECK(file_system_root_.path().empty());
  }
}


bool CefBrowserImpl::CanGoBack()
{
  if(!CefThread::CurrentlyOn(CefThread::UI))
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    base::WaitableEvent event(false, false);

    bool retVal = true;

    // Request the data from the UI thread
    CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_CanGoBackNotify, &retVal, &event));

    // Wait for the UI thread callback to tell us that the data is available
    event.Wait();

    return retVal;
  }
  else
  {
    // Call the method directly
    return UIT_CanGoBack();
  }
}

void CefBrowserImpl::GoBack()
{
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_BACK));
}

bool CefBrowserImpl::CanGoForward()
{
  if(!CefThread::CurrentlyOn(CefThread::UI))
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    base::WaitableEvent event(false, false);

    bool retVal = true;

    // Request the data from the UI thread
    CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_CanGoForwardNotify, &retVal, &event));

    // Wait for the UI thread callback to tell us that the data is available
    event.Wait();

    return retVal;
  }
  else
  {
    // Call the method directly
    return UIT_CanGoForward();
  }
}

void CefBrowserImpl::GoForward()
{
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_FORWARD));
}

void CefBrowserImpl::Reload()
{
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_RELOAD));
}

void CefBrowserImpl::ReloadIgnoreCache()
{
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_RELOAD_NOCACHE));
}

void CefBrowserImpl::StopLoad()
{
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleActionView, MENU_ID_NAV_STOP));
}

void CefBrowserImpl::SetFocus(bool enable)
{
  if (CefThread::CurrentlyOn(CefThread::UI))
  {
    UIT_SetFocus(GetWebViewHost(), enable);
  }
  else
  {
    CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_SetFocus, GetWebViewHost(), enable));
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
  return GetWebView() ? GetCefFrame(GetWebView()->mainFrame()) : NULL;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFocusedFrame()
{
  return GetWebView() ? GetCefFrame(GetWebView()->focusedFrame()) : NULL;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFrame(const CefString& name)
{
  WebView* view = GetWebView();
  if (!view)
    return NULL;

  WebFrame* frame = view->findFrameByName(string16(name));
  if(frame)
    return GetCefFrame(frame);
  return NULL;
}

void CefBrowserImpl::GetFrameNames(std::vector<CefString>& names)
{
  WebView* view = GetWebView();
  if (!view)
    return;

  WebFrame* main_frame = view->mainFrame();
  WebFrame* it = main_frame;
  do {
    if(it != main_frame) {
      string16 str = it->name();
      names.push_back(str);
    }
    it = it->traverseNext(true);
  } while (it != main_frame);
}

void CefBrowserImpl::Find(int identifier, const CefString& searchText,
                          bool forward, bool matchCase, bool findNext)
{
  WebKit::WebFindOptions options;
  options.forward = forward;
  options.matchCase = matchCase;
  options.findNext = findNext;

  // Execute the request on the UI thread.
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_Find, identifier, searchText, options));
}

void CefBrowserImpl::StopFinding(bool clearSelection)
{
  // Execute the request on the UI thread.
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_StopFinding, clearSelection));
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
        frame_main_ = new CefFrameImpl(this, CefString());
      cef_frame = frame_main_;
    } else {
      // Locate or create the appropriate named reference.
      CefString name = string16(frame->name());
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

void CefBrowserImpl::RemoveCefFrame(const CefString& name)
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
  WebView* view = GetWebView();
  if (!view)
    return NULL;

  CefString name = frame->GetName();
  if(name.empty())
    return view ->mainFrame();
  return view ->findFrameByName(string16(name));
}

void CefBrowserImpl::Undo(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_UNDO, frame.get()));
}

void CefBrowserImpl::Redo(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_REDO, frame.get()));
}

void CefBrowserImpl::Cut(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_CUT, frame.get()));
}

void CefBrowserImpl::Copy(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_COPY, frame.get()));
}

void CefBrowserImpl::Paste(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_PASTE, frame.get()));
}

void CefBrowserImpl::Delete(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_DELETE, frame.get()));
}

void CefBrowserImpl::SelectAll(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_SELECTALL, frame.get()));
}


void CefBrowserImpl::Print(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_PRINT, frame.get()));
}

void CefBrowserImpl::ViewSource(CefRefPtr<CefFrame> frame)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_HandleAction, MENU_ID_VIEWSOURCE, frame.get()));
}

CefString CefBrowserImpl::GetSource(CefRefPtr<CefFrame> frame)
{
  if(!CefThread::CurrentlyOn(CefThread::UI))
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    base::WaitableEvent event(false, false);

    CefRefPtr<CefStreamWriter> stream(new CefBytesWriter(BUFFER_SIZE));

    // Request the data from the UI thread
    frame->AddRef();
    stream->AddRef();
    CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_GetDocumentStringNotify, frame.get(), stream.get(),
        &event));

    // Wait for the UI thread callback to tell us that the data is available
    event.Wait();

    return static_cast<CefBytesWriter*>(stream.get())->GetDataString();
  }
  else
  {
    // Retrieve the document string directly
    WebKit::WebFrame* web_frame = GetWebFrame(frame);
    if(web_frame)
      return string16(web_frame->contentAsMarkup());
    return CefString();
  }
}

CefString CefBrowserImpl::GetText(CefRefPtr<CefFrame> frame)
{
  if(!CefThread::CurrentlyOn(CefThread::UI))
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    base::WaitableEvent event(false, false);

    CefRefPtr<CefStreamWriter> stream(new CefBytesWriter(BUFFER_SIZE));

    // Request the data from the UI thread
    frame->AddRef();
    stream->AddRef();
    CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_GetDocumentTextNotify, frame.get(), stream.get(),
        &event));

    // Wait for the UI thread callback to tell us that the data is available
    event.Wait();

    return static_cast<CefBytesWriter*>(stream.get())->GetDataString();
  }
  else
  {
    // Retrieve the document text directly
    WebKit::WebFrame* web_frame = GetWebFrame(frame);
    if(web_frame)
      return webkit_glue::DumpDocumentText(web_frame);
    return CefString();
  }
}

void CefBrowserImpl::LoadRequest(CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefRequest> request)
{
  DCHECK(request.get() != NULL);
  frame->AddRef();
  request->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadURLForRequestRef, frame.get(), request.get()));
}

void CefBrowserImpl::LoadURL(CefRefPtr<CefFrame> frame,
                             const CefString& url)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadURL, frame.get(), url));
}

void CefBrowserImpl::LoadString(CefRefPtr<CefFrame> frame,
                                const CefString& string,
                                const CefString& url)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadHTML, frame.get(), string, url));
}

void CefBrowserImpl::LoadStream(CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefStreamReader> stream,
                                const CefString& url)
{
  DCHECK(stream.get() != NULL);
  frame->AddRef();
  stream->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_LoadHTMLForStreamRef, frame.get(), stream.get(),
      url));
}

void CefBrowserImpl::ExecuteJavaScript(CefRefPtr<CefFrame> frame,
                                       const CefString& jsCode, 
                                       const CefString& scriptUrl,
                                       int startLine)
{
  frame->AddRef();
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(this,
      &CefBrowserImpl::UIT_ExecuteJavaScript, frame.get(), jsCode, scriptUrl,
      startLine));
}

CefString CefBrowserImpl::GetURL(CefRefPtr<CefFrame> frame)
{
  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame)
    return std::string(web_frame->url().spec());
  return CefString();
}

// static
bool CefBrowser::CreateBrowser(CefWindowInfo& windowInfo, bool popup,
                               CefRefPtr<CefHandler> handler,
                               const CefString& url)
{
  if(!_Context.get())
    return false;

  CefString newUrl = url;
  CefBrowserSettings settings(_Context->browser_defaults());

  if(handler.get())
  {
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    CefHandler::RetVal rv = handler->HandleBeforeCreated(NULL, windowInfo,
        popup, CefPopupFeatures(), handler, newUrl, settings);
    if(rv == RV_HANDLED)
      return false;
  }

  CefRefPtr<CefBrowserImpl> browser(
      new CefBrowserImpl(windowInfo, settings, popup, handler));
  CefThread::PostTask(CefThread::UI, FROM_HERE, NewRunnableMethod(browser.get(),
      &CefBrowserImpl::UIT_CreateBrowser, newUrl));
  return true;
}

// static
CefRefPtr<CefBrowser> CefBrowser::CreateBrowserSync(CefWindowInfo& windowInfo,
    bool popup, CefRefPtr<CefHandler> handler, const CefString& url)
{
  if(!_Context.get() || !CefThread::CurrentlyOn(CefThread::UI))
    return NULL;

  CefString newUrl = url;
  CefRefPtr<CefBrowser> alternateBrowser;
  CefBrowserSettings settings(_Context->browser_defaults());

  if(handler.get())
  {
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    CefHandler::RetVal rv = handler->HandleBeforeCreated(NULL, windowInfo,
        popup, CefPopupFeatures(), handler, newUrl, settings);
    if(rv == RV_HANDLED)
      return false;
  }

  CefRefPtr<CefBrowser> browser(
      new CefBrowserImpl(windowInfo, settings, popup, handler));
  static_cast<CefBrowserImpl*>(browser.get())->UIT_CreateBrowser(newUrl);

  return browser;
}

void CefBrowserImpl::UIT_DestroyBrowser()
{
  if(handler_.get()) {
    // Notify the handler that the window is about to be closed.
    handler_->HandleBeforeWindowClose(this);
  }
  GetWebViewDelegate()->RevokeDragDrop();
  
  // Clean up anything associated with the WebViewHost widget.
  GetWebViewHost()->webwidget()->close();
  webviewhost_.reset();
  
  // Remove the reference added in UIT_CreateBrowser().
  Release();
  
  // Remove the browser from the list maintained by the context.
  _Context->RemoveBrowser(this);
}

void CefBrowserImpl::UIT_LoadURL(CefFrame* frame,
                                 const CefString& url)
{
  UIT_LoadURLForRequest(frame, url, CefString(), WebHTTPBody(),
      CefRequest::HeaderMap());
}

void CefBrowserImpl::UIT_LoadURLForRequestRef(CefFrame* frame,
                                              CefRequest* request)
{
  CefString url = request->GetURL();
  CefString method = request->GetMethod();

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
                                         const CefString& url,
                                         const CefString& method,
                                         const WebKit::WebHTTPBody& upload_data,
                                         const CefRequest::HeaderMap& headers)
{
  REQUIRE_UIT();
  
  if (url.empty())
      return;

  std::string urlStr(url);
  GURL gurl = GURL(urlStr);

  if (!gurl.is_valid() && !gurl.has_scheme()) {
    // Try to add "http://" at the beginning
    std::string new_url = std::string("http://") + urlStr;
    gurl = GURL(new_url);
    if (!gurl.is_valid())
      return;
  }

  nav_controller_->LoadEntry(
      new BrowserNavigationEntry(-1, gurl, CefString(), frame->GetName(),
          method, upload_data, headers));

  frame->Release();
}

void CefBrowserImpl::UIT_LoadHTML(CefFrame* frame,
                                  const CefString& html,
                                  const CefString& url)
{
  REQUIRE_UIT();

  std::string urlStr(url);
  GURL gurl = GURL(urlStr);

  if (!gurl.is_valid() && !gurl.has_scheme()) {
    // Try to add "http://" at the beginning
    std::string new_url = std::string("http://") + urlStr;
    gurl = GURL(new_url);
    if (!gurl.is_valid())
      return;
  }

  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame)
    web_frame->loadHTMLString(std::string(html), gurl);

  frame->Release();
}

void CefBrowserImpl::UIT_LoadHTMLForStreamRef(CefFrame* frame,
                                              CefStreamReader* stream,
                                              const CefString& url)
{
  REQUIRE_UIT();
  
  std::string urlStr(url);
  GURL gurl = GURL(urlStr);

  if (!gurl.is_valid() && !gurl.has_scheme()) {
    // Try to add "http://" at the beginning
    std::string new_url = std::string("http://") + urlStr;
    gurl = GURL(new_url);
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
                                           const CefString& js_code, 
                                           const CefString& script_url,
                                           int start_line)
{
  REQUIRE_UIT();

  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    web_frame->executeScript(WebScriptSource(string16(js_code),
        WebURL(GURL(std::string(script_url))), start_line));
  }

  frame->Release();
}

void CefBrowserImpl::UIT_GoBackOrForward(int offset)
{
  REQUIRE_UIT();
  nav_controller_->GoToOffset(offset);
}

void CefBrowserImpl::UIT_Reload(bool ignoreCache)
{
  REQUIRE_UIT();
  nav_controller_->Reload(ignoreCache);
}

bool CefBrowserImpl::UIT_Navigate(const BrowserNavigationEntry& entry,
                                  bool reload,
                                  bool ignoreCache)
{
  REQUIRE_UIT();

  WebView* view = GetWebView();
  if (!view)
    return false;

  // Get the right target frame for the entry.
  WebFrame* frame;
  if (!entry.GetTargetFrame().empty())
    frame = view->findFrameByName(string16(entry.GetTargetFrame()));
  else
    frame = view->mainFrame();

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
    frame->reload(ignoreCache);
  } else if (!entry.GetContentState().empty()) {
    DCHECK(entry.GetPageID() != -1);
    frame->loadHistoryItem(
        webkit_glue::HistoryItemFromString(entry.GetContentState()));
  } else {
    DCHECK(entry.GetPageID() == -1);
    WebURLRequest request(entry.GetURL());

    if(entry.GetMethod().length() > 0)
      request.setHTTPMethod(string16(entry.GetMethod()));

    if(entry.GetHeaders().size() > 0)
      CefRequestImpl::SetHeaderMap(entry.GetHeaders(), request);

    if(!entry.GetUploadData().isNull())
    {
      string16 method = request.httpMethod();
      if(method == ASCIIToUTF16("GET") || method == ASCIIToUTF16("HEAD"))
        request.setHTTPMethod(ASCIIToUTF16("POST"));

      if(request.httpHeaderField(ASCIIToUTF16("Content-Type")).length() == 0) {
        request.setHTTPHeaderField(
            ASCIIToUTF16("Content-Type"),
            ASCIIToUTF16("application/x-www-form-urlencoded"));
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
    view->setFocusedFrame(frame);
    UIT_SetFocus(GetWebViewHost(), true);
  }

  return true;
}

CefRefPtr<CefBrowserImpl> CefBrowserImpl::UIT_CreatePopupWindow(
    const CefString& url, const CefPopupFeatures& features)
{
  REQUIRE_UIT();
    
  CefWindowInfo info;
#if defined(OS_WIN)
  info.SetAsPopup(NULL, CefString());
#endif
  CefRefPtr<CefHandler> handler = handler_;
  CefString newUrl = url;

  // Start with the current browser window's settings.
  CefBrowserSettings settings(settings_);

  if(handler_.get())
  {
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    CefHandler::RetVal rv = handler_->HandleBeforeCreated(this, info, true,
        features, handler, newUrl, settings);
    if(rv == RV_HANDLED)
      return NULL;
  }

  CefRefPtr<CefBrowserImpl> browser(
      new CefBrowserImpl(info, settings, true, handler));
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
      UIT_Reload(false);
      break;
    case MENU_ID_NAV_RELOAD_NOCACHE:
      UIT_Reload(true);
      break;
    case MENU_ID_NAV_STOP:
      if (GetWebView()) 
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

void CefBrowserImpl::UIT_GetDocumentStringNotify(CefFrame* frame,
                                                 CefStreamWriter* writer,
                                                 base::WaitableEvent* event)
{
  REQUIRE_UIT();
  
  WebKit::WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    // Retrieve the document string
    std::string markup = web_frame->contentAsMarkup().utf8();
    // Write the document string to the stream
    writer->Write(markup.c_str(), markup.size(), 1);
  }
  
  // Notify the calling thread that the data is now available
  event->Signal();

  writer->Release();
  frame->Release();
}

void CefBrowserImpl::UIT_GetDocumentTextNotify(CefFrame* frame,
                                               CefStreamWriter* writer,
                                               base::WaitableEvent* event)
{
  REQUIRE_UIT();
  
  WebKit::WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    // Retrieve the document string
    string16 wstr = webkit_glue::DumpDocumentText(web_frame);
    std::string str;
    UTF16ToUTF8(wstr.c_str(), wstr.length(), &str);
    // Write the document string to the stream
    writer->Write(str.c_str(), str.size(), 1);
  }
  
  // Notify the calling thread that the data is now available
  event->Signal();

  writer->Release();
  frame->Release();
}

void CefBrowserImpl::UIT_CanGoBackNotify(bool *retVal,
                                         base::WaitableEvent* event)
{
  REQUIRE_UIT();
  
  *retVal = UIT_CanGoBack();

  // Notify the calling thread that the data is now available
  event->Signal();
}

void CefBrowserImpl::UIT_CanGoForwardNotify(bool *retVal,
                                            base::WaitableEvent* event)
{
  REQUIRE_UIT();
  
  *retVal = UIT_CanGoForward();

  // Notify the calling thread that the data is now available
  event->Signal();
}

void CefBrowserImpl::UIT_Find(int identifier, const CefString& search_text,
                              const WebKit::WebFindOptions& options)
{
  WebView* view = GetWebView();
  if (!view)
    return;

  WebFrame* main_frame = view->mainFrame();
  string16 searchText(search_text);

  if (main_frame->document().isPluginDocument()) {
    WebPlugin* plugin = main_frame->document().to<WebPluginDocument>().plugin();
    webkit_glue::WebPluginDelegate* delegate =
        static_cast<webkit_glue::WebPluginImpl*>(plugin)->delegate();
    if (options.findNext) {
      // Just navigate back/forward.
      delegate->SelectFindResult(options.forward);
    } else {
      if (delegate->StartFind(searchText, options.matchCase, identifier)) {
      } else {
        // No find results.
        UIT_NotifyFindStatus(identifier, 0, gfx::Rect(), 0, true);
      }
    }
    return;
  }

  WebFrame* frame_after_main = main_frame->traverseNext(true);
  WebFrame* focused_frame = view->focusedFrame();
  WebFrame* search_frame = focused_frame;  // start searching focused frame.

  bool multi_frame = (frame_after_main != main_frame);

  // If we have multiple frames, we don't want to wrap the search within the
  // frame, so we check here if we only have main_frame in the chain.
  bool wrap_within_frame = !multi_frame;

  WebRect selection_rect;
  bool result = false;

  // If something is selected when we start searching it means we cannot just
  // increment the current match ordinal; we need to re-generate it.
  WebRange current_selection = focused_frame->selectionRange();

  do {
    result = search_frame->find(identifier, searchText, options,
        wrap_within_frame, &selection_rect);

    if (!result) {
      // don't leave text selected as you move to the next frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"));

      // Find the next frame, but skip the invisible ones.
      do {
        // What is the next frame to search? (we might be going backwards). Note
        // that we specify wrap=true so that search_frame never becomes NULL.
        search_frame = options.forward ?
            search_frame->traverseNext(true) :
            search_frame->traversePrevious(true);
      } while (!search_frame->hasVisibleContent() &&
               search_frame != focused_frame);

      // Make sure selection doesn't affect the search operation in new frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"));

      // If we have multiple frames and we have wrapped back around to the
      // focused frame, we need to search it once more allowing wrap within
      // the frame, otherwise it will report 'no match' if the focused frame has
      // reported matches, but no frames after the focused_frame contain a
      // match for the search word(s).
      if (multi_frame && search_frame == focused_frame) {
        result = search_frame->find(
            identifier, searchText,
            options, true,  // Force wrapping.
            &selection_rect);
      }
    }

    view->setFocusedFrame(search_frame);
  } while (!result && search_frame != focused_frame);

  if (options.findNext && current_selection.isNull()) {
    // Force the main_frame to report the actual count.
    main_frame->increaseMatchCount(0, identifier);
  } else {
    // If nothing is found, set result to "0 of 0", otherwise, set it to
    // "-1 of 1" to indicate that we found at least one item, but we don't know
    // yet what is active.
    int ordinal = result ? -1 : 0;  // -1 here means, we might know more later.
    int match_count = result ? 1 : 0;  // 1 here means possibly more coming.

    // If we find no matches then this will be our last status update.
    // Otherwise the scoping effort will send more results.
    bool final_status_update = !result;

    // Send the search result.
    UIT_NotifyFindStatus(identifier, match_count, selection_rect, ordinal,
        final_status_update);

    // Scoping effort begins, starting with the mainframe.
    search_frame = main_frame;

    main_frame->resetMatchCount();

    do {
      // Cancel all old scoping requests before starting a new one.
      search_frame->cancelPendingScopingEffort();

      // We don't start another scoping effort unless at least one match has
      // been found.
      if (result) {
        // Start new scoping request. If the scoping function determines that it
        // needs to scope, it will defer until later.
        search_frame->scopeStringMatches(identifier, searchText, options,
                                         true);  // reset the tickmarks
      }

      // Iterate to the next frame. The frame will not necessarily scope, for
      // example if it is not visible.
      search_frame = search_frame->traverseNext(true);
    } while (search_frame != main_frame);
  }
}

void CefBrowserImpl::UIT_StopFinding(bool clear_selection)
{
  WebView* view = GetWebView();
  if (!view)
    return;

  WebDocument doc = view->mainFrame()->document();
  if (doc.isPluginDocument()) {
    WebPlugin* plugin = view->mainFrame()->document().
        to<WebPluginDocument>().plugin();
    webkit_glue::WebPluginDelegate* delegate =
        static_cast<webkit_glue::WebPluginImpl*>(plugin)->delegate();
    delegate->StopFind();
    return;
  }

  if (clear_selection)
    view->focusedFrame()->executeCommand(WebString::fromUTF8("Unselect"));

  WebFrame* frame = view->mainFrame();
  while (frame) {
    frame->stopFinding(clear_selection);
    frame = frame->traverseNext(false);
  }
}

void CefBrowserImpl::UIT_NotifyFindStatus(int identifier, int count,
                                          const WebKit::WebRect& selection_rect,
                                          int active_match_ordinal,
                                          bool final_update)
{
  if(handler_.get())
  {
    CefRect rect(selection_rect.x, selection_rect.y, selection_rect.width,
        selection_rect.height);
    handler_->HandleFindResult(this, identifier, count, rect,
        active_match_ordinal, final_update);
  }
}


// CefFrameImpl

bool CefFrameImpl::IsFocused()
{
  return (browser_->GetWebView() &&
         (browser_->GetWebFrame(this) ==
            browser_->GetWebView()->focusedFrame()));
}
