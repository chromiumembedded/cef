// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "precompiled_libcef.h"
#include "config.h"
#include "browser_webview_delegate.h"
#include "browser_impl.h"
#include "browser_navigation_controller.h"
#include "context.h"
#include "request_impl.h"
#include "v8_impl.h"

#include "base/file_util.h"
#include "base/gfx/gdi_util.h"
#include "base/gfx/point.h"
#include "base/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/webappcachecontext.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/window_open_disposition.h"
#include "browser_webkit_glue.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
#include "browser_drag_delegate.h"
#include "browser_drop_delegate.h"
#endif

using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDragData;
using WebKit::WebHistoryItem;
using WebKit::WebNavigationPolicy;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebWidget;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

namespace {

int next_page_id_ = 1;

}  // namespace

// WebViewDelegate -----------------------------------------------------------

WebView* BrowserWebViewDelegate::CreateWebView(WebView* webview,
                                               bool user_gesture,
                                               const GURL& creator_url) {
  CefRefPtr<CefBrowserImpl> browser =
      browser_->UIT_CreatePopupWindow(std::wstring());
  return browser.get() ? browser->GetWebView() : NULL;
}

WebKit::WebWidget* BrowserWebViewDelegate::CreatePopupWidget(
    WebView* webview,
    bool activatable) {
  return browser_->UIT_CreatePopupWidget(webview);
}

WebKit::WebMediaPlayer* BrowserWebViewDelegate::CreateWebMediaPlayer(
    WebKit::WebMediaPlayerClient* client) {
  scoped_refptr<media::FilterFactoryCollection> factory =
      new media::FilterFactoryCollection();

  // TODO(hclam): this is the same piece of code as in RenderView, maybe they
  // should be grouped together.
  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory =
      new webkit_glue::MediaResourceLoaderBridgeFactory(
          GURL::EmptyGURL(),  // referrer
          "null",             // frame origin
          "null",             // main_frame_origin
          base::GetCurrentProcId(),
          WebAppCacheContext::kNoAppCacheContextId,
          0);
  factory->AddFactory(webkit_glue::BufferedDataSource::CreateFactory(
      MessageLoop::current(), bridge_factory));
  return new webkit_glue::WebMediaPlayerImpl(client, factory);
}

WebWorker* BrowserWebViewDelegate::CreateWebWorker(WebWorkerClient* client) {
  return NULL;
}

void BrowserWebViewDelegate::OpenURL(WebView* webview, const GURL& url,
                                     const GURL& referrer,
                                     WebNavigationPolicy policy) {
  DCHECK_NE(policy, WebKit::WebNavigationPolicyCurrentTab);
  CefRefPtr<CefBrowserImpl> browser =
    browser_->UIT_CreatePopupWindow(UTF8ToWide(url.spec()));

  if(browser.get())
    browser->UIT_Show(policy);
}

void BrowserWebViewDelegate::DidStartLoading(WebView* webview) {
  // clear the title so we can tell if it wasn't provided by the page
  browser_->UIT_SetTitle(std::wstring());

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has started
    handler->HandleLoadStart(browser_, NULL);
  }
}

void BrowserWebViewDelegate::DidStopLoading(WebView* webview) {
  if(browser_->UIT_GetTitle().empty()) {
    // no title was provided by the page, so send a blank string to the client
    CefRefPtr<CefHandler> handler = browser_->GetHandler();
    if(handler.get()) {
      // Notify the handler of a page title change
      handler->HandleTitleChange(browser_, browser_->UIT_GetTitle());
    }
  }
  
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has ended
    handler->HandleLoadEnd(browser_, NULL);
  }
}

void BrowserWebViewDelegate::WindowObjectCleared(WebFrame* webframe) {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = webkit_glue::GetV8Context(webframe);
    if(context.IsEmpty())
      return;

    v8::Context::Scope scope(context);

    CefRefPtr<CefFrame> frame(browser_->GetCefFrame(webframe));
    CefRefPtr<CefV8Value> object = new CefV8ValueImpl(context->Global());
    handler->HandleJSBinding(browser_, frame, object);
  }
}

