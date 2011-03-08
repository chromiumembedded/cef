// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "browser_webview_delegate.h"
#include "browser_appcache_system.h"
#include "browser_file_system.h"
#include "browser_impl.h"
#include "browser_navigation_controller.h"
#include "browser_web_worker.h"
#include "browser_webkit_glue.h"
#include "browser_zoom_map.h"
#include "cef_context.h"
#include "request_impl.h"
#include "v8_impl.h"

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "media/base/filter_collection.h"
#include "media/base/message_loop_factory_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/plugins/npapi/webplugin_impl.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
#include "browser_drag_delegate_win.h"
#include "web_drop_target_win.h"
#endif

using appcache::WebApplicationCacheHostImpl;
using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebCookieJar;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebFileChooserParams;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebImage;
using WebKit::WebMediaPlayer;
using WebKit::WebMediaPlayerClient;
using WebKit::WebNavigationType;
using WebKit::WebNavigationPolicy;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebPopupType;
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
using WebKit::WebWindowFeatures;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using WebKit::WebKeyboardEvent;

namespace {

int next_page_id_ = 1;

void TranslatePopupFeatures(const WebWindowFeatures& webKitFeatures,
                            CefPopupFeatures& features)
{
  features.x = static_cast<int>(webKitFeatures.x);
  features.xSet = webKitFeatures.xSet;
  features.y = static_cast<int>(webKitFeatures.y);
  features.ySet = webKitFeatures.ySet;
  features.width = static_cast<int>(webKitFeatures.width);
  features.widthSet = webKitFeatures.widthSet;
  features.height = static_cast<int>(webKitFeatures.height);
  features.heightSet =  webKitFeatures.heightSet;

  features.menuBarVisible =  webKitFeatures.menuBarVisible;
  features.statusBarVisible =  webKitFeatures.statusBarVisible;
  features.toolBarVisible =  webKitFeatures.toolBarVisible;
  features.locationBarVisible =  webKitFeatures.locationBarVisible;
  features.scrollbarsVisible =  webKitFeatures.scrollbarsVisible;
  features.resizable =  webKitFeatures.resizable;

  features.fullscreen =  webKitFeatures.fullscreen;
  features.dialog =  webKitFeatures.dialog;      
  features.additionalFeatures = NULL;
  if (webKitFeatures.additionalFeatures.size() > 0)   
     features.additionalFeatures = cef_string_list_alloc();

  CefString str;
  for(unsigned int i = 0; i < webKitFeatures.additionalFeatures.size(); ++i) {
    str = string16(webKitFeatures.additionalFeatures[i]);
    cef_string_list_append(features.additionalFeatures, str.GetStruct());
  }
}

}  // namespace

// WebViewClient -------------------------------------------------------------

WebView* BrowserWebViewDelegate::createView(WebFrame* creator,
    const WebURLRequest& request, const WebWindowFeatures& features,
    const WebString& name) {
  CefString url;
  if (!request.isNull())
    url = request.url().spec().utf16();
  CefPopupFeatures cefFeatures;
  TranslatePopupFeatures(features, cefFeatures);
  CefRefPtr<CefBrowserImpl> browser =
      browser_->UIT_CreatePopupWindow(url, cefFeatures);
  return browser.get() ? browser->UIT_GetWebView() : NULL;
}

WebWidget* BrowserWebViewDelegate::createPopupMenu(WebPopupType popup_type) {
  // TODO(darin): Should we take into account |popup_type| (for activation
  //              purpose)?
  return browser_->UIT_CreatePopupWidget();
}

WebStorageNamespace* BrowserWebViewDelegate::createSessionStorageNamespace(
    unsigned quota) {
  // Enforce quota, ignoring the parameter from WebCore as in Chrome. We could
  // potentially use DOMStorageContext to manage session storage but there's
  // currently no need since session storage data is not written to disk.
  return WebKit::WebStorageNamespace::createSessionStorageNamespace(
      WebStorageNamespace::m_sessionStorageQuota);
}

void BrowserWebViewDelegate::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  std::string messageStr = message.text.utf8();
  std::string sourceStr = source_name.utf8();

  CefHandler::RetVal rv = RV_CONTINUE;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleConsoleMessage(browser_, messageStr, sourceStr,
        source_line);
  }

  if(rv == RV_CONTINUE) {
    logging::LogMessage("CONSOLE", 0).stream() << "\""
                                               << messageStr
                                               << ",\" source: "
                                               << sourceStr
                                               << "("
                                               << source_line
                                               << ")";
  }
}

