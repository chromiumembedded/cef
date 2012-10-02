// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "libcef/browser_webview_delegate.h"

#include <algorithm>

#include "libcef/browser_appcache_system.h"
#include "libcef/browser_dom_storage_system.h"
#include "libcef/browser_file_system.h"
#include "libcef/browser_impl.h"
#include "libcef/browser_navigation_controller.h"
#include "libcef/browser_webkit_glue.h"
#include "libcef/browser_zoom_map.h"
#include "libcef/cef_context.h"
#include "libcef/cef_process_ui_thread.h"
#include "libcef/dom_document_impl.h"
#include "libcef/request_impl.h"
#include "libcef/v8_impl.h"

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/plugins/npapi/webplugin_impl.h"

#if defined(OS_WIN)
#include "libcef/browser_drag_delegate_win.h"
#include "libcef/web_drop_target_win.h"
#endif

#if defined(OS_LINUX)
#include "libcef/web_drag_source_gtk.h"
#include "libcef/web_drop_target_gtk.h"
#endif

using appcache::WebApplicationCacheHostImpl;
using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebCookieJar;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebEditingAction;
using WebKit::WebFileChooserParams;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
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
using WebKit::WebKeyboardEvent;

namespace {

int next_page_id_ = 1;

void TranslatePopupFeatures(const WebWindowFeatures& webKitFeatures,
                            CefPopupFeatures& features) {
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
  for (unsigned int i = 0; i < webKitFeatures.additionalFeatures.size(); ++i) {
    str = string16(webKitFeatures.additionalFeatures[i]);
    cef_string_list_append(features.additionalFeatures, str.GetStruct());
  }
}

}  // namespace

// WebViewClient -------------------------------------------------------------

WebView* BrowserWebViewDelegate::createView(WebFrame* creator,
    const WebURLRequest& request, const WebWindowFeatures& features,
    const WebString& name, WebKit::WebNavigationPolicy policy) {
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
  return BrowserDomStorageSystem::instance().CreateSessionStorageNamespace();
}

WebKit::WebGraphicsContext3D* BrowserWebViewDelegate::createGraphicsContext3D(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  WebKit::WebView* web_view = browser_->UIT_GetWebView();
  if (!web_view)
    return NULL;
      
  const CefSettings& settings = _Context->settings();
  return webkit_glue::CreateGraphicsContext3D(settings.graphics_implementation,
      attributes, web_view, true);
}

void BrowserWebViewDelegate::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  std::string messageStr = message.text.utf8();
  std::string sourceStr = source_name.utf8();

  bool handled = false;
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler.get()) {
      handled = handler->OnConsoleMessage(browser_, messageStr, sourceStr,
          source_line);
    }
  }

  if (!handled) {
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
  WebWidgetHost* host = GetWidgetHost();
  if (host && OnKeyboardEvent(host->GetLastKeyEvent(), true))
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

  if (!ShowFileChooser(file_names, params.multiSelect, params.title,
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
  CefString messageStr = string16(message);
  bool handled = false;

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefJSDialogHandler> handler;
  if (client.get())
    handler = client->GetJSDialogHandler();

  if (handler.get()) {
    handled = handler->OnJSAlert(browser_, browser_->UIT_GetCefFrame(frame),
        messageStr);
  }
  if (!handled)
    ShowJavaScriptAlert(frame, messageStr);
}

bool BrowserWebViewDelegate::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  CefString messageStr = string16(message);
  bool retval = false;
  bool handled = false;

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefJSDialogHandler> handler;
  if (client.get())
    handler = client->GetJSDialogHandler();

  if (handler.get()) {
    handled = handler->OnJSConfirm(browser_, browser_->UIT_GetCefFrame(frame),
        messageStr, retval);
  }
  if (!handled)
    retval = ShowJavaScriptConfirm(frame, messageStr);
  return retval;
}

bool BrowserWebViewDelegate::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  CefString messageStr = string16(message);
  CefString defaultValueStr = string16(default_value);
  CefString actualValueStr;
  if (actual_value)
    actualValueStr = string16(*actual_value);

  bool retval = false;
  bool handled = false;

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefJSDialogHandler> handler;
  if (client.get())
    handler = client->GetJSDialogHandler();

  if (handler.get()) {
    handled = handler->OnJSPrompt(browser_, browser_->UIT_GetCefFrame(frame),
        messageStr, defaultValueStr, retval, actualValueStr);
  }
  if (!handled) {
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
    const WebString& text, WebTextDirection hint) {
  CefString tooltipStr = string16(text);
  bool handled = false;
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler.get())
      handled = handler->OnTooltip(browser_, tooltipStr);
  }

  if (!handled)
    GetWidgetHost()->SetTooltipText(tooltipStr);
}

