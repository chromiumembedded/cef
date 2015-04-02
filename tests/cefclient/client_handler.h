// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <string>

#include "include/base/cef_lock.h"
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"

#if defined(OS_LINUX)
// The Linux client uses GTK instead of the underlying platform type (X11).
#include <gtk/gtk.h>
#define ClientWindowHandle GtkWidget*
#else
#define ClientWindowHandle CefWindowHandle
#endif

// Define this value to redirect all popup URLs to the main application browser
// window.
// #define TEST_REDIRECT_POPUP_URLS

namespace client {

// ClientHandler implementation.
class ClientHandler : public CefClient,
                      public CefContextMenuHandler,
                      public CefDisplayHandler,
                      public CefDownloadHandler,
                      public CefDragHandler,
                      public CefGeolocationHandler,
                      public CefKeyboardHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefRenderHandler,
                      public CefRequestHandler {
 public:
  // Interface implemented to handle off-screen rendering.
  class RenderHandler : public CefRenderHandler {
   public:
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) =0;
  };

  typedef std::set<CefMessageRouterBrowserSide::Handler*> MessageHandlerSet;

  ClientHandler();
  ~ClientHandler();

  // CefClient methods
  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefDialogHandler> GetDialogHandler() OVERRIDE {
    return dialog_handler_;
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
  CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() OVERRIDE {
    return jsdialog_handler_;
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
  CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE {
    return this;
  }
  CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) OVERRIDE;

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
                      CefRefPtr<CefRequestCallback> callback) OVERRIDE;
  void OnProtocolExecution(CefRefPtr<CefBrowser> browser,
                           const CefString& url,
                           bool& allow_os_execution) OVERRIDE;
  bool OnCertificateError(
      CefRefPtr<CefBrowser> browser,
      ErrorCode cert_error,
      const CefString& request_url,
      CefRefPtr<CefSSLInfo> ssl_info,
      CefRefPtr<CefRequestCallback> callback) OVERRIDE;
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status) OVERRIDE;

  // CefRenderHandler methods
  bool GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                         CefRect& rect) OVERRIDE;
  bool GetViewRect(CefRefPtr<CefBrowser> browser,
                   CefRect& rect) OVERRIDE;
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) OVERRIDE;
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) OVERRIDE;
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) OVERRIDE;
  void OnPopupSize(CefRefPtr<CefBrowser> browser,
                   const CefRect& rect) OVERRIDE;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) OVERRIDE;
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      CursorType type,
                      const CefCursorInfo& custom_cursor_info) OVERRIDE;
  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x, int y) OVERRIDE;
  void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                        CefRenderHandler::DragOperation operation) OVERRIDE;

  void SetMainWindowHandle(ClientWindowHandle handle);
  ClientWindowHandle GetMainWindowHandle() const;
  void SetEditWindowHandle(ClientWindowHandle handle);
  void SetButtonWindowHandles(ClientWindowHandle backHandle,
                              ClientWindowHandle forwardHandle,
                              ClientWindowHandle reloadHandle,
                              ClientWindowHandle stopHandle);

  void SetOSRHandler(CefRefPtr<RenderHandler> handler);
  CefRefPtr<RenderHandler> GetOSRHandler() const;

  CefRefPtr<CefBrowser> GetBrowser() const;
  int GetBrowserId() const;

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  // Returns true if the main browser window is currently closing. Used in
  // combination with DoClose() and the OS close notification to properly handle
  // 'onbeforeunload' JavaScript events during window close.
  bool IsClosing() const;

  void ShowDevTools(CefRefPtr<CefBrowser> browser,
                    const CefPoint& inspect_element_at);
  void CloseDevTools(CefRefPtr<CefBrowser> browser);

  // Returns the startup URL.
  std::string GetStartupURL() const;

  bool Save(const std::string& path, const std::string& data);

 private:
  void SetLoading(bool isLoading);
  void SetNavState(bool canGoBack, bool canGoForward);

  // Test context menu creation.
  void BuildTestMenu(CefRefPtr<CefMenuModel> model);
  bool ExecuteTestMenu(int command_id);
  struct TestMenuState {
    TestMenuState() : check_item(true), radio_item(0) {}
    bool check_item;
    int radio_item;
  } test_menu_state_;

  // START THREAD SAFE MEMBERS
  // The following members are thread-safe because they're initialized during
  // object construction and not changed thereafter.

  // The startup URL.
  std::string startup_url_;

  // True if mouse cursor change is disabled.
  bool mouse_cursor_change_disabled_;

  CefRefPtr<CefDialogHandler> dialog_handler_;
  CefRefPtr<CefJSDialogHandler> jsdialog_handler_;
  // END THREAD SAFE MEMBERS

  // Lock used to protect members accessed on multiple threads. Make it mutable
  // so that it can be used from const methods.
  mutable base::Lock lock_;

  // START LOCK PROTECTED MEMBERS
  // The following members are accessed on multiple threads and must be
  // protected by |lock_|.

  // The child browser window.
  CefRefPtr<CefBrowser> browser_;

  // The child browser id.
  int browser_id_;

  // True if the main browser window is currently closing.
  bool is_closing_;
  // END LOCK PROTECTED MEMBERS

  // START UI THREAD ACCESS ONLY MEMBERS
  // The following members will only be accessed on the CEF UI thread.

  // List of any popup browser windows.
  typedef std::list<CefRefPtr<CefBrowser> > BrowserList;
  BrowserList popup_browsers_;

  // The main frame window handle.
  ClientWindowHandle main_handle_;

  // The edit window handle.
  ClientWindowHandle edit_handle_;

  // The button window handles.
  ClientWindowHandle back_handle_;
  ClientWindowHandle forward_handle_;
  ClientWindowHandle stop_handle_;
  ClientWindowHandle reload_handle_;

  // The handler for off-screen rendering, if any.
  CefRefPtr<RenderHandler> osr_handler_;

  // Used for console logging purposes.
  const std::string console_log_file_;
  bool first_console_message_;

  // True if an editable field currently has focus.
  bool focus_on_editable_field_;

  // Handles the browser side of query routing. The renderer side is handled
  // in client_renderer.cc.
  CefRefPtr<CefMessageRouterBrowserSide> message_router_;

  // Set of Handlers registered with the message router.
  MessageHandlerSet message_handler_set_;

  // Number of currently existing browser windows. The application will exit
  // when the number of windows reaches 0.
  static int browser_count_;

  // END UI THREAD ACCESS ONLY MEMBERS

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_H_