WebNavigationPolicy BrowserWebViewDelegate::PolicyForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationType type,
    WebNavigationPolicy default_policy,
    bool is_redirect) {
  
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Gather browse request information
    CefRefPtr<CefRequest> req(CefRequest::CreateRequest());
    
    GURL request_url = request.url();
    req->SetURL(UTF8ToWide(request_url.spec()));
    req->SetMethod(
        UTF8ToWide(webkit_glue::WebStringToStdString(request.httpMethod())));

    const WebKit::WebHTTPBody& httpBody = request.httpBody();
    if(!httpBody.isNull()) {
      CefRefPtr<CefPostData> postdata(CefPostData::CreatePostData());
      static_cast<CefPostDataImpl*>(postdata.get())->Set(httpBody);
      req->SetPostData(postdata);
    }

    CefRequest::HeaderMap map;
    CefRequestImpl::GetHeaderMap(request, map);
    if(map.size() > 0)
      static_cast<CefRequestImpl*>(req.get())->SetHeaderMap(map);

    // Notify the handler of a browse request
    CefHandler::RetVal rv = handler->HandleBeforeBrowse(browser_,
        browser_->GetCefFrame(frame), req, (CefHandler::NavType)type,
        is_redirect);
    if(rv == RV_HANDLED)
      return WebKit::WebNavigationPolicyIgnore;
    }

  WebNavigationPolicy result;
  if (policy_delegate_enabled_) {
    std::wstring frame_name = frame->GetName();
    std::string url_description;
    GURL request_url = request.url();
    if (request_url.SchemeIs("file")) {
      url_description = request_url.ExtractFileName();
    } else {
      url_description = request_url.spec();
    }
    if (policy_delegate_is_permissive_) {
      result = WebKit::WebNavigationPolicyCurrentTab;
    } else {
      result = WebKit::WebNavigationPolicyIgnore;
    }
  } else {
    result = default_policy;
  }
  return result;
}

void BrowserWebViewDelegate::AssignIdentifierToRequest(WebView* webview,
                                                uint32 identifier,
                                                const WebURLRequest& request) {
}

void BrowserWebViewDelegate::WillSendRequest(WebView* webview,
                                             uint32 identifier,
                                             WebURLRequest* request) {
  // The requestor ID is used by the resource loader bridge to locate the
  // browser that originated the request.
  request->setRequestorID(browser_->UIT_GetUniqueID());
}

void BrowserWebViewDelegate::DidFinishLoading(WebView* webview,
                                           uint32 identifier) {
  
}

void BrowserWebViewDelegate::DidFailLoadingWithError(WebView* webview,
                                                  uint32 identifier,
                                                  const WebURLError& error) {
  
}

void BrowserWebViewDelegate::DidCreateDataSource(WebFrame* frame,
                                                 WebDataSource* ds) {
  ds->setExtraData(pending_extra_data_.release());
}

void BrowserWebViewDelegate::DidStartProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame,
    NavigationGesture gesture) {
  if (!top_loading_frame_) {
    top_loading_frame_ = frame;
  }
  UpdateAddressBar(webview);
}

void BrowserWebViewDelegate::DidReceiveServerRedirectForProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame) {
  UpdateAddressBar(webview);
}

void BrowserWebViewDelegate::DidFailProvisionalLoadWithError(
    WebView* webview,
    const WebURLError& error,
    WebFrame* frame) {
  LocationChangeDone(frame);

  // error codes are defined in net\base\net_error_list.h

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.reason == net::ERR_ABORTED)
    return;

  const WebDataSource* failed_ds = frame->GetProvisionalDataSource();
  BrowserExtraData* extra_data =
      static_cast<BrowserExtraData*>(failed_ds->extraData());
  bool replace = extra_data && extra_data->pending_page_id != -1;

  std::string error_text;

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // give the handler an opportunity to generate a custom error message
    std::wstring error_str;
    CefHandler::RetVal rv = handler->HandleLoadError(browser_,
        browser_->GetCefFrame(frame),
        static_cast<CefHandler::ErrorCode>(error.reason),
        UTF8ToWide(failed_ds->request().url().spec().data()), error_str);
    if(rv == RV_HANDLED && !error_str.empty())
      error_text = WideToUTF8(error_str);
  } else {
    error_text = StringPrintf("Error %d when loading url %s", error.reason,
        failed_ds->request().url().spec().data());
  }

  // Make sure we never show errors in view source mode.
  frame->SetInViewSourceMode(false);

  frame->LoadHTMLString(
      error_text, GURL("testshell-error:"), error.unreachableURL, replace);
}

void BrowserWebViewDelegate::DidCommitLoadForFrame(WebView* webview,
                                                WebFrame* frame,
                                                bool is_new_navigation) {
  UpdateForCommittedLoad(frame, is_new_navigation);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has started
    handler->HandleLoadStart(browser_, browser_->GetCefFrame(frame));
  }
}