bool BrowserWebViewDelegate::acceptsLoadDrops() {
  return !browser_->settings().load_drops_disabled;
}

void BrowserWebViewDelegate::focusNext() {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
    if (handler.get()) {
      // Notify the handler that it should take a focus
      handler->OnTakeFocus(browser_, true);
    }
  }
}

void BrowserWebViewDelegate::focusPrevious() {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
    if (handler.get()) {
      // Notify the handler that it should take a focus
      handler->OnTakeFocus(browser_, false);
    }
  }
}

void BrowserWebViewDelegate::focusedNodeChanged(const WebKit::WebNode& node) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
    if (handler.get()) {
      if (node.isNull()) {
        handler->OnFocusedNodeChanged(browser_, browser_->GetFocusedFrame(),
            NULL);
      } else {
        const WebKit::WebDocument& document = node.document();
        if (!document.isNull()) {
          WebKit::WebFrame* frame = document.frame();
          CefRefPtr<CefDOMDocumentImpl> documentImpl =
              new CefDOMDocumentImpl(browser_, frame);
          handler->OnFocusedNodeChanged(browser_,
              browser_->UIT_GetCefFrame(frame),
              documentImpl->GetOrCreateNode(node));
          documentImpl->Detach();
        }
      }
    }
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

WebKit::WebGeolocationClient* BrowserWebViewDelegate::geolocationClient() {
  return browser_->UIT_GetGeolocationClient();
}

// WebPermissionClient -------------------------------------------------------

bool BrowserWebViewDelegate::allowScriptExtension(
    WebKit::WebFrame* frame,
    const WebKit::WebString& extensionName,
    int extensionGroup) {
  bool allowExtension = true;
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefPermissionHandler> handler = client->GetPermissionHandler();
    if (handler.get()) {
      CefString extensionNameStr = string16(extensionName);
      allowExtension = !handler->OnBeforeScriptExtensionLoad(
          browser_, browser_->UIT_GetCefFrame(frame), extensionNameStr);
    }
  }
  return allowExtension;
}

// WebPrerendererClient ------------------------------------------------------

void BrowserWebViewDelegate::willAddPrerender(WebKit::WebPrerender* prerender) {
}

// WebPluginPageDelegate -----------------------------------------------------

WebKit::WebPlugin* BrowserWebViewDelegate::CreatePluginReplacement(
    const FilePath& file_path) {
  return NULL;
}

WebCookieJar* BrowserWebViewDelegate::GetCookieJar() {
  return &cookie_jar_;
}

// WebWidgetClient -----------------------------------------------------------

void BrowserWebViewDelegate::didInvalidateRect(const WebRect& rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->InvalidateRect(rect);
}

void BrowserWebViewDelegate::didScrollRect(int dx, int dy,
                                        const WebRect& clip_rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScrollRect(dx, dy, clip_rect);
}

void BrowserWebViewDelegate::scheduleComposite() {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScheduleComposite();
}

void BrowserWebViewDelegate::scheduleAnimation() {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScheduleAnimation();
}