void BrowserWebViewDelegate::printPage(WebFrame* frame) {
  if (!frame) {
    WebView* view = browser_->UIT_GetWebView();
    if (view)
      frame = view->mainFrame();
  }
  if (frame)
    browser_->UIT_PrintPages(frame);
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

bool BrowserWebViewDelegate::shouldChangeSelectedRange(
    const WebRange& from_range, const WebRange& to_range,
    WebTextAffinity affinity, bool still_selecting) {
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
        default:
          break;
      }
    }
  }
  if (rv == RV_HANDLED)
    return true;

  if (edit_command_name_.empty())
    return false;

  WebView* view = browser_->UIT_GetWebView();
  WebFrame* frame = NULL;
  if (view)
    frame = view->focusedFrame();
  if (!frame)
    return false;

  return frame->executeCommand(WebString::fromUTF8(edit_command_name_),
                               WebString::fromUTF8(edit_command_value_));
}

bool BrowserWebViewDelegate::runFileChooser(
      const WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion) {
  // Support file open dialog.
  std::vector<FilePath> file_names;
  
  if(!ShowFileChooser(file_names, params.multiSelect, params.title, 
      webkit_glue::WebStringToFilePath(params.initialValue))) {
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
  CefHandler::RetVal rv = RV_CONTINUE;
  CefString messageStr = string16(message);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSAlert(browser_, browser_->UIT_GetCefFrame(frame),
        messageStr);
  }
  if(rv != RV_HANDLED)
    ShowJavaScriptAlert(frame, messageStr);
}

bool BrowserWebViewDelegate::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  CefHandler::RetVal rv = RV_CONTINUE;
  CefString messageStr = string16(message);
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSConfirm(browser_, browser_->UIT_GetCefFrame(frame),
        messageStr, retval);
  }
  if(rv != RV_HANDLED)
    retval = ShowJavaScriptConfirm(frame, messageStr);
  return retval;
}

bool BrowserWebViewDelegate::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  CefString messageStr = string16(message);
  CefString defaultValueStr = string16(default_value);
  CefString actualValueStr;
  if(actual_value)
    actualValueStr = string16(*actual_value);

  CefHandler::RetVal rv = RV_CONTINUE;
  bool retval = false;
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    rv = handler->HandleJSPrompt(browser_, browser_->UIT_GetCefFrame(frame),
        messageStr, defaultValueStr, retval, actualValueStr);
  }
  if(rv != RV_HANDLED) {
    retval = ShowJavaScriptPrompt(frame, messageStr, defaultValueStr,
        &actualValueStr);
  }
  if (actual_value)
    *actual_value = string16(actualValueStr);
  
  return retval;
}

bool BrowserWebViewDelegate::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  return true;  // Allow window closure.
}

void BrowserWebViewDelegate::setStatusText(const WebString& text) {
  ShowStatus(text, STATUSTYPE_TEXT);
}

void BrowserWebViewDelegate::setMouseOverURL(const WebURL& url) {
  ShowStatus(url.spec().utf16(), STATUSTYPE_MOUSEOVER_URL);
}

void BrowserWebViewDelegate::setKeyboardFocusURL(const WebKit::WebURL& url) {
  ShowStatus(url.spec().utf16(), STATUSTYPE_KEYBOARD_FOCUS_URL);
}

void BrowserWebViewDelegate::setToolTipText(
    const WebString& text, WebTextDirection hint) 
{
  CefString tooltipStr = string16(text);
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get() && handler->HandleTooltip(browser_, tooltipStr)
      == RV_CONTINUE){
     GetWidgetHost()->SetTooltipText(tooltipStr);
  }
}

void BrowserWebViewDelegate::startDragging(
    const WebDragData& data,
    WebDragOperationsMask mask,
    const WebImage& image,
    const WebPoint& image_offset) {
#if defined(OS_WIN)
  drag_delegate_ = new BrowserDragDelegate(this);
  drag_delegate_->StartDragging(WebDropData(data), mask, image.getSkBitmap(),
                                image_offset);
#else
  // TODO(port): Support drag and drop.
  EndDragging();
#endif
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

void BrowserWebViewDelegate::scheduleComposite() {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScheduleComposite();
}

void BrowserWebViewDelegate::scheduleAnimation() {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScheduleAnimation();
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

void BrowserWebViewDelegate::closeWidgetSoon() {
  if (this == browser_->UIT_GetWebViewDelegate()) {
    browser_->UIT_CloseBrowser();
  } else if (this == browser_->UIT_GetPopupDelegate()) {
    browser_->UIT_ClosePopupWidget();
  }
}

WebScreenInfo BrowserWebViewDelegate::screenInfo() {
  if (WebWidgetHost* host = GetWidgetHost())
    return host->GetScreenInfo();

  return WebScreenInfo();
}

// WebFrameClient ------------------------------------------------------------

WebPlugin* BrowserWebViewDelegate::createPlugin(
    WebFrame* frame, const WebPluginParams& params) {
  bool allow_wildcard = true;
  webkit::npapi::WebPluginInfo info;
  std::string actual_mime_type;
  if (!webkit::npapi::PluginList::Singleton()->GetPluginInfo(
          params.url, params.mimeType.utf8(), allow_wildcard, &info,
          &actual_mime_type) || !webkit::npapi::IsPluginEnabled(info)) {
    return NULL;
  }

  return new webkit::npapi::WebPluginImpl(
      frame, params, info.path, actual_mime_type, AsWeakPtr());
}

WebWorker* BrowserWebViewDelegate::createWorker(
    WebFrame* frame, WebWorkerClient* client) {
  return new BrowserWebWorker();
}

WebMediaPlayer* BrowserWebViewDelegate::createMediaPlayer(
    WebFrame* frame, WebMediaPlayerClient* client) {
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
     new media::MessageLoopFactoryImpl());

  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());

  scoped_refptr<webkit_glue::VideoRendererImpl> video_renderer(
      new webkit_glue::VideoRendererImpl(false));
  collection->AddVideoRenderer(video_renderer);

  // Add the audio renderer.
  collection->AddAudioRenderer(new media::AudioRendererImpl());

  scoped_ptr<webkit_glue::WebMediaPlayerImpl> result(
      new webkit_glue::WebMediaPlayerImpl(client,
                                          collection.release(),
                                          message_loop_factory.release()));
  if (!result->Initialize(frame, false, video_renderer))
    return NULL;
  return result.release();
}

