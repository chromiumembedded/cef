// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "browser_webview_delegate.h"
#include "browser_impl.h"
#include "browser_navigation_controller.h"
#include "context.h"
#include "request_impl.h"
#include "v8_impl.h"

#include "base/file_util.h"
#include "base/gfx/point.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/trace_event.h"
#include "base/utf_string_conversions.h"
#include "gfx/gdi_util.h"
#include "gfx/native_widget_types.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
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

using appcache::WebApplicationCacheHostImpl;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebCookieJar;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebMediaPlayer;
using WebKit::WebMediaPlayerClient;
using WebKit::WebNavigationType;
using WebKit::WebNavigationPolicy;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSecurityOrigin;
using WebKit::WebSize;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebTextAffinity;
using WebKit::WebTextDirection;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using WebKit::WebView;
using WebKit::WebWidget;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using WebKit::WebKeyboardEvent;

namespace {

int next_page_id_ = 1;

}  // namespace

// WebViewDelegate -----------------------------------------------------------

void BrowserWebViewDelegate::SetUserStyleSheetEnabled(bool is_enabled) {
  WebPreferences* prefs = _Context->GetWebPreferences();
  prefs->user_style_sheet_enabled = is_enabled;
  prefs->Apply(browser_->GetWebView());
}

void BrowserWebViewDelegate::SetUserStyleSheetLocation(const GURL& location) {
  WebPreferences* prefs = _Context->GetWebPreferences();
  prefs->user_style_sheet_enabled = true;
  prefs->user_style_sheet_location = location;
  prefs->Apply(browser_->GetWebView());
}

// WebViewClient -------------------------------------------------------------

WebView* BrowserWebViewDelegate::createView(WebFrame* creator) {
  CefRefPtr<CefBrowserImpl> browser =
      browser_->UIT_CreatePopupWindow(std::wstring());
  return browser.get() ? browser->GetWebView() : NULL;
}

WebWidget* BrowserWebViewDelegate::createPopupMenu(
    bool activatable) {
  // TODO(darin): Should we honor activatable?
  return browser_->UIT_CreatePopupWidget();
}

WebStorageNamespace* BrowserWebViewDelegate::createSessionStorageNamespace() {
  return WebKit::WebStorageNamespace::createSessionStorageNamespace();
}

void BrowserWebViewDelegate::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
    logging::LogMessage("CONSOLE", 0).stream() << "\""
                                               << message.text.utf8().data()
                                               << ",\" source: "
                                               << source_name.utf8().data()
                                               << "("
                                               << source_line
                                               << ")";
}

void BrowserWebViewDelegate::didStartLoading() {
  // clear the title so we can tell if it wasn't provided by the page
  browser_->UIT_SetTitle(std::wstring());

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has started
    handler->HandleLoadStart(browser_, NULL);
  }
}