// This method is called when:
// A. A request is loaded in a window other than the source window
//    (FrameLoader::loadFrameRequest), or
// B. A request is loaded in an already existing popup window
//    (FrameLoader::createWindow), or
// C. A DOM window receives a focus event (DOMWindow::focus)
void BrowserWebViewDelegate::didFocus() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    bool handled = false;
    CefRefPtr<CefClient> client = browser_->GetClient();
    if (client.get()) {
      CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
      if (handler.get())
        handled = handler->OnSetFocus(browser_, FOCUS_SOURCE_WIDGET);
    }

    if (!handled)
      browser_->UIT_SetFocus(host, true);
  }
}

// This method is called when a DOM window receives a blur event
// (DOMWindow::blur).
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
  if (WebWidgetHost* host = GetWidgetHost()) {
    WebScreenInfo info = host->GetScreenInfo();

    if (browser_->IsWindowRenderingDisabled()) {
      // Retrieve the screen rectangle from the handler.
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
        if (handler.get()) {
          CefRect rect(info.rect.x, info.rect.y, info.rect.width,
                       info.rect.height);
          if (handler->GetScreenRect(browser_, rect)) {
            info.rect = WebRect(rect.x, rect.y, rect.width, rect.height);
            info.availableRect = info.rect;
          }
        }
      }
    }

    return info;
  }

  return WebScreenInfo();
}

// WebFrameClient ------------------------------------------------------------

WebPlugin* BrowserWebViewDelegate::createPlugin(
    WebFrame* frame, const WebPluginParams& params) {
  bool allow_wildcard = true;
  std::vector<webkit::WebPluginInfo> plugins;
  std::vector<std::string> mime_types;
  webkit::npapi::PluginList::Singleton()->GetPluginInfoArray(
      params.url, params.mimeType.utf8(), allow_wildcard,
      NULL, &plugins, &mime_types);
  if (plugins.empty())
    return NULL;

#if defined(OS_MACOSX)
  // Mac does not supported windowed plugins.
  bool force_windowless = true;
#else
  bool force_windowless = browser_->IsWindowRenderingDisabled();
#endif

  if (force_windowless) {
    DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());

    std::string mime_type = mime_types.front();
    bool flash = LowerCaseEqualsASCII(mime_type,
                                      "application/x-shockwave-flash");
    bool silverlight = StartsWithASCII(mime_type,
                                       "application/x-silverlight", false);

    if (flash) {
      // "wmode" values of "opaque" or "transparent" are allowed.
      size_t size = params.attributeNames.size();
      for (size_t i = 0; i < size; ++i) {
        std::string name = params.attributeNames[i].utf8();
        if (name == "wmode") {
          std::string value = params.attributeValues[i].utf8();
          if (value == "opaque" || value == "transparent")
            flash = false;
          break;
        }
      }
    }

    if (flash || silverlight) {
      WebPluginParams params_copy = params;
      params_copy.mimeType = WebString::fromUTF8(mime_type);

      // Force Flash and Silverlight plugins to use windowless mode.
      size_t size = params_copy.attributeNames.size();

      WebVector<WebString> new_names(size+1),  new_values(size+1);

      for (size_t i = 0; i < size; ++i) {
        new_names[i] = params_copy.attributeNames[i];
        new_values[i] = params_copy.attributeValues[i];
      }

      if (flash) {
        new_names[size] = "wmode";
        new_values[size] = "opaque";
      } else if (silverlight) {
        new_names[size] = "windowless";
        new_values[size] = "true";
      }

      params_copy.attributeNames.swap(new_names);
      params_copy.attributeValues.swap(new_values);

      return new webkit::npapi::WebPluginImpl(
          frame, params_copy, plugins.front().path, AsWeakPtr());
    }
  }

  return new webkit::npapi::WebPluginImpl(
      frame, params, plugins.front().path, AsWeakPtr());
}

WebApplicationCacheHost* BrowserWebViewDelegate::createApplicationCacheHost(
    WebFrame* frame, WebApplicationCacheHostClient* client) {
  return BrowserAppCacheSystem::CreateApplicationCacheHost(client);
}

