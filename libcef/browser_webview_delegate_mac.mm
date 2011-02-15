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
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenu;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

// WebViewClient --------------------------------------------------------------

WebWidget* BrowserWebViewDelegate::createPopupMenu(
    const WebPopupMenuInfo& info) {
  WebWidget* webwidget = browser_->UIT_CreatePopupWidget();
  browser_->UIT_GetPopupDelegate()->SetPopupMenuInfo(info);
  return webwidget;
}

void BrowserWebViewDelegate::showContextMenu(
    WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data) {
  NOTIMPLEMENTED();
}

// WebWidgetClient ------------------------------------------------------------

void BrowserWebViewDelegate::show(WebNavigationPolicy policy) {
  if (!popup_menu_info_.get())
    return;
  if (this != browser_->UIT_GetPopupDelegate())
    return;
  // Display a HTML select menu.

  std::vector<WebMenuItem> items;
  for (size_t i = 0; i < popup_menu_info_->items.size(); ++i)
    items.push_back(popup_menu_info_->items[i]);

  int item_height = popup_menu_info_->itemHeight;
  double font_size = popup_menu_info_->itemFontSize;
  int selected_index = popup_menu_info_->selectedIndex;
  bool right_aligned = popup_menu_info_->rightAligned;
  popup_menu_info_.reset();  // No longer needed.

  const WebRect& bounds = popup_bounds_;

  // Set up the menu position.
  NSView* web_view = browser_->UIT_GetWebViewWndHandle();
  NSRect view_rect = [web_view bounds];
  int y_offset = bounds.y + bounds.height;
  NSRect position = NSMakeRect(bounds.x, view_rect.size.height - y_offset,
                               bounds.width, bounds.height);

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items
                                                fontSize:font_size
                                            rightAligned:right_aligned]);

  [menu_runner runMenuInView:browser_->UIT_GetWebViewWndHandle()
                  withBounds:position
                initialIndex:selected_index];

  // Get the selected item and forward to WebKit. WebKit expects an input event
  // (mouse down, keyboard activity) for this, so we calculate the proper
  // position based on the selected index and provided bounds.
  WebWidgetHost* popup = browser_->UIT_GetPopupHost();
  NSWindow* window = [browser_->UIT_GetMainWndHandle() window];
  int window_num = [window windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    popup->MouseEvent(event);
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    popup->KeyEvent(event);
  }

  browser_->UIT_ClosePopupWidget();
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
  } else if (this == browser_->UIT_GetPopupDelegate()) {
    popup_bounds_ = rect;  // The initial position of the popup.
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

void BrowserWebViewDelegate::SetPopupMenuInfo(const WebPopupMenuInfo& info) {
  popup_menu_info_.reset(new WebPopupMenuInfo(info));
}

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
