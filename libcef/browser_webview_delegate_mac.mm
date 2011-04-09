// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_webview_delegate.h"
#include "browser_impl.h"

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/glue/webmenurunner_mac.h"

using WebKit::WebCursorInfo;
using WebKit::WebExternalPopupMenu;
using WebKit::WebExternalPopupMenuClient;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

// WebViewClient --------------------------------------------------------------

WebExternalPopupMenu* BrowserWebViewDelegate::createExternalPopupMenu(
    const WebPopupMenuInfo& info,
    WebExternalPopupMenuClient* client) {
  DCHECK(!external_popup_menu_.get());
  external_popup_menu_.reset(new ExternalPopupMenu(this, info, client));
  return external_popup_menu_.get();
}

void BrowserWebViewDelegate::ClosePopupMenu() {
  if (external_popup_menu_ == NULL) {
    NOTREACHED();
    return;
  }
  external_popup_menu_.reset();
}

void BrowserWebViewDelegate::showContextMenu(
    WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data) {
  NOTIMPLEMENTED();
}

// WebWidgetClient ------------------------------------------------------------

void BrowserWebViewDelegate::show(WebNavigationPolicy policy) {
}

void BrowserWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  NSCursor* ns_cursor = WebCursor(cursor_info).GetCursor();
  [ns_cursor set];
}

WebRect BrowserWebViewDelegate::windowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    NSView *view = host->view_handle();
    NSRect rect = [view frame];
    return gfx::Rect(NSRectToCGRect(rect));
  }
  return WebRect();
}

void BrowserWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == browser_->UIT_GetWebViewDelegate()) {
    // TODO(port): Set the window rectangle.
  }
}

WebRect BrowserWebViewDelegate::rootWindowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    NSView *view = host->view_handle();
    NSRect rect = [[[view window] contentView] frame];
    return gfx::Rect(NSRectToCGRect(rect));
  }
  return WebRect();
}

@interface NSWindow(OSInternals)
- (NSRect)_growBoxRect;
@end

WebRect BrowserWebViewDelegate::windowResizerRect() {
  NSRect resize_rect = NSMakeRect(0, 0, 0, 0);
  WebWidgetHost* host = GetWidgetHost();
  if (host) {
    NSView *view = host->view_handle();
    NSWindow* window = [view window];
    resize_rect = [window _growBoxRect];
    // The scrollbar assumes that the resizer goes all the way down to the
    // bottom corner, so we ignore any y offset to the rect itself and use the
    // entire bottom corner.
    resize_rect.origin.y = 0;
    // Convert to view coordinates from window coordinates.
    resize_rect = [view convertRect:resize_rect fromView:nil];
    // Flip the rect in view coordinates
    resize_rect.origin.y =
        [view frame].size.height - resize_rect.origin.y -
        resize_rect.size.height;
  }
  return gfx::Rect(NSRectToCGRect(resize_rect));
}

void BrowserWebViewDelegate::runModal() {
  NOTIMPLEMENTED();
}

// WebPluginPageDelegate ------------------------------------------------------

webkit::npapi::WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
    const FilePath& path,
    const std::string& mime_type) {
  WebWidgetHost *host = GetWidgetHost();
  if (!host)
    return NULL;

  gfx::PluginWindowHandle containing_view = NULL;
  return webkit::npapi::WebPluginDelegateImpl::Create(
      path, mime_type, containing_view);
}

void BrowserWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle handle) {
}

void BrowserWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle handle) {
}

void BrowserWebViewDelegate::DidMovePlugin(
    const webkit::npapi::WebPluginGeometry& move) {
  // TODO(port): add me once plugins work.
}

// Protected methods ----------------------------------------------------------

void BrowserWebViewDelegate::ShowJavaScriptAlert(
    WebKit::WebFrame* webframe, const CefString& message) {
  std::string messageStr(message);
  NSString *text = [NSString stringWithUTF8String:messageStr.c_str()];
  NSAlert *alert = [NSAlert alertWithMessageText:@"JavaScript Alert"
                                   defaultButton:@"OK"
                                 alternateButton:nil
                                     otherButton:nil
                       informativeTextWithFormat:text];
  [alert runModal];
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(
    WebKit::WebFrame* webframe, const CefString& message) {
  NOTIMPLEMENTED();
  return false;
}

bool BrowserWebViewDelegate::ShowJavaScriptPrompt(
    WebKit::WebFrame* webframe, const CefString& message,
    const CefString& default_value, CefString* result) {
  NOTIMPLEMENTED();
  return false;
}

// Called to show the file chooser dialog.
bool BrowserWebViewDelegate::ShowFileChooser(std::vector<FilePath>& file_names,
                                             const bool multi_select,
                                             const WebKit::WebString& title,
                                             const FilePath& default_file) {
  NOTIMPLEMENTED();
  return false;
}