WebKit::WebCookieJar* BrowserWebViewDelegate::cookieJar(WebFrame* frame) {
  return &cookie_jar_;
}

void BrowserWebViewDelegate::willClose(WebFrame* frame) {
  browser_->UIT_BeforeFrameClosed(frame);
}

void BrowserWebViewDelegate::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  DCHECK_NE(policy, WebKit::WebNavigationPolicyCurrentTab);
  std::string url = request.url().spec().data();
  CefRefPtr<CefBrowserImpl> newBrowser =
      browser_->UIT_CreatePopupWindow(url, CefPopupFeatures());
  if (newBrowser.get() && !url.empty())
    newBrowser->GetMainFrame()->LoadURL(url);
}

WebNavigationPolicy BrowserWebViewDelegate::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationType type, const WebNode& originating_node,
    WebNavigationPolicy default_policy, bool is_redirect) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
    if (handler.get()) {
      // Gather browse request information
      CefRefPtr<CefRequest> req(CefRequest::CreateRequest());

      GURL request_url = request.url();
      if (!request_url.is_valid())
        return WebKit::WebNavigationPolicyIgnore;

      req->SetURL(request_url.spec());
      req->SetMethod(string16(request.httpMethod()));

      const WebKit::WebHTTPBody& httpBody = request.httpBody();
      if (!httpBody.isNull()) {
        CefRefPtr<CefPostData> postdata(CefPostData::CreatePostData());
        static_cast<CefPostDataImpl*>(postdata.get())->Set(httpBody);
        req->SetPostData(postdata);
      }

      CefRequest::HeaderMap map;
      CefRequestImpl::GetHeaderMap(request, map);
      if (map.size() > 0)
        static_cast<CefRequestImpl*>(req.get())->SetHeaderMap(map);

      cef_handler_navtype_t navType;
      if (browser_->is_dropping())
        navType = NAVTYPE_LINKDROPPED;
      else
        navType = (cef_handler_navtype_t)type;

      // Notify the handler of a browse request
      bool handled = handler->OnBeforeBrowse(browser_,
          browser_->UIT_GetCefFrame(frame), req, navType,
          is_redirect);
      if (handled)
        return WebKit::WebNavigationPolicyIgnore;
    }
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

  if (frame->parent() == 0) {
    GURL url = ds->request().url();
    if (!url.is_empty())
      browser_->set_pending_url(url);
  }
}

void BrowserWebViewDelegate::didStartProvisionalLoad(WebFrame* frame) {
}

void BrowserWebViewDelegate::didReceiveServerRedirectForProvisionalLoad(
    WebFrame* frame) {
}

void BrowserWebViewDelegate::didFailProvisionalLoad(
    WebFrame* frame, const WebURLError& error) {
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

  bool handled = false;

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefLoadHandler> handler = client->GetLoadHandler();
    if (handler.get()) {
      // give the handler an opportunity to generate a custom error message
      handled = handler->OnLoadError(browser_,
          browser_->UIT_GetCefFrame(frame),
          static_cast<cef_handler_errorcode_t>(error.reason),
          std::string(failed_ds->request().url().spec().data()), errorStr);
    }
  }

  if (handled && !errorStr.empty()) {
    error_text = errorStr;
  } else {
    error_text = base::StringPrintf("Error %d when loading url %s",
        error.reason, failed_ds->request().url().spec().data());
  }

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  frame->loadHTMLString(
      error_text, GURL("cef-error:"), error.unreachableURL, false);

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

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefLoadHandler> handler = client->GetLoadHandler();
    if (handler.get()) {
      // Notify the handler that loading has started.
      handler->OnLoadStart(browser_, browser_->UIT_GetCefFrame(frame));
    }
  }

  // Apply zoom settings only on top-level frames.
  if (is_main_frame) {
    // Restore the zoom value that we have for this URL, if any.
    GURL url = frame->document().url();
    double zoomLevel = 0.0;
    bool didGetCustomZoom = false;
    if (client.get()) {
      CefRefPtr<CefZoomHandler> handler = client->GetZoomHandler();
      if (handler.get()) {
        double newZoomLevel = zoomLevel;
        didGetCustomZoom =
            handler->OnGetZoomLevel(browser_, url.spec(), newZoomLevel);
        if (didGetCustomZoom)
          zoomLevel = newZoomLevel;
      }
    }
    if (!didGetCustomZoom)
      ZoomMap::GetInstance()->get(url, zoomLevel);
    frame->view()->setZoomLevel(false, zoomLevel);
    browser_->set_zoom_level(zoomLevel);
  }
}