WebApplicationCacheHost* BrowserWebViewDelegate::createApplicationCacheHost(
    WebFrame* frame, WebApplicationCacheHostClient* client) {
  return BrowserAppCacheSystem::CreateApplicationCacheHost(client);
}

void BrowserWebViewDelegate::willClose(WebFrame* frame) {
  browser_->UIT_BeforeFrameClosed(frame);
}

void BrowserWebViewDelegate::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  DCHECK_NE(policy, WebKit::WebNavigationPolicyCurrentTab);
  browser_->UIT_CreatePopupWindow(std::string(request.url().spec().data()),
      CefPopupFeatures());
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
    if (!request_url.is_valid())
      return WebKit::WebNavigationPolicyIgnore;

    req->SetURL(request_url.spec());
    req->SetMethod(string16(request.httpMethod()));

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
        browser_->UIT_GetCefFrame(frame), req, (CefHandler::NavType)type,
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
}

void BrowserWebViewDelegate::didReceiveServerRedirectForProvisionalLoad(
    WebFrame* frame) {
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
  
  if (extra_data && !extra_data->request_committed) {
    // Set the pending extra_data for our error page as the same pending_page_id
    // to keep the history from getting messed up.
    set_pending_extra_data(new BrowserExtraData(extra_data->pending_page_id));
  }

  std::string error_text;
  CefString errorStr;

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  CefHandler::RetVal rv = RV_CONTINUE;
  if(handler.get()) {
    // give the handler an opportunity to generate a custom error message
    rv = handler->HandleLoadError(browser_,
        browser_->UIT_GetCefFrame(frame),
        static_cast<CefHandler::ErrorCode>(error.reason),
        std::string(failed_ds->request().url().spec().data()), errorStr);
  }
    
  if(rv == RV_HANDLED && !errorStr.empty()) {
    error_text = errorStr;
  } else {
    error_text = StringPrintf("Error %d when loading url %s",
        error.reason, failed_ds->request().url().spec().data());
  }

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  frame->loadHTMLString(
      error_text, GURL("testshell-error:"), error.unreachableURL, false);

  // In case loadHTMLString failed before DidCreateDataSource was called.
  set_pending_extra_data(NULL);
}

void BrowserWebViewDelegate::didCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  bool is_main_frame = (frame->parent() == 0);
  if (is_main_frame) {
    // Clear the title so we can tell if it wasn't provided by the page.
    browser_->UIT_SetTitle(std::wstring());
  }

  UpdateForCommittedLoad(frame, is_new_navigation);

  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Notify the handler that loading has started.
    handler->HandleLoadStart(browser_, browser_->UIT_GetCefFrame(frame));
  }

  // Apply zoom settings only on top-level frames.
  if(is_main_frame) {
    // Restore the zoom value that we have for this URL, if any.
    double zoomLevel = 0.0;
    ZoomMap::GetInstance()->get(frame->url(), zoomLevel);
    frame->view()->setZoomLevel(false, zoomLevel);
    browser_->set_zoom_level(zoomLevel);
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

    CefRefPtr<CefFrame> cframe(browser_->UIT_GetCefFrame(frame));
    CefRefPtr<CefV8Value> object = new CefV8ValueImpl(context->Global());
    handler->HandleJSBinding(browser_, cframe, object);
  }
}

