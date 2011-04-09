// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _EXTERNAL_POPUP_MENU_MAC_H
#define _EXTERNAL_POPUP_MENU_MAC_H

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExternalPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenuInfo.h"

class BrowserWebViewDelegate;
namespace WebKit {
class WebExternalPopupMenuClient;
}

class ExternalPopupMenu : public WebKit::WebExternalPopupMenu {
 public:
  ExternalPopupMenu(BrowserWebViewDelegate* delegate,
                    const WebKit::WebPopupMenuInfo& popup_menu_info,
                    WebKit::WebExternalPopupMenuClient* popup_menu_client);

  // WebKit::WebExternalPopupMenu implementation:
  virtual void show(const WebKit::WebRect& bounds);
  virtual void close();

 private:
  BrowserWebViewDelegate* delegate_;
  WebKit::WebPopupMenuInfo popup_menu_info_;
  WebKit::WebExternalPopupMenuClient* popup_menu_client_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPopupMenu);
};

#endif  // _EXTERNAL_POPUP_MENU_MAC_H