void BrowserWebViewDelegate::didStopLoading() {
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

bool BrowserWebViewDelegate::shouldBeginEditing(const WebRange& range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::shouldEndEditing(const WebRange& range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::shouldInsertNode(const WebNode& node,
                                           const WebRange& range,
                                           WebEditingAction action) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::shouldInsertText(const WebString& text,
                                           const WebRange& range,
                                           WebEditingAction action) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::shouldChangeSelectedRange(const WebRange& from_range,
                                                    const WebRange& to_range,
                                                    WebTextAffinity affinity,
                                                    bool still_selecting) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::shouldDeleteRange(const WebRange& range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::shouldApplyStyle(const WebString& style,
                                           const WebRange& range) {
  return browser_->UIT_AllowEditing();
}

bool BrowserWebViewDelegate::isSmartInsertDeleteEnabled() {
  return smart_insert_delete_enabled_;
}

bool BrowserWebViewDelegate::isSelectTrailingWhitespaceEnabled() {
  return select_trailing_whitespace_enabled_;
}

bool BrowserWebViewDelegate::handleCurrentKeyboardEvent() {
  CefHandler::RetVal rv = RV_CONTINUE;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if (handler.get()) {
      WebWidgetHost* host = GetWidgetHost();
      if (host) {
        WebKeyboardEvent event = host->GetLastKeyEvent();
        switch (event.type)
        {
        case WebKeyboardEvent::RawKeyDown: 
          rv = handler->HandleKeyEvent(browser_,
              KEYEVENT_RAWKEYDOWN, event.windowsKeyCode,
              event.modifiers, event.isSystemKey?true:false);
          break;
        case WebKeyboardEvent::KeyUp:
          rv = handler->HandleKeyEvent(browser_,
              KEYEVENT_KEYUP, event.windowsKeyCode,
              event.modifiers, event.isSystemKey?true:false);
          break;
        case WebKeyboardEvent::Char:
          rv = handler->HandleKeyEvent(browser_,
              KEYEVENT_CHAR, event.windowsKeyCode,
              event.modifiers, event.isSystemKey?true:false);
          break;
      }
    }
  }
  if (rv == RV_HANDLED)
    return true;

  if (edit_command_name_.empty())
    return false;

  WebFrame* frame = browser_->GetWebView()->focusedFrame();
  if (!frame)
    return false;

  return frame->executeCommand(WebString::fromUTF8(edit_command_name_),
                               WebString::fromUTF8(edit_command_value_));
}

bool BrowserWebViewDelegate::runFileChooser(
      bool multi_select, const WebKit::WebString& title,
      const WebKit::WebString& initial_value,
      WebKit::WebFileChooserCompletion* chooser_completion) {
  // Support file open dialog.
  std::vector<FilePath> file_names;
  
  if(!ShowFileChooser(file_names, multi_select, title, 
      webkit_glue::WebStringToFilePath(initial_value))) {
    return false;
  }

  WebVector<WebString> ws_file_names(file_names.size());
  for (size_t i = 0; i < file_names.size(); ++i) {
    ws_file_names[i] = webkit_glue::FilePathToWebString(file_names[i]);
  }

  chooser_completion->didChooseFile(ws_file_names);

  return true;
}

void BrowserWebViewDelegate::runModalAlertDialog(
    WebFrame* frame, const WebString& message) {
  std::wstring messageStr = UTF16ToWideHack(message);
  CefHandler::RetVal rv = RV_CONTINUE;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSAlert(browser_, browser_->GetCefFrame(frame),
        messageStr);
  }
  if(rv != RV_HANDLED)
    ShowJavaScriptAlert(frame, messageStr);
}

bool BrowserWebViewDelegate::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  std::wstring messageStr = UTF16ToWideHack(message);
  CefHandler::RetVal rv = RV_CONTINUE;
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSConfirm(browser_, browser_->GetCefFrame(frame),
        messageStr, retval);
  }
  if(rv != RV_HANDLED)
    retval = ShowJavaScriptConfirm(frame, messageStr);
  return retval;
}

bool BrowserWebViewDelegate::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  std::wstring wmessage = UTF16ToWideHack(message);
  std::wstring wdefault = UTF16ToWideHack(default_value);
  std::wstring wresult;

  if(actual_value)
    wresult = UTF16ToWideHack(*actual_value);

  CefHandler::RetVal rv = RV_CONTINUE;
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSPrompt(browser_, browser_->GetCefFrame(frame),
        wmessage, wdefault, retval, wresult);
  }
  if(rv != RV_HANDLED)
    retval = ShowJavaScriptPrompt(frame, wmessage, wdefault, &wresult);

  if(actual_value && !wresult.empty())
    *actual_value = WideToUTF16Hack(wresult);

  return retval;
}

bool BrowserWebViewDelegate::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  return true;  // Allow window closure.
}

void BrowserWebViewDelegate::setStatusText(const WebString& text) {
}

void BrowserWebViewDelegate::setMouseOverURL(const WebURL& url) {
}

void BrowserWebViewDelegate::setKeyboardFocusURL(const WebKit::WebURL& url) {
}

void BrowserWebViewDelegate::setToolTipText(
    const WebString& text, WebTextDirection hint) {
}

void BrowserWebViewDelegate::startDragging(
    const WebPoint& mouse_coords, const WebDragData& data,
    WebDragOperationsMask mask) {
  // TODO(tc): Drag and drop is disabled in the test shell because we need
  // to be able to convert from WebDragData to an IDataObject.
  //if (!drag_delegate_)
  //  drag_delegate_ = new BrowserDragDelegate(shell_->webViewWnd(),
  //                                        shell_->webView());
  //const DWORD ok_effect = DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE;
  //DWORD effect;
  //HRESULT res = DoDragDrop(drop_data.data_object, drag_delegate_.get(),
  //                         ok_effect, &effect);
  //DCHECK(DRAGDROP_S_DROP == res || DRAGDROP_S_CANCEL == res);
  browser_->GetWebView()->dragSourceSystemDragEnded();
}

