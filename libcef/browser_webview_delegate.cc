// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "precompiled_libcef.h"
#include "browser_webview_delegate.h"
#include "browser_impl.h"
#include "browser_navigation_controller.h"
#include "context.h"
#include "request_impl.h"

#include "base/file_util.h"
#include "base/gfx/gdi_util.h"
#include "base/gfx/point.h"
#include "base/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/window_open_disposition.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
#include "browser_drag_delegate.h"
#include "browser_drop_delegate.h"
#endif

namespace {

int next_page_id_ = 1;

}  // namespace

// WebViewDelegate -----------------------------------------------------------

WebView* BrowserWebViewDelegate::CreateWebView(WebView* webview,
                                               bool user_gesture) {
  CefRefPtr<CefBrowserImpl> browser =
      browser_->UIT_CreatePopupWindow(std::wstring());
  return browser.get() ? browser->UIT_GetWebView() : NULL;
}

WebWidget* BrowserWebViewDelegate::CreatePopupWidget(WebView* webview,
                                                     bool activatable) {
  return browser_->UIT_CreatePopupWidget(webview);
}

void BrowserWebViewDelegate::OpenURL(WebView* webview, const GURL& url,
                                     const GURL& referrer,
                                     WindowOpenDisposition disposition) {
  DCHECK_NE(disposition, CURRENT_TAB);  // No code for this
  if (disposition == SUPPRESS_OPEN)
    return;
  
  CefRefPtr<CefBrowserImpl> browser =
    browser_->UIT_CreatePopupWindow(UTF8ToWide(url.spec()));

  if(browser.get())
    browser->UIT_Show(browser->UIT_GetWebView(), disposition);
}

void BrowserWebViewDelegate::DidStartLoading(WebView* webview) {
  // clear the title so we can tell if it wasn't provided by the page
  browser_->UIT_SetTitle(std::wstring());

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has started
    handler->HandleLoadStart(browser_);
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
    handler->HandleLoadEnd(browser_);
  }
}

void BrowserWebViewDelegate::WindowObjectCleared(WebFrame* webframe) {
  browser_->UIT_BindJSObjectsToWindow(webframe);
}

WindowOpenDisposition BrowserWebViewDelegate::DispositionForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebRequest* request,
    WebNavigationType type,
    WindowOpenDisposition disposition,
    bool is_redirect) {
  
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Gather browse request information
    CefRefPtr<CefRequest> req(CefRequest::CreateRequest());
    
    req->SetURL(UTF8ToWide(request->GetURL().spec()));
    req->SetMethod(UTF8ToWide(request->GetHttpMethod()));

    if(request->HasUploadData()) {
      scoped_refptr<net::UploadData> data(new net::UploadData());
			request->GetUploadData(data.get());

      CefRefPtr<CefPostData> postdata(CefPostData::CreatePostData());
      static_cast<CefPostDataImpl*>(postdata.get())->Set(*data.get());
      req->SetPostData(postdata);
		}

		WebRequest::HeaderMap map;
		request->GetHttpHeaders(&map);
    if(map.size() > 0)
      static_cast<CefRequestImpl*>(req.get())->SetHeaderMap(map);

    // Notify the handler of a browse request
    CefHandler::RetVal rv = handler->HandleBeforeBrowse(browser_, req,
        (CefHandler::NavType)type, is_redirect);
    if(rv == RV_HANDLED)
			return IGNORE_ACTION;
	}
      
  if (is_custom_policy_delegate_) {
    std::wstring frame_name = frame->GetName();
    printf("Policy delegate: attempt to load %s\n",
           request->GetURL().spec().c_str());
    return IGNORE_ACTION;
  } else {
    return WebViewDelegate::DispositionForNavigationAction(
      webview, frame, request, type, disposition, is_redirect);
  }
}

void BrowserWebViewDelegate::SetCustomPolicyDelegate(bool isCustom) {
  is_custom_policy_delegate_ = isCustom;
}

void BrowserWebViewDelegate::AssignIdentifierToRequest(WebView* webview,
                                                    uint32 identifier,
                                                    const WebRequest& request) {
}

void BrowserWebViewDelegate::WillSendRequest(WebView* webview,
                                          uint32 identifier,
                                          WebRequest* request) {
  // The requestor ID is used by the resource loader bridge to locate the
  // browser that originated the request.
  request->SetRequestorID(browser_->UIT_GetUniqueID());
}

void BrowserWebViewDelegate::DidFinishLoading(WebView* webview,
                                           uint32 identifier) {
  
}

