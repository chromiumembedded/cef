// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_H_
#pragma once

#include <set>
#include <string>

#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#include "cefclient/dialog_handler_gtk.h"

// The Linux client uses GTK instead of the underlying platform type (X11).
#define ClientWindowHandle GtkWidget*
#else
#define ClientWindowHandle CefWindowHandle
#endif

namespace client {

// Client handler abstract base class. Provides common functionality shared by
// all concrete client handler implementations.
class ClientHandler : public CefClient,
                      public CefContextMenuHandler,
                      public CefDisplayHandler,
                      public CefDownloadHandler,
                      public CefDragHandler,
                      public CefGeolocationHandler,
                      public CefKeyboardHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefRequestHandler {
 public:
  typedef std::set<CefMessageRouterBrowserSide::Handler*> MessageHandlerSet;

  ClientHandler(const std::string& startup_url,
                bool is_osr);
  ~ClientHandler();

  // CefClient methods
  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefDragHandler> GetDragHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefGeolocationHandler> GetGeolocationHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) OVERRIDE;

#if defined(OS_LINUX)
  CefRefPtr<CefDialogHandler> GetDialogHandler() OVERRIDE {
    return dialog_handler_;
  }
  CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() OVERRIDE {
    return dialog_handler_;
  }
#endif

  // CefContextMenuHandler methods
  void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefContextMenuParams> params,
                           CefRefPtr<CefMenuModel> model) OVERRIDE;
  bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefContextMenuParams> params,
                            int command_id,
                            EventFlags event_flags) OVERRIDE;

  // CefDisplayHandler methods
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) OVERRIDE;
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) OVERRIDE;
  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                        const CefString& message,
                        const CefString& source,
                        int line) OVERRIDE;

  // CefDownloadHandler methods
  void OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) OVERRIDE;
  void OnDownloadUpdated(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      CefRefPtr<CefDownloadItemCallback> callback) OVERRIDE;

  // CefDragHandler methods
  bool OnDragEnter(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefDragData> dragData,
                   CefDragHandler::DragOperationsMask mask) OVERRIDE;

  // CefGeolocationHandler methods
  bool OnRequestGeolocationPermission(
      CefRefPtr<CefBrowser> browser,
      const CefString& requesting_url,
      int request_id,
      CefRefPtr<CefGeolocationCallback> callback) OVERRIDE;

  // CefKeyboardHandler methods
  bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                     const CefKeyEvent& event,
                     CefEventHandle os_event,
                     bool* is_keyboard_shortcut) OVERRIDE;

  // CefLifeSpanHandler methods
  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     bool* no_javascript_access) OVERRIDE;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  bool DoClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // CefLoadHandler methods
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) OVERRIDE;
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) OVERRIDE;

  // CefRequestHandler methods
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool is_redirect) OVERRIDE;
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE;
  bool OnQuotaRequest(CefRefPtr<CefBrowser> browser,
                      const CefString& origin_url,
                      int64 new_size,
                      CefRefPtr<CefQuotaCallback> callback) OVERRIDE;
  void OnProtocolExecution(CefRefPtr<CefBrowser> browser,
                           const CefString& url,
                           bool& allow_os_execution) OVERRIDE;
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status) OVERRIDE;

  // Set the main frame window handle.
  void SetMainWindowHandle(ClientWindowHandle handle);

  // Get the main frame window handle. Can only be called on the CEF UI thread.
  ClientWindowHandle GetMainWindowHandle() const;

  // Returns the number of browsers currently using this handler. Can only be
  // called on the CEF UI thread.
  int GetBrowserCount() const;

  // Show a new DevTools popup window.
  void ShowDevTools(CefRefPtr<CefBrowser> browser,
                    const CefPoint& inspect_element_at);

  // Close the existing DevTools popup window, if any.
  void CloseDevTools(CefRefPtr<CefBrowser> browser);

  // Returns the startup URL.
  std::string startup_url() const { return startup_url_; }

  // Returns true if this handler uses off-screen rendering.
  bool is_osr() const { return is_osr_; }

 protected:
  // The following virtual methods are called on the CEF UI thread unless
  // otherwise indicated.

  // A browser has been created.
  virtual void BrowserCreated(CefRefPtr<CefBrowser> browser) = 0;

  // A browser is closing.
  virtual void BrowserClosing(CefRefPtr<CefBrowser> browser) = 0;

  // A browser has been closed.
  virtual void BrowserClosed(CefRefPtr<CefBrowser> browser) = 0;

  // Set the window URL address.
  virtual void SetAddress(CefRefPtr<CefBrowser> browser,
                          const CefString& url) = 0;

  // Set the window title.
  virtual void SetTitle(CefRefPtr<CefBrowser> browser,
                        const CefString& title) = 0;

  // Set the loading state.
  virtual void SetLoadingState(CefRefPtr<CefBrowser> browser,
                               bool isLoading,
                               bool canGoBack,
                               bool canGoForward) = 0;

  // Create a new popup window using the specified information. |is_devtools|
  // will be true if the window will be used for DevTools. Return true to
  // proceed with popup browser creation or false to cancel the popup browser.
  // May be called on any thead.
  virtual bool CreatePopupWindow(
      bool is_devtools,
      const CefPopupFeatures& popupFeatures,
      CefWindowInfo& windowInfo,
      CefRefPtr<CefClient>& client,
      CefBrowserSettings& settings) = 0;

 private:
  // Test context menu creation.
  void BuildTestMenu(CefRefPtr<CefMenuModel> model);
  bool ExecuteTestMenu(int command_id);

  // THREAD SAFE MEMBERS
  // The following members may be accessed from any thread.

  // The startup URL.
  const std::string startup_url_;

  // True if this handler uses off-screen rendering.
  const bool is_osr_;

  // True if mouse cursor change is disabled.
  bool mouse_cursor_change_disabled_;

#if defined(OS_LINUX)
  // Custom dialog handler for GTK.
  CefRefPtr<ClientDialogHandlerGtk> dialog_handler_;
#endif

  // Handles the browser side of query routing. The renderer side is handled
  // in client_renderer.cc.
  CefRefPtr<CefMessageRouterBrowserSide> message_router_;
  
  // UI THREAD MEMBERS
  // The following members will only be accessed on the CEF UI thread.

  // Track state information for the text context menu.
  struct TestMenuState {
    TestMenuState() : check_item(true), radio_item(0) {}
    bool check_item;
    int radio_item;
  } test_menu_state_;

  // The current number of browsers using this handler.
  int browser_count_;

  // The main frame window handle.
  ClientWindowHandle main_handle_;

  // Console logging state.
  const std::string console_log_file_;
  bool first_console_message_;

  // True if an editable field currently has focus.
  bool focus_on_editable_field_;

  // Set of Handlers registered with the message router.
  MessageHandlerSet message_handler_set_;

  DISALLOW_COPY_AND_ASSIGN(ClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_H_
