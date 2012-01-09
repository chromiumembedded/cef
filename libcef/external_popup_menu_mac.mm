// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/external_popup_menu_mac.h"
#include "libcef/browser_impl.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebExternalPopupMenuClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenuInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "webkit/glue/webmenurunner_mac.h"

ExternalPopupMenu::ExternalPopupMenu(
    BrowserWebViewDelegate* delegate,
    const WebKit::WebPopupMenuInfo& popup_menu_info,
    WebKit::WebExternalPopupMenuClient* popup_menu_client)
    : delegate_(delegate),
      popup_menu_info_(popup_menu_info),
      popup_menu_client_(popup_menu_client) {
}

void ExternalPopupMenu::show(const WebKit::WebRect& bounds) {
  // Display a HTML select menu.

  std::vector<WebMenuItem> items;
  for (size_t i = 0; i < popup_menu_info_.items.size(); ++i)
    items.push_back(popup_menu_info_.items[i]);

  double font_size = popup_menu_info_.itemFontSize;
  int selected_index = popup_menu_info_.selectedIndex;
  bool right_aligned = popup_menu_info_.rightAligned;

  CefBrowserImpl* browser = delegate_->GetBrowser();

  // Set up the menu position.
  NSView* web_view = browser->UIT_GetWebViewWndHandle();
  NSRect view_rect = [web_view bounds];
  int y_offset = bounds.y + bounds.height;
  NSRect position = NSMakeRect(bounds.x, view_rect.size.height - y_offset,
                               bounds.width, bounds.height);

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items
                                                fontSize:font_size
                                            rightAligned:right_aligned]);

  [menu_runner runMenuInView:browser->UIT_GetWebViewWndHandle()
                  withBounds:position
                initialIndex:selected_index];

  if ([menu_runner menuItemWasChosen])
    popup_menu_client_->didAcceptIndex([menu_runner indexOfSelectedItem]);
  else
    popup_menu_client_->didCancel();

  delegate_->ClosePopupMenu();
}

void ExternalPopupMenu::close()  {
  popup_menu_client_ = NULL;
  delegate_ = NULL;
}