void BrowserWebViewDelegate::didCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int extensionGroup,
    int worldId) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (!client.get())
    return;

  CefRefPtr<CefV8ContextHandler> handler = client->GetV8ContextHandler();
  if (!handler.get())
    return;

  v8::HandleScope handle_scope;
  v8::Context::Scope scope(context);

  CefRefPtr<CefFrame> framePtr(browser_->UIT_GetCefFrame(frame));
  CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));

  handler->OnContextCreated(browser_, framePtr, contextPtr);
}

void BrowserWebViewDelegate::willReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int worldId) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (!client.get())
    return;

  CefRefPtr<CefV8ContextHandler> handler = client->GetV8ContextHandler();
  if (!handler.get())
    return;

  v8::HandleScope handle_scope;
  v8::Context::Scope scope(context);

  CefRefPtr<CefFrame> framePtr(browser_->UIT_GetCefFrame(frame));
  CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));

  handler->OnContextReleased(browser_, framePtr, contextPtr);
}

void BrowserWebViewDelegate::didReceiveTitle(
    WebFrame* frame, const WebString& title, WebTextDirection direction) {
  bool is_main_frame = (frame->parent() == 0);
  if (is_main_frame) {
    CefString titleStr = string16(title);
    browser_->UIT_SetTitle(titleStr);
    CefRefPtr<CefClient> client = browser_->GetClient();
    if (client.get()) {
      CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
      if (handler.get()) {
        // Notify the handler of a page title change
        handler->OnTitleChange(browser_, titleStr);
      }
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

void BrowserWebViewDelegate::didNavigateWithinPage(
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

void BrowserWebViewDelegate::didChangeContentsSize(
    WebFrame* frame, const WebSize& size) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler.get()) {
      handler->OnContentsSizeChange(browser_, browser_->UIT_GetCefFrame(frame),
          size.width, size.height);
    }
  }
}

void BrowserWebViewDelegate::reportFindInPageMatchCount(
    int request_id, int count, bool final_update) {
  browser_->UIT_NotifyFindStatus(request_id, count, gfx::Rect(),
      -1,  // Don't update active match ordinal.
      final_update);
}

void BrowserWebViewDelegate::reportFindInPageSelection(
    int request_id, int active_match_ordinal, const WebKit::WebRect& sel) {
  browser_->UIT_NotifyFindStatus(request_id, -1, sel, active_match_ordinal,
      false);
}

void BrowserWebViewDelegate::openFileSystem(
    WebFrame* frame, WebFileSystem::Type type,
    long long size,  // NOLINT(runtime/int)
    bool create,
    WebFileSystemCallbacks* callbacks) {
  BrowserFileSystem* fileSystem = static_cast<BrowserFileSystem*>(
      WebKit::webKitPlatformSupport()->fileSystem());
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
#if defined(OS_WIN)
      destroy_on_drag_end_(false),
#endif
      smart_insert_delete_enabled_(true),
#if defined(OS_WIN)
      select_trailing_whitespace_enabled_(true),
#else
      select_trailing_whitespace_enabled_(false),
#endif
      block_redirects_(false),
      cookie_jar_(browser) {
}

BrowserWebViewDelegate::~BrowserWebViewDelegate() {
}

void BrowserWebViewDelegate::Reset() {
  // Do a little placement new dance...
  CefBrowserImpl* browser = browser_;
  this->~BrowserWebViewDelegate();
  new (this)BrowserWebViewDelegate(browser);  // NOLINT(whitespace/parens)
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

bool BrowserWebViewDelegate::OnKeyboardEvent(
    const WebKit::WebKeyboardEvent& event, bool isAfterJavaScript) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefKeyboardHandler> handler;
  if (client.get())
    handler = client->GetKeyboardHandler();
  if (!handler.get())
    return false;

  CefKeyboardHandler::KeyEventType eventType;
  switch (event.type) {
  case WebKeyboardEvent::RawKeyDown:
    eventType = KEYEVENT_RAWKEYDOWN;
    break;
  case WebKeyboardEvent::KeyDown:
    eventType = KEYEVENT_KEYDOWN;
    break;
  case WebKeyboardEvent::KeyUp:
    eventType = KEYEVENT_KEYUP;
    break;
  case WebKeyboardEvent::Char:
    eventType = KEYEVENT_CHAR;
    break;
  default:
    return false;
  }

  return handler->OnKeyEvent(browser_, eventType, event.windowsKeyCode,
      event.modifiers, event.isSystemKey?true:false, isAfterJavaScript);
}

void BrowserWebViewDelegate::ShowStatus(const WebString& text,
                                        cef_handler_statustype_t type) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler.get()) {
      CefString textStr = string16(text);
      handler->OnStatusMessage(browser_, textStr, type);
    }
  }
}

