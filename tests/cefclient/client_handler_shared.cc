// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler_shared.h"

#include <stdio.h>
#include <algorithm>
#include <string>

#include "include/base/cef_bind.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_closure_task.h"
#include "cefclient/client_switches.h"
#include "cefclient/main_context.h"
#include "cefclient/main_message_loop.h"
#include "cefclient/test_runner.h"

namespace client {

ClientHandlerShared::ClientHandlerShared()
  : ClientHandler(MainContext::Get()->GetMainURL(),
                  CefCommandLine::GetGlobalCommandLine()->HasSwitch(
                      switches::kOffScreenRenderingEnabled)),
    is_closing_(false),
    browser_id_(0),
    edit_handle_(NULL),
    back_handle_(NULL),
    forward_handle_(NULL),
    stop_handle_(NULL),
    reload_handle_(NULL) {
}

ClientHandlerShared::~ClientHandlerShared() {
}

void ClientHandlerShared::BrowserCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (browser_id_ == 0)   {
    // Keep references to the browser hosted in the main window.
    browser_id_ = browser->GetIdentifier();
    {
      // Protect modification of |browser_| with a lock because it may be
      // accessed from different threads.
      base::AutoLock lock_scope(lock_);
      browser_ = browser;
    }
  } else if (browser->IsPopup()) {
    // Add to the list of popup browsers.
    popup_browsers_.push_back(browser);

    // Give focus to the popup browser. Perform asynchronously because the
    // parent window may attempt to keep focus after launching the popup.
    CefPostTask(TID_UI,
        base::Bind(&CefBrowserHost::SetFocus, browser->GetHost().get(), true));
  }
}

void ClientHandlerShared::BrowserClosing(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Closing the main window requires special handling. See the DoClose()
  // documentation in the CEF header for a detailed destription of this
  // process.
  if (browser_id_ == browser->GetIdentifier()) {
    // Protect modification of |is_closing_| with a lock because it may be
    // accessed from different threads.
    base::AutoLock lock_scope(lock_);
    // Set a flag to indicate that the window close should be allowed.
    is_closing_ = true;
  }
}

void ClientHandlerShared::BrowserClosed(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (browser_id_ == browser->GetIdentifier()) {
    {
      // Protect modification of |browser_| with a lock because it may be
      // accessed from different threads.
      base::AutoLock lock_scope(lock_);
      // Free the browser pointer so that the browser can be destroyed.
      browser_ = NULL;
    }

    if (osr_handler_.get()) {
      osr_handler_->OnBeforeClose(browser);

      // Protect modification of |osr_handler_| with a lock because it may be
      // accessed from different threads.
      base::AutoLock lock_scope(lock_);
      osr_handler_ = NULL;
    }

    browser_id_ = 0;
  } else if (browser->IsPopup()) {
    // Remove from the browser popup list.
    BrowserList::iterator bit = popup_browsers_.begin();
    for (; bit != popup_browsers_.end(); ++bit) {
      if ((*bit)->IsSame(browser)) {
        popup_browsers_.erase(bit);
        break;
      }
    }
  }
  
  if (GetBrowserCount() == 0) {
    // Quit the main message loop.
    MainMessageLoop::Get()->Quit();
  }
}

bool ClientHandlerShared::CreatePopupWindow(
    bool is_devtools,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings) {
  // Note: This method will be called on multiple threads.

  if (is_devtools) {
    // Create DevTools as a windowed popup browser using the same client.
#if defined(OS_WIN)
    windowInfo.SetAsPopup(GetMainWindowHandle(), "DevTools");
#endif
    client = this;
  } else if (is_osr()) {
    CefPostTask(TID_UI,
        base::Bind(test_runner::Alert, GetBrowser(),
                   "Popup windows are disabled with off-screen rendering."));

    // Cancel popups in off-screen rendering mode.
    return false;
  }

  return true;
}

void ClientHandlerShared::SetUXWindowHandles(
   ClientWindowHandle editHandle,
   ClientWindowHandle backHandle,
   ClientWindowHandle forwardHandle,
   ClientWindowHandle reloadHandle,
   ClientWindowHandle stopHandle) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI,
        base::Bind(&ClientHandlerShared::SetUXWindowHandles, this,
                   editHandle, backHandle, forwardHandle, reloadHandle,
                   stopHandle));
    return;
  }

  edit_handle_ = editHandle;
  back_handle_ = backHandle;
  forward_handle_ = forwardHandle;
  reload_handle_ = reloadHandle;
  stop_handle_ = stopHandle;
}

CefRefPtr<ClientHandlerShared::RenderHandler>
    ClientHandlerShared::GetOSRHandler() const {
  if (CefCurrentlyOn(TID_UI)) {
    // No need for locking because |osr_handler_| will only be modified on this
    // thread.
    return osr_handler_;
  } else {
    // Use a lock to protect |osr_handler_| from being modified while we're
    // accessing it from a different thread.
    base::AutoLock lock_scope(lock_);
    return osr_handler_;
  }
}

void ClientHandlerShared::SetOSRHandler(CefRefPtr<RenderHandler> handler) {
  // Protect modification of |osr_handler_| with a lock because it may be
  // accessed from different threads.
  base::AutoLock lock_scope(lock_);
  osr_handler_ = handler;
}

CefRefPtr<CefBrowser> ClientHandlerShared::GetBrowser() const {
  if (CefCurrentlyOn(TID_UI)) {
    // No need for locking because |browser_| will only be modified on this
    // thread.
    return browser_;
  } else {
    // Use a lock to protect |browser_| from being modified while we're
    // accessing it from a different thread.
    base::AutoLock lock_scope(lock_);
    return browser_;
  }
}

int ClientHandlerShared::GetBrowserId() const {
  CEF_REQUIRE_UI_THREAD();
  return browser_id_;
}

void ClientHandlerShared::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI,
        base::Bind(&ClientHandlerShared::CloseAllBrowsers, this, force_close));
    return;
  }

  if (!popup_browsers_.empty()) {
    // Request that any popup browsers close.
    BrowserList::const_iterator it = popup_browsers_.begin();
    for (; it != popup_browsers_.end(); ++it)
      (*it)->GetHost()->CloseBrowser(force_close);
  }

  if (browser_.get()) {
    // Request that the main browser close.
    browser_->GetHost()->CloseBrowser(force_close);
  }
}

bool ClientHandlerShared::IsClosing() const {
  if (CefCurrentlyOn(TID_UI)) {
    // No need for locking because |is_closing_| will only be modified on this
    // thread.
    return is_closing_;
  } else {
    // Use a lock to protect |is_closing_| from being modified while we're
    // accessing it from a different thread.
    base::AutoLock lock_scope(lock_);
    return is_closing_;
  }
}

}  // namespace client