void BrowserWebViewDelegate::didReceiveTitle(
    WebFrame* frame, const WebString& title) {
  bool is_main_frame = (frame->parent() == 0);
  if (is_main_frame) {
    CefString titleStr = string16(title);
    browser_->UIT_SetTitle(titleStr);
    CefRefPtr<CefHandler> handler = browser_->GetHandler();
    if(handler.get()) {
      // Notify the handler of a page title change
      handler->HandleTitleChange(browser_, titleStr);
    }
  }
}

void BrowserWebViewDelegate::didFailLoad(
    WebFrame* frame, const WebURLError& error) {
  LocationChangeDone(frame);
}

void BrowserWebViewDelegate::didFinishLoad(WebFrame* frame) {
  LocationChangeDone(frame);
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

void BrowserWebViewDelegate::reportFindInPageMatchCount(
    int request_id, int count, bool final_update)
{
  browser_->UIT_NotifyFindStatus(request_id, count, gfx::Rect(),
      -1,  // // Don't update active match ordinal.
      final_update);
}

void BrowserWebViewDelegate::reportFindInPageSelection(
    int request_id, int active_match_ordinal, const WebKit::WebRect& sel)
{
  browser_->UIT_NotifyFindStatus(request_id, -1, sel, active_match_ordinal,
      false);
}

void BrowserWebViewDelegate::openFileSystem(
    WebFrame* frame, WebFileSystem::Type type, long long size, bool create,
    WebFileSystemCallbacks* callbacks) {
  BrowserFileSystem* fileSystem = static_cast<BrowserFileSystem*>(
      WebKit::webKitClient()->fileSystem());
  fileSystem->OpenFileSystem(frame, type, size, create, callbacks);
}

// Public methods ------------------------------------------------------------

BrowserWebViewDelegate::BrowserWebViewDelegate(CefBrowserImpl* browser)
    : policy_delegate_enabled_(false),
      policy_delegate_is_permissive_(false),
      policy_delegate_should_notify_done_(false),
      browser_(browser),
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
  DCHECK(!drop_target_);
  drop_target_ = new WebDropTarget(browser_->UIT_GetWebViewWndHandle(),
                                   browser_->UIT_GetWebView());
#endif
}

void BrowserWebViewDelegate::RevokeDragDrop() {
#if defined(OS_WIN)
  if (drop_target_.get())
    ::RevokeDragDrop(browser_->UIT_GetWebViewWndHandle());
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

void BrowserWebViewDelegate::ShowStatus(const WebString& text,
                                        CefHandler::StatusType type)
{
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    CefString textStr = string16(text);
    handler->HandleStatus(browser_, textStr, type);
  }
}

void BrowserWebViewDelegate::LocationChangeDone(WebFrame* frame) {
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if (!handler.get())
    return;

  bool is_main_frame = (frame->parent() == 0);
  if (is_main_frame) {
    CefString title = browser_->UIT_GetTitle();
    if (title.empty()) {
      // No title was provided by the page, so send a blank string to the
      // client.
      handler->HandleTitleChange(browser_, title);
    }
  }

  // Notify the handler that loading has ended.
  int httpStatusCode = frame->dataSource()->response().httpStatusCode();
  handler->HandleLoadEnd(browser_, browser_->UIT_GetCefFrame(frame),
      httpStatusCode);
}

WebWidgetHost* BrowserWebViewDelegate::GetWidgetHost() {
  if (this == browser_->UIT_GetWebViewDelegate())
    return browser_->UIT_GetWebViewHost();
  if (this == browser_->UIT_GetPopupDelegate())
    return browser_->UIT_GetPopupHost();
  return NULL;
}

void BrowserWebViewDelegate::EndDragging() {
  browser_->UIT_GetWebView()->dragSourceSystemDragEnded();
#if defined(OS_WIN)
  drag_delegate_ = NULL;
#endif
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

  bool is_main_frame = (frame->parent() == 0);
  if (is_main_frame) {
    CefRefPtr<CefHandler> handler = browser_->GetHandler();
    if(handler.get()) {
      // Notify the handler of an address change
      std::string url = std::string(entry->GetURL().spec().c_str());
      handler->HandleAddressChange(browser_, browser_->UIT_GetCefFrame(frame),
          url);
    }
  }

  const WebHistoryItem& history_item = frame->currentHistoryItem();
  if (!history_item.isNull())
    entry->SetContentState(webkit_glue::HistoryItemToString(history_item));

  BrowserNavigationController* controller =
      browser_->UIT_GetNavigationController();
  controller->DidNavigateToEntry(entry.release());
  browser_->set_nav_state(!controller->IsAtStart(), !controller->IsAtEnd());

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

  WebView* view = browser_->UIT_GetWebView();
  if (!view) 
    return;

  const WebHistoryItem& history_item = view->mainFrame()->previousHistoryItem();
  if (history_item.isNull())
    return;

  entry->SetContentState(webkit_glue::HistoryItemToString(history_item));
}