void BrowserWebViewDelegate::LocationChangeDone(WebFrame* frame) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (!client.get())
    return;

  bool is_main_frame = (frame->parent() == 0);
  if (is_main_frame) {
    CefString title = browser_->UIT_GetTitle();
    if (title.empty()) {
      // No title was provided by the page, so send a blank string to the
      // client.
      CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
      if (handler.get())
        handler->OnTitleChange(browser_, title);
    }
  }

  CefRefPtr<CefLoadHandler> handler = client->GetLoadHandler();
  if (handler.get()) {
    // Notify the handler that loading has ended.
    int httpStatusCode = frame->dataSource()->response().httpStatusCode();
    handler->OnLoadEnd(browser_, browser_->UIT_GetCefFrame(frame),
        httpStatusCode);
  }
}

WebWidgetHost* BrowserWebViewDelegate::GetWidgetHost() {
  if (this == browser_->UIT_GetWebViewDelegate())
    return browser_->UIT_GetWebViewHost();
  if (this == browser_->UIT_GetPopupDelegate())
    return browser_->UIT_GetPopupHost();
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
  BrowserNavigationController* controller =
      browser_->UIT_GetNavigationController();

  if (controller->GetEntryCount() == 0) {
    // This is the first navigation for the browser. Indicate that the browser
    // now has a document.
    browser_->set_has_document(true);
  }

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

  // Update attributes of the CefFrame if it currently exists.
  browser_->UIT_UpdateCefFrame(frame);

  bool is_main_frame = (frame->parent() == 0);
  CefRefPtr<CefClient> client = browser_->GetClient();

  if (is_main_frame && client.get()) {
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler.get()) {
      // Notify the handler of an address change
      std::string url = std::string(entry->GetURL().spec().c_str());
      handler->OnAddressChange(browser_, browser_->UIT_GetCefFrame(frame), url);
    }
  }

  const WebHistoryItem& history_item = frame->currentHistoryItem();
  if (!history_item.isNull())
    entry->SetContentState(webkit_glue::HistoryItemToString(history_item));

  bool old_can_go_back = !controller->IsAtStart();
  bool old_can_go_forward = !controller->IsAtEnd();
  controller->DidNavigateToEntry(entry.release());
  bool new_can_go_back = !controller->IsAtStart();
  bool new_can_go_forward = !controller->IsAtEnd();

  last_page_id_updated_ = std::max(last_page_id_updated_, page_id_);

  if (old_can_go_back != new_can_go_back ||
      old_can_go_forward != new_can_go_forward) {
    browser_->set_nav_state(new_can_go_back, new_can_go_forward);
    CefRefPtr<CefDisplayHandler> handler = client->GetDisplayHandler();
    if (handler.get()) {
      // Notify the handler of a navigation state change
      handler->OnNavStateChange(browser_, new_can_go_back, new_can_go_forward);
    }
  }
}