void BrowserWebViewDelegate::DidReceiveTitle(WebView* webview,
                                          const std::wstring& title,
                                          WebFrame* frame) {
  browser_->UIT_SetTitle(title);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler of a page title change
    handler->HandleTitleChange(browser_, title);
  }
}

void BrowserWebViewDelegate::DidFinishLoadForFrame(WebView* webview,
                                                WebFrame* frame) {
  UpdateAddressBar(webview);
  LocationChangeDone(frame);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has ended
    handler->HandleLoadEnd(browser_, browser_->GetCefFrame(frame));
  }
}

void BrowserWebViewDelegate::DidFailLoadWithError(WebView* webview,
                                                  const WebURLError& error,
                                                  WebFrame* frame) {
  LocationChangeDone(frame);
}

void BrowserWebViewDelegate::DidFinishDocumentLoadForFrame(WebView* webview,
                                                        WebFrame* frame) {
  
}

void BrowserWebViewDelegate::DidHandleOnloadEventsForFrame(WebView* webview,
                                                        WebFrame* frame) {
  
}

void BrowserWebViewDelegate::DidChangeLocationWithinPageForFrame(
    WebView* webview, WebFrame* frame, bool is_new_navigation) {
  frame->GetDataSource()->setExtraData(pending_extra_data_.release());
  UpdateForCommittedLoad(frame, is_new_navigation);
}

void BrowserWebViewDelegate::DidReceiveIconForFrame(WebView* webview,
                                                 WebFrame* frame) {
  
}

void BrowserWebViewDelegate::WillPerformClientRedirect(WebView* webview,
                                                    WebFrame* frame,
                                                    const std::wstring& dest_url,
                                                    unsigned int delay_seconds,
                                                    unsigned int fire_date) {
  
}

void BrowserWebViewDelegate::DidCancelClientRedirect(WebView* webview,
                                                  WebFrame* frame) {
  
}

void BrowserWebViewDelegate::AddMessageToConsole(WebView* webview,
                                              const std::wstring& message,
                                              unsigned int line_no,
                                              const std::wstring& source_id) {
  logging::LogMessage("CONSOLE", 0).stream() << "\"" 
                                             << message.c_str() 
                                             << ",\" source: " 
                                             << source_id.c_str() 
                                             << "(" 
                                             << line_no 
                                             << ")";
}

void BrowserWebViewDelegate::RunJavaScriptAlert(WebFrame* webframe,
                                                const std::wstring& message) {
  CefHandler::RetVal rv = RV_CONTINUE;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSAlert(browser_, browser_->GetCefFrame(webframe),
        message);
  }
  if(rv != RV_HANDLED)
    ShowJavaScriptAlert(webframe, message);
}

bool BrowserWebViewDelegate::RunJavaScriptConfirm(WebFrame* webframe,
                                               const std::wstring& message) {
  CefHandler::RetVal rv = RV_CONTINUE;
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSConfirm(browser_, browser_->GetCefFrame(webframe),
        message, retval);
  }
  if(rv != RV_HANDLED)
    retval = ShowJavaScriptConfirm(webframe, message);
  return retval;
}

bool BrowserWebViewDelegate::RunJavaScriptPrompt(WebFrame* webframe,
    const std::wstring& message, const std::wstring& default_value,
    std::wstring* result) {
  CefHandler::RetVal rv = RV_CONTINUE;
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSPrompt(browser_, browser_->GetCefFrame(webframe),
        message, default_value, retval, *result);
  }
  if(rv != RV_HANDLED)
    retval = ShowJavaScriptPrompt(webframe, message, default_value, result);
  return retval;
}

void BrowserWebViewDelegate::SetStatusbarText(WebView* webview,
                                              const std::wstring& message) {
  
}

void BrowserWebViewDelegate::StartDragging(WebView* webview,
                                        const WebDragData& drag_data) {
#if defined(OS_WIN)
  // TODO(port): make this work on all platforms.
  if (!drag_delegate_) {
    drag_delegate_ = new BrowserDragDelegate(
        browser_->GetWebViewWndHandle(),
        browser_->GetWebView());
  }
  // TODO(tc): Drag and drop is disabled in the test shell because we need
  // to be able to convert from WebDragData to an IDataObject.
  //const DWORD ok_effect = DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE;
  //DWORD effect;
  //HRESULT res = DoDragDrop(drag_data.data_object, drag_delegate_.get(),
  //                         ok_effect, &effect);
  //DCHECK(DRAGDROP_S_DROP == res || DRAGDROP_S_CANCEL == res);
  webview->DragSourceSystemDragEnded();
#endif
}