void BrowserWebViewDelegate::DidFailLoadingWithError(WebView* webview,
                                                  uint32 identifier,
                                                  const WebError& error) {
  
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
    const WebError& error,
    WebFrame* frame) {
  LocationChangeDone(frame->GetProvisionalDataSource());

  // error codes are defined in net\base\net_error_list.h

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.GetErrorCode() == net::ERR_ABORTED)
    return;

  const WebRequest& failed_request =
      frame->GetProvisionalDataSource()->GetRequest();
  BrowserExtraRequestData* extra_data =
      static_cast<BrowserExtraRequestData*>(failed_request.GetExtraData());
  bool replace = extra_data && extra_data->pending_page_id != -1;

  scoped_ptr<WebRequest> request(failed_request.Clone());
  request->SetURL(GURL("cef-error:"));

  std::string error_text;

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // give the handler an opportunity to generate a custom error message
    std::wstring error_str;
    CefHandler::RetVal rv = handler->HandleLoadError(browser_,
        static_cast<CefHandler::ErrorCode>(error.GetErrorCode()),
        UTF8ToWide(error.GetFailedURL().spec()), error_str);
    if(rv == RV_HANDLED && !error_str.empty())
      error_text = WideToUTF8(error_str);
  } else {
      error_text = StringPrintf("Error loading url: %d", error.GetErrorCode());
  }

  frame->LoadAlternateHTMLString(request.get(), error_text,
      error.GetFailedURL(), replace);
}

void BrowserWebViewDelegate::DidCommitLoadForFrame(WebView* webview,
                                                WebFrame* frame,
                                                bool is_new_navigation) {
  UpdateForCommittedLoad(frame, is_new_navigation);
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
  LocationChangeDone(frame->GetDataSource());
}

void BrowserWebViewDelegate::DidFailLoadWithError(WebView* webview,
                                               const WebError& error,
                                               WebFrame* frame) {
  LocationChangeDone(frame->GetDataSource());
}

void BrowserWebViewDelegate::DidFinishDocumentLoadForFrame(WebView* webview,
                                                        WebFrame* frame) {
  
}

void BrowserWebViewDelegate::DidHandleOnloadEventsForFrame(WebView* webview,
                                                        WebFrame* frame) {
  
}

void BrowserWebViewDelegate::DidChangeLocationWithinPageForFrame(
    WebView* webview, WebFrame* frame, bool is_new_navigation) {
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
  if(handler.get())
    rv = handler->HandleJSAlert(browser_, message);
  if(rv != RV_HANDLED)
    ShowJavaScriptAlert(webframe, message);
}

bool BrowserWebViewDelegate::RunJavaScriptConfirm(WebFrame* webframe,
                                               const std::wstring& message) {
  CefHandler::RetVal rv = RV_CONTINUE;
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get())
    rv = handler->HandleJSConfirm(browser_, message, retval);
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
    rv = handler->HandleJSPrompt(browser_, message, default_value,
        retval, *result);
  }
  if(rv != RV_HANDLED)
    retval = ShowJavaScriptPrompt(webframe, message, default_value, result);
  return retval;
}

void BrowserWebViewDelegate::SetStatusbarText(WebView* webview,
                                              const std::wstring& message) {
  
}