void BrowserWebViewDelegate::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  BrowserNavigationEntry* entry =
      browser_->UIT_GetNavigationController()->GetEntryWithPageID(page_id_);
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

bool BrowserWebViewDelegate::OnBeforeMenu(
    const WebKit::WebContextMenuData& data, int mouse_x, int mouse_y,
    int& edit_flags, int& type_flags) {
  // Populate the edit flags values.
  edit_flags = data.editFlags;
  if (browser_->UIT_CanGoBack())
    edit_flags |= MENU_CAN_GO_BACK;
  if (browser_->UIT_CanGoForward())
    edit_flags |= MENU_CAN_GO_FORWARD;

  // Populate the type flags values.
  type_flags = MENUTYPE_NONE;
  if (!data.pageURL.isEmpty())
    type_flags |= MENUTYPE_PAGE;
  if (!data.frameURL.isEmpty())
    type_flags |= MENUTYPE_FRAME;
  if (!data.linkURL.isEmpty())
    type_flags |= MENUTYPE_LINK;
  if (data.mediaType == WebContextMenuData::MediaTypeImage)
    type_flags |= MENUTYPE_IMAGE;
  if (!data.selectedText.isEmpty())
    type_flags |= MENUTYPE_SELECTION;
  if (data.isEditable)
    type_flags |= MENUTYPE_EDITABLE;
  if (data.isSpellCheckingEnabled && !data.misspelledWord.isEmpty())
    type_flags |= MENUTYPE_MISSPELLED_WORD;
  if (data.mediaType == WebContextMenuData::MediaTypeVideo)
    type_flags |= MENUTYPE_VIDEO;
  if (data.mediaType == WebContextMenuData::MediaTypeAudio)
    type_flags |= MENUTYPE_AUDIO;

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefMenuHandler> handler;
  if (client.get())
    handler = client->GetMenuHandler();

  if (handler.get()) {
    // Gather menu information.
    CefMenuInfo menuInfo;

    CefString linkStr(std::string(data.linkURL.spec()));
    CefString imageStr(std::string(data.srcURL.spec()));
    CefString pageStr(std::string(data.pageURL.spec()));
    CefString frameStr(std::string(data.frameURL.spec()));
    CefString selectedTextStr(string16(data.selectedText));
    CefString misspelledWordStr(string16(data.misspelledWord));
    CefString securityInfoStr(std::string(data.securityInfo));

    menuInfo.typeFlags = type_flags;
    menuInfo.x = mouse_x;
    menuInfo.y = mouse_y;
    cef_string_set(linkStr.c_str(), linkStr.length(), &menuInfo.linkUrl, false);
    cef_string_set(imageStr.c_str(), imageStr.length(), &menuInfo.imageUrl,
                   false);
    cef_string_set(pageStr.c_str(), pageStr.length(), &menuInfo.pageUrl, false);
    cef_string_set(frameStr.c_str(), frameStr.length(), &menuInfo.frameUrl,
                   false);
    cef_string_set(selectedTextStr.c_str(), selectedTextStr.length(),
                   &menuInfo.selectionText, false);
    cef_string_set(misspelledWordStr.c_str(), misspelledWordStr.length(),
                   &menuInfo.misspelledWord, false);
    menuInfo.editFlags = edit_flags;
    cef_string_set(securityInfoStr.c_str(), securityInfoStr.length(),
                   &menuInfo.securityInfo, false);

    // Notify the handler that a context menu is requested.
    if (handler->OnBeforeMenu(browser_, menuInfo))
      return true;
  }

  return false;
}