void BrowserWebViewDelegate::focusNext() {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that it should take a focus
    handler->HandleTakeFocus(browser_, false);
  }
}

void BrowserWebViewDelegate::focusPrevious() {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that it should take a focus
    handler->HandleTakeFocus(browser_, true);
  }
}

void BrowserWebViewDelegate::navigateBackForwardSoon(int offset) {
  browser_->UIT_GetNavigationController()->GoToOffset(offset);
}

int BrowserWebViewDelegate::historyBackListCount() {
  int current_index =
      browser_->UIT_GetNavigationController()->GetLastCommittedEntryIndex();
  return current_index;
}

int BrowserWebViewDelegate::historyForwardListCount() {
  int current_index =
      browser_->UIT_GetNavigationController()->GetLastCommittedEntryIndex();
  return browser_->UIT_GetNavigationController()->GetEntryCount()
      - current_index - 1;
}

// WebPluginPageDelegate -----------------------------------------------------

WebCookieJar* BrowserWebViewDelegate::GetCookieJar() {
  return WebKit::webKitClient()->cookieJar();
}

// WebWidgetClient -----------------------------------------------------------

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

// WebFrameClient ------------------------------------------------------------

WebPlugin* BrowserWebViewDelegate::createPlugin(
    WebFrame* frame, const WebPluginParams& params) {
  return new webkit_glue::WebPluginImpl(frame, params, AsWeakPtr());
}

WebMediaPlayer* BrowserWebViewDelegate::createMediaPlayer(
    WebFrame* frame, WebMediaPlayerClient* client) {
  scoped_refptr<media::FilterFactoryCollection> factory =
      new media::FilterFactoryCollection();

  appcache::WebApplicationCacheHostImpl* appcache_host =
      appcache::WebApplicationCacheHostImpl::FromFrame(frame);

  // TODO(hclam): this is the same piece of code as in RenderView, maybe they
  // should be grouped together.
  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory =
      new webkit_glue::MediaResourceLoaderBridgeFactory(
          GURL(),  // referrer
          "null",  // frame origin
          "null",  // main_frame_origin
          base::GetCurrentProcId(),
          appcache_host ? appcache_host->host_id() : appcache::kNoHostId,
          0);
  // A simple data source that keeps all data in memory.
  media::FilterFactory* simple_data_source_factory =
      webkit_glue::SimpleDataSource::CreateFactory(MessageLoop::current(),
                                                   bridge_factory);
  // A sophisticated data source that does memory caching.
  media::FilterFactory* buffered_data_source_factory =
      webkit_glue::BufferedDataSource::CreateFactory(MessageLoop::current(),
                                                     bridge_factory);
  factory->AddFactory(buffered_data_source_factory);
  factory->AddFactory(simple_data_source_factory);
  return new webkit_glue::WebMediaPlayerImpl(
      client, factory, new webkit_glue::VideoRendererImpl::FactoryFactory());
}

void BrowserWebViewDelegate::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  DCHECK_NE(policy, WebKit::WebNavigationPolicyCurrentTab);
  browser_->UIT_CreatePopupWindow(UTF8ToWide(request.url().spec().data()));
}

WebNavigationPolicy BrowserWebViewDelegate::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationType type, const WebNode& originating_node,
    WebNavigationPolicy default_policy, bool is_redirect) {
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

WebURLError BrowserWebViewDelegate::cannotHandleRequestError(
    WebFrame* frame, const WebURLRequest& request) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

WebURLError BrowserWebViewDelegate::cancelledError(
    WebFrame* frame, const WebURLRequest& request) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

void BrowserWebViewDelegate::didCreateDataSource(
    WebFrame* frame, WebDataSource* ds) {
  ds->setExtraData(pending_extra_data_.release());
}

void BrowserWebViewDelegate::didStartProvisionalLoad(WebFrame* frame) {
  if (!top_loading_frame_) {
    top_loading_frame_ = frame;
  }

  UpdateAddressBar(frame->view());
}

void BrowserWebViewDelegate::didReceiveServerRedirectForProvisionalLoad(
    WebFrame* frame) {
  UpdateAddressBar(frame->view());
}

void BrowserWebViewDelegate::didFailProvisionalLoad(
    WebFrame* frame, const WebURLError& error) {
  LocationChangeDone(frame);

  // error codes are defined in net\base\net_error_list.h

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.reason == net::ERR_ABORTED)
    return;

  const WebDataSource* failed_ds = frame->provisionalDataSource();
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
  frame->enableViewSourceMode(false);

  frame->loadHTMLString(
      error_text, GURL("testshell-error:"), error.unreachableURL, replace);
}