// The output from these methods in non-interactive mode should match that
// expected by the layout tests.  See EditingDelegate.m in DumpRenderTree.
bool BrowserWebViewDelegate::ShouldBeginEditing(WebView* webview, 
                                             std::wstring range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::ShouldEndEditing(WebView* webview, 
                                           std::wstring range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::ShouldInsertNode(WebView* webview, 
                                           std::wstring node, 
                                           std::wstring range,
                                           std::wstring action) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::ShouldInsertText(WebView* webview, 
                                           std::wstring text, 
                                           std::wstring range,
                                           std::wstring action) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::ShouldChangeSelectedRange(WebView* webview, 
                                                    std::wstring fromRange, 
                                                    std::wstring toRange, 
                                                    std::wstring affinity, 
                                                    bool stillSelecting) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::ShouldDeleteRange(WebView* webview, 
                                            std::wstring range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::ShouldApplyStyle(WebView* webview, 
                                           std::wstring style,
                                           std::wstring range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::SmartInsertDeleteEnabled() {
  return true;
}

void BrowserWebViewDelegate::DidBeginEditing() { 
  
}

void BrowserWebViewDelegate::DidChangeSelection() {
  
}

void BrowserWebViewDelegate::DidChangeContents() {
  
}

void BrowserWebViewDelegate::DidEndEditing() {
  
}

void BrowserWebViewDelegate::NavigateBackForwardSoon(int offset) {
  browser_->UIT_GetNavigationController()->GoToOffset(offset);
}

int BrowserWebViewDelegate::GetHistoryBackListCount() {
  int current_index =
      browser_->UIT_GetNavigationController()->GetLastCommittedEntryIndex();
  return current_index;
}

int BrowserWebViewDelegate::GetHistoryForwardListCount() {
  int current_index =
      browser_->UIT_GetNavigationController()->GetLastCommittedEntryIndex();
  return browser_->UIT_GetNavigationController()->GetEntryCount()
      - current_index - 1;
}

void BrowserWebViewDelegate::TakeFocus(WebView* webview, bool reverse) {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that it should take a focus
    handler->HandleTakeFocus(browser_, reverse);
  }
}

void BrowserWebViewDelegate::SetUserStyleSheetEnabled(bool is_enabled) {
  WebPreferences* prefs = _Context->GetWebPreferences();
  prefs->user_style_sheet_enabled = is_enabled;
  browser_->GetWebView()->SetPreferences(*prefs);
}

void BrowserWebViewDelegate::SetUserStyleSheetLocation(const GURL& location) {
  WebPreferences* prefs = _Context->GetWebPreferences();
  prefs->user_style_sheet_enabled = true;
  prefs->user_style_sheet_location = location;
  browser_->GetWebView()->SetPreferences(*prefs);
}

void BrowserWebViewDelegate::SetSmartInsertDeleteEnabled(bool enabled) {
  smart_insert_delete_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void BrowserWebViewDelegate::SetSelectTrailingWhitespaceEnabled(bool enabled) {
  select_trailing_whitespace_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void BrowserWebViewDelegate::RegisterDragDrop() {
#if defined(OS_WIN)
  // TODO(port): add me once drag and drop works.
  DCHECK(!drop_delegate_);
  drop_delegate_ = new BrowserDropDelegate(browser_->GetWebViewWndHandle(),
                                           browser_->GetWebView());
#endif
}

void BrowserWebViewDelegate::SetCustomPolicyDelegate(bool is_custom,
                                                  bool is_permissive) {
  policy_delegate_enabled_ = is_custom;
  policy_delegate_is_permissive_ = is_permissive;
}

void BrowserWebViewDelegate::WaitForPolicyDelegate() {
  policy_delegate_enabled_ = true;
}

// WebWidgetClient ---------------------------------------------------------

void BrowserWebViewDelegate::didInvalidateRect(const WebRect& rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->DidInvalidateRect(rect);
}

void BrowserWebViewDelegate::didScrollRect(int dx, int dy,
                                           const WebRect& clip_rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->DidScrollRect(dx, dy, clip_rect);
}

void BrowserWebViewDelegate::didFocus() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    CefRefPtr<CefHandler> handler = browser_->GetHandler();
    if (handler.get() && handler->HandleSetFocus(browser_, true) == RV_CONTINUE)
      browser_->UIT_SetFocus(host, true);
  }
}

void BrowserWebViewDelegate::didBlur() {
  if (WebWidgetHost* host = GetWidgetHost())
    browser_->UIT_SetFocus(host, false);
}

WebScreenInfo BrowserWebViewDelegate::screenInfo() {
  if (WebWidgetHost* host = GetWidgetHost())
    return host->GetScreenInfo();

  return WebScreenInfo();
}

// Private methods -----------------------------------------------------------

void BrowserWebViewDelegate::UpdateAddressBar(WebView* webView) {
/*
  WebFrame* mainFrame = webView->UIT_GetMainFrame();
  WebDataSource* dataSource = mainFrame->GetDataSource();
  if (!dataSource)
    dataSource = mainFrame->GetProvisionalDataSource();
  if (!dataSource)
    return;

  GURL gUrl = dataSource->request().firstPartyForCookies();
*/
}

void BrowserWebViewDelegate::LocationChangeDone(WebFrame* frame) {
  if (frame == top_loading_frame_)
    top_loading_frame_ = NULL;
}

WebWidgetHost* BrowserWebViewDelegate::GetWidgetHost() {
  if (this == browser_->GetWebViewDelegate())
    return browser_->GetWebViewHost();
  if (this == browser_->GetPopupDelegate())
    return browser_->GetPopupHost();
  return NULL;
}

void BrowserWebViewDelegate::UpdateForCommittedLoad(WebFrame* frame,
                                                 bool is_new_navigation) {
  WebView* webview = browser_->GetWebView();

  // Code duplicated from RenderView::DidCommitLoadForFrame.
  BrowserExtraData* extra_data = static_cast<BrowserExtraData*>(
      frame->GetDataSource()->extraData());

  if (is_new_navigation) {
    // New navigation.
    UpdateSessionHistory(frame);
    page_id_ = next_page_id_++;
  } else if (extra_data && extra_data->pending_page_id != -1 &&
             !extra_data->request_committed) {
    // This is a successful session history navigation!
    UpdateSessionHistory(frame);
    page_id_ = extra_data->pending_page_id;
  }

  // Don't update session history multiple times.
  if (extra_data)
    extra_data->request_committed = true;

  UpdateURL(frame);
}

void BrowserWebViewDelegate::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->GetDataSource();
  DCHECK(ds);

  const WebURLRequest& request = ds->request();

  // Type is unused.
  scoped_ptr<BrowserNavigationEntry> entry(new BrowserNavigationEntry);

  // Bug 654101: the referrer will be empty on https->http transitions. It
  // would be nice if we could get the real referrer from somewhere.
  entry->SetPageID(page_id_);
  if (ds->hasUnreachableURL()) {
    entry->SetURL(ds->unreachableURL());
  } else {
    entry->SetURL(request.url());
  }

  std::wstring url = UTF8ToWide(entry->GetURL().spec().c_str());

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler of an address change
    handler->HandleAddressChange(browser_, browser_->GetCefFrame(frame), url);
  }

  const WebHistoryItem& history_item = frame->GetCurrentHistoryItem();
  if (!history_item.isNull())
    entry->SetContentState(webkit_glue::HistoryItemToString(history_item));

  browser_->UIT_GetNavigationController()->DidNavigateToEntry(entry.release());

  last_page_id_updated_ = std::max(last_page_id_updated_, page_id_);
}

void BrowserWebViewDelegate::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  BrowserNavigationEntry* entry = static_cast<BrowserNavigationEntry*>(
      browser_->UIT_GetNavigationController()->GetEntryWithPageID(page_id_));
  if (!entry)
    return;

  const WebHistoryItem& history_item =
      browser_->GetWebView()->GetMainFrame()->GetPreviousHistoryItem();
  if (history_item.isNull())
    return;

  entry->SetContentState(webkit_glue::HistoryItemToString(history_item));
}

std::wstring BrowserWebViewDelegate::GetFrameDescription(WebFrame* webframe) {
  std::wstring name = webframe->GetName();

  if (webframe == browser_->GetWebView()->GetMainFrame()) {
    if (name.length())
      return L"main frame \"" + name + L"\"";
    else
      return L"main frame";
  } else {
    if (name.length())
      return L"frame \"" + name + L"\"";
    else
      return L"frame (anonymous)";
  }
}