void BrowserWebViewDelegate::StartDragging(WebView* webview,
                                        const WebDropData& drop_data) {
#if defined(OS_WIN)
  // TODO(port): make this work on all platforms.
  if (!drag_delegate_) {
    drag_delegate_ = new BrowserDragDelegate(
        browser_->UIT_GetWebViewWndHandle(),
        browser_->UIT_GetWebView());
  }
  // TODO(tc): Drag and drop is disabled in the test shell because we need
  // to be able to convert from WebDragData to an IDataObject.
  //const DWORD ok_effect = DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE;
  //DWORD effect;
  //HRESULT res = DoDragDrop(drop_data.data_object, drag_delegate_.get(),
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

WebHistoryItem* BrowserWebViewDelegate::GetHistoryEntryAtOffset(int offset) {
  BrowserNavigationEntry* entry = static_cast<BrowserNavigationEntry*>(
      browser_->UIT_GetNavigationController()->GetEntryAtOffset(offset));
  if (!entry)
    return NULL;

  return entry->GetHistoryItem();
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

void BrowserWebViewDelegate::SetUserStyleSheetEnabled(bool is_enabled) {
  WebPreferences* prefs = _Context->GetWebPreferences();
  prefs->user_style_sheet_enabled = is_enabled;
  browser_->UIT_GetWebView()->SetPreferences(*prefs);
}

void BrowserWebViewDelegate::SetUserStyleSheetLocation(const GURL& location) {
  WebPreferences* prefs = _Context->GetWebPreferences();
  prefs->user_style_sheet_enabled = true;
  prefs->user_style_sheet_location = location;
  browser_->UIT_GetWebView()->SetPreferences(*prefs);
}

// WebWidgetDelegate ---------------------------------------------------------

gfx::NativeViewId BrowserWebViewDelegate::GetContainingView(WebWidget* webwidget) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    return gfx::IdFromNativeView(host->window_handle());

  return NULL;
}

void BrowserWebViewDelegate::DidInvalidateRect(WebWidget* webwidget,
                                            const gfx::Rect& rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    host->DidInvalidateRect(rect);
}

void BrowserWebViewDelegate::DidScrollRect(WebWidget* webwidget, int dx, int dy,
                                        const gfx::Rect& clip_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    host->DidScrollRect(dx, dy, clip_rect);
}

void BrowserWebViewDelegate::Focus(WebWidget* webwidget) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    browser_->UIT_SetFocus(host, true);
}

void BrowserWebViewDelegate::Blur(WebWidget* webwidget) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    browser_->UIT_SetFocus(host, false);
}

bool BrowserWebViewDelegate::IsHidden() {
  return false;
}

void BrowserWebViewDelegate::RegisterDragDrop() {
#if defined(OS_WIN)
  // TODO(port): add me once drag and drop works.
  DCHECK(!drop_delegate_);
  drop_delegate_ = new BrowserDropDelegate(browser_->UIT_GetWebViewWndHandle(),
                                           browser_->UIT_GetWebView());
#endif
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

  GURL gUrl = dataSource->GetRequest().GetMainDocumentURL();
*/
}

void BrowserWebViewDelegate::LocationChangeDone(WebDataSource* data_source) {
  if (data_source->GetWebFrame() == top_loading_frame_)
    top_loading_frame_ = NULL;
}

WebWidgetHost* BrowserWebViewDelegate::GetHostForWidget(WebWidget* webwidget) {
  if (webwidget == browser_->UIT_GetWebView())
    return browser_->UIT_GetWebViewHost();
  if (webwidget == browser_->UIT_GetPopup())
    return browser_->UIT_GetPopupHost();
  return NULL;
}

void BrowserWebViewDelegate::UpdateForCommittedLoad(WebFrame* frame,
                                                 bool is_new_navigation) {
  WebView* webview = browser_->UIT_GetWebView();

  // Code duplicated from RenderView::DidCommitLoadForFrame.
  const WebRequest& request =
      webview->GetMainFrame()->GetDataSource()->GetRequest();
  BrowserExtraRequestData* extra_data =
      static_cast<BrowserExtraRequestData*>(request.GetExtraData());

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

  const WebRequest& request = ds->GetRequest();

  // Type is unused.
  scoped_ptr<BrowserNavigationEntry> entry(new BrowserNavigationEntry);

  // Bug 654101: the referrer will be empty on https->http transitions. It
  // would be nice if we could get the real referrer from somewhere.
  entry->SetPageID(page_id_);
  if (ds->HasUnreachableURL()) {
    entry->SetURL(GURL(ds->GetUnreachableURL()));
  } else {
    entry->SetURL(GURL(request.GetURL()));
  }

  std::wstring url = UTF8ToWide(entry->GetURL().spec().c_str());
	browser_->SetURL(url);

  if(frame->GetView()->GetMainFrame() == frame) {
    // only send address changes that originate from the main frame
    CefRefPtr<CefHandler> handler = browser_->GetHandler();
    if(handler.get()) {
      // Notify the handler of an address change
      handler->HandleAddressChange(browser_, url);
    }
  }

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

  std::string state;
  if (!browser_->UIT_GetWebView()->GetMainFrame()->
      GetPreviousHistoryState(&state))
    return;

  entry->SetContentState(state);
}

std::wstring BrowserWebViewDelegate::GetFrameDescription(WebFrame* webframe) {
  std::wstring name = webframe->GetName();

  if (webframe == browser_->UIT_GetWebView()->GetMainFrame()) {
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

void BrowserWebViewDelegate::TakeFocus(WebView* webview, bool reverse) {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that it should take a focus
    handler->HandleTakeFocus(browser_, reverse);
  }
}