void BrowserWebViewDelegate::didCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  UpdateForCommittedLoad(frame, is_new_navigation);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has started
    handler->HandleLoadStart(browser_, browser_->GetCefFrame(frame));
  }
}

void BrowserWebViewDelegate::didClearWindowObject(WebFrame* frame) {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = webkit_glue::GetV8Context(frame);
    if(context.IsEmpty())
      return;

    v8::Context::Scope scope(context);

    CefRefPtr<CefFrame> cframe(browser_->GetCefFrame(frame));
    CefRefPtr<CefV8Value> object = new CefV8ValueImpl(context->Global());
    handler->HandleJSBinding(browser_, cframe, object);
  }
}

void BrowserWebViewDelegate::didReceiveTitle(
    WebFrame* frame, const WebString& title) {
  std::wstring wtitle = UTF16ToWideHack(title);

  browser_->UIT_SetTitle(wtitle);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler of a page title change
    handler->HandleTitleChange(browser_, wtitle);
  }
}

void BrowserWebViewDelegate::didFailLoad(
    WebFrame* frame, const WebURLError& error) {
  LocationChangeDone(frame);
}

void BrowserWebViewDelegate::didFinishLoad(WebFrame* frame) {
  UpdateAddressBar(frame->view());
  LocationChangeDone(frame);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has ended
    handler->HandleLoadEnd(browser_, browser_->GetCefFrame(frame));
  }
}

void BrowserWebViewDelegate::didChangeLocationWithinPage(
    WebFrame* frame, bool is_new_navigation) {
  frame->dataSource()->setExtraData(pending_extra_data_.release());
  UpdateForCommittedLoad(frame, is_new_navigation);
}

void BrowserWebViewDelegate::willSendRequest(
    WebFrame* frame, unsigned identifier, WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  if (!redirect_response.isNull() && block_redirects_) {
    // To block the request, we set its URL to an empty one.
    request.setURL(WebURL());
    return;
  }

  // The requestor ID is used by the resource loader bridge to locate the
  // browser that originated the request.
  request.setRequestorID(browser_->UIT_GetUniqueID());
}


// Public methods ------------------------------------------------------------

BrowserWebViewDelegate::BrowserWebViewDelegate(CefBrowserImpl* browser)
    : policy_delegate_enabled_(false),
      policy_delegate_is_permissive_(false),
      policy_delegate_should_notify_done_(false),
      browser_(browser),
      top_loading_frame_(NULL),
      page_id_(-1),
      last_page_id_updated_(-1),
      smart_insert_delete_enabled_(true),
#if defined(OS_WIN)
      select_trailing_whitespace_enabled_(true),
#else
      select_trailing_whitespace_enabled_(false),
#endif
      block_redirects_(false) {
}

BrowserWebViewDelegate::~BrowserWebViewDelegate() {
}

void BrowserWebViewDelegate::Reset() {
  // Do a little placement new dance...
  CefBrowserImpl* browser = browser_;
  this->~BrowserWebViewDelegate();
  new (this) BrowserWebViewDelegate(browser);
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

void BrowserWebViewDelegate::RevokeDragDrop() {
#if defined(OS_WIN)
  ::RevokeDragDrop(browser_->GetWebViewWndHandle());
#endif
}

void BrowserWebViewDelegate::SetCustomPolicyDelegate(bool is_custom,
                                                  bool is_permissive) {
  policy_delegate_enabled_ = is_custom;
  policy_delegate_is_permissive_ = is_permissive;
}

void BrowserWebViewDelegate::WaitForPolicyDelegate() {
  policy_delegate_enabled_ = true;
  policy_delegate_should_notify_done_ = true;
}

// Private methods -----------------------------------------------------------

void BrowserWebViewDelegate::UpdateAddressBar(WebView* webView) {
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
  // Code duplicated from RenderView::DidCommitLoadForFrame.
  BrowserExtraData* extra_data = static_cast<BrowserExtraData*>(
      frame->dataSource()->extraData());

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
  WebDataSource* ds = frame->dataSource();
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

  const WebHistoryItem& history_item = frame->currentHistoryItem();
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
      browser_->GetWebView()->mainFrame()->previousHistoryItem();
  if (history_item.isNull())
    return;

  entry->SetContentState(webkit_glue::HistoryItemToString(history_item));
}
