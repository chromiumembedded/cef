// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler.h"
#include <stdio.h>
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_path_util.h"
#include "include/cef_process_util.h"
#include "include/cef_runnable.h"
#include "include/cef_trace.h"
#include "include/cef_url.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "cefclient/binding_test.h"
#include "cefclient/cefclient.h"
#include "cefclient/client_renderer.h"
#include "cefclient/client_switches.h"
#include "cefclient/dialog_test.h"
#include "cefclient/dom_test.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"
#include "cefclient/window_test.h"

namespace {

// Custom menu command Ids.
enum client_menu_ids {
  CLIENT_ID_SHOW_DEVTOOLS   = MENU_ID_USER_FIRST,
  CLIENT_ID_CLOSE_DEVTOOLS,
  CLIENT_ID_TESTMENU_SUBMENU,
  CLIENT_ID_TESTMENU_CHECKITEM,
  CLIENT_ID_TESTMENU_RADIOITEM1,
  CLIENT_ID_TESTMENU_RADIOITEM2,
  CLIENT_ID_TESTMENU_RADIOITEM3,
};

const char kTestOrigin[] = "http://tests/";

// Retrieve the file name and mime type based on the specified url.
bool ParseTestUrl(const std::string& url,
                  std::string* file_name,
                  std::string* mime_type) {
  // Retrieve the path component.
  CefURLParts parts;
  CefParseURL(url, parts);
  std::string file = CefString(&parts.path);
  if (file.size() < 2)
    return false;

  // Remove the leading slash.
  file = file.substr(1);

  // Verify that the file name is valid.
  for(size_t i = 0; i < file.size(); ++i) {
    const char c = file[i];
    if (!isalpha(c) && !isdigit(c) && c != '_' && c != '.')
      return false;
  }

  // Determine the mime type based on the file extension, if any.
  size_t pos = file.rfind(".");
  if (pos != std::string::npos) {
    std::string ext = file.substr(pos + 1);
    if (ext == "html")
      *mime_type = "text/html";
    else if (ext == "png")
      *mime_type = "image/png";
    else
      return false;
  } else {
    // Default to an html extension if none is specified.
    *mime_type = "text/html";
    file += ".html";
  }

  *file_name = file;
  return true;
}

}  // namespace

int ClientHandler::m_BrowserCount = 0;

ClientHandler::ClientHandler()
  : m_MainHwnd(NULL),
    m_BrowserId(0),
    m_bIsClosing(false),
    m_EditHwnd(NULL),
    m_BackHwnd(NULL),
    m_ForwardHwnd(NULL),
    m_StopHwnd(NULL),
    m_ReloadHwnd(NULL),
    m_bFocusOnEditableField(false) {
  // Read command line settings.
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  if (command_line->HasSwitch(cefclient::kUrl))
    m_StartupURL = command_line->GetSwitchValue(cefclient::kUrl);
  if (m_StartupURL.empty())
    m_StartupURL = "http://www.google.com/";

  m_bMouseCursorChangeDisabled =
      command_line->HasSwitch(cefclient::kMouseCursorChangeDisabled);
}

ClientHandler::~ClientHandler() {
}

bool ClientHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  if (message_router_->OnProcessMessageReceived(browser, source_process,
                                                message)) {
    return true;
  }

  // Check for messages from the client renderer.
  std::string message_name = message->GetName();
  if (message_name == client_renderer::kFocusedNodeChangedMessage) {
    // A message is sent from ClientRenderDelegate to tell us whether the
    // currently focused DOM node is editable. Use of |m_bFocusOnEditableField|
    // is redundant with CefKeyEvent.focus_on_editable_field in OnPreKeyEvent
    // but is useful for demonstration purposes.
    m_bFocusOnEditableField = message->GetArgumentList()->GetBool(0);
    return true;
  }

  return false;
}

void ClientHandler::OnBeforeContextMenu(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefContextMenuParams> params,
    CefRefPtr<CefMenuModel> model) {
  if ((params->GetTypeFlags() & (CM_TYPEFLAG_PAGE | CM_TYPEFLAG_FRAME)) != 0) {
    // Add a separator if the menu already has items.
    if (model->GetCount() > 0)
      model->AddSeparator();

    // Add DevTools items to all context menus.
    model->AddItem(CLIENT_ID_SHOW_DEVTOOLS, "&Show DevTools");
    model->AddItem(CLIENT_ID_CLOSE_DEVTOOLS, "Close DevTools");

    // Test context menu features.
    BuildTestMenu(model);
  }
}

bool ClientHandler::OnContextMenuCommand(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefContextMenuParams> params,
    int command_id,
    EventFlags event_flags) {
  switch (command_id) {
    case CLIENT_ID_SHOW_DEVTOOLS:
      ShowDevTools(browser);
      return true;
    case CLIENT_ID_CLOSE_DEVTOOLS:
      CloseDevTools(browser);
      return true;
    default:  // Allow default handling, if any.
      return ExecuteTestMenu(command_id);
  }
}

bool ClientHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                     const CefString& message,
                                     const CefString& source,
                                     int line) {
  REQUIRE_UI_THREAD();

  bool first_message;
  std::string logFile;

  {
    AutoLock lock_scope(this);

    first_message = m_LogFile.empty();
    if (first_message) {
      std::stringstream ss;
      ss << AppGetWorkingDirectory();
#if defined(OS_WIN)
      ss << "\\";
#else
      ss << "/";
#endif
      ss << "console.log";
      m_LogFile = ss.str();
    }
    logFile = m_LogFile;
  }

  FILE* file = fopen(logFile.c_str(), "a");
  if (file) {
    std::stringstream ss;
    ss << "Message: " << std::string(message) << "\r\nSource: " <<
        std::string(source) << "\r\nLine: " << line <<
        "\r\n-----------------------\r\n";
    fputs(ss.str().c_str(), file);
    fclose(file);

    if (first_message)
      SendNotification(NOTIFY_CONSOLE_MESSAGE);
  }

  return false;
}

void ClientHandler::OnBeforeDownload(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  REQUIRE_UI_THREAD();
  // Continue the download and show the "Save As" dialog.
  callback->Continue(GetDownloadPath(suggested_name), true);
}

void ClientHandler::OnDownloadUpdated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  REQUIRE_UI_THREAD();
  if (download_item->IsComplete()) {
    SetLastDownloadFile(download_item->GetFullPath());
    SendNotification(NOTIFY_DOWNLOAD_COMPLETE);
  }
}

bool ClientHandler::OnDragEnter(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefDragData> dragData,
                                DragOperationsMask mask) {
  REQUIRE_UI_THREAD();

  // Forbid dragging of link URLs.
  if (mask & DRAG_OPERATION_LINK)
    return true;

  return false;
}

void ClientHandler::OnRequestGeolocationPermission(
      CefRefPtr<CefBrowser> browser,
      const CefString& requesting_url,
      int request_id,
      CefRefPtr<CefGeolocationCallback> callback) {
  // Allow geolocation access from all websites.
  callback->Continue(true);
}

bool ClientHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                  const CefKeyEvent& event,
                                  CefEventHandle os_event,
                                  bool* is_keyboard_shortcut) {
  if (!event.focus_on_editable_field && event.windows_key_code == 0x20) {
    // Special handling for the space character when an input element does not
    // have focus. Handling the event in OnPreKeyEvent() keeps the event from
    // being processed in the renderer. If we instead handled the event in the
    // OnKeyEvent() method the space key would cause the window to scroll in
    // addition to showing the alert box.
    if (event.type == KEYEVENT_RAWKEYDOWN) {
      browser->GetMainFrame()->ExecuteJavaScript(
          "alert('You pressed the space bar!');", "", 0);
    }
    return true;
  }

  return false;
}

bool ClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  const CefString& target_url,
                                  const CefString& target_frame_name,
                                  const CefPopupFeatures& popupFeatures,
                                  CefWindowInfo& windowInfo,
                                  CefRefPtr<CefClient>& client,
                                  CefBrowserSettings& settings,
                                  bool* no_javascript_access) {
  if (browser->GetHost()->IsWindowRenderingDisabled()) {
    // Cancel popups in off-screen rendering mode.
    return true;
  }
  return false;
}

void ClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  if (!message_router_) {
    // Create the browser-side router for query handling.
    CefMessageRouterConfig config;
    message_router_ = CefMessageRouterBrowserSide::Create(config);

    // Register handlers with the router.
    CreateMessageHandlers(message_handler_set_);
    MessageHandlerSet::const_iterator it = message_handler_set_.begin();
    for (; it != message_handler_set_.end(); ++it)
      message_router_->AddHandler(*(it), false);
  }

  // Disable mouse cursor change if requested via the command-line flag.
  if (m_bMouseCursorChangeDisabled)
    browser->GetHost()->SetMouseCursorChangeDisabled(true);

  AutoLock lock_scope(this);
  if (!m_Browser.get())   {
    // We need to keep the main child window, but not popup windows
    m_Browser = browser;
    m_BrowserId = browser->GetIdentifier();
  } else if (browser->IsPopup()) {
    // Add to the list of popup browsers.
    m_PopupBrowsers.push_back(browser);

    // Give focus to the popup browser.
    browser->GetHost()->SetFocus(true);
  }

  m_BrowserCount++;
}

bool ClientHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  // Closing the main window requires special handling. See the DoClose()
  // documentation in the CEF header for a detailed destription of this
  // process.
  if (m_BrowserId == browser->GetIdentifier()) {
    // Notify the browser that the parent window is about to close.
    browser->GetHost()->ParentWindowWillClose();

    // Set a flag to indicate that the window close should be allowed.
    m_bIsClosing = true;
  }

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void ClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  message_router_->OnBeforeClose(browser);

  if (m_BrowserId == browser->GetIdentifier()) {
    // Free the browser pointer so that the browser can be destroyed
    m_Browser = NULL;

    if (m_OSRHandler.get()) {
      m_OSRHandler->OnBeforeClose(browser);
      m_OSRHandler = NULL;
    }
  } else if (browser->IsPopup()) {
    // Remove from the browser popup list.
    BrowserList::iterator bit = m_PopupBrowsers.begin();
    for (; bit != m_PopupBrowsers.end(); ++bit) {
      if ((*bit)->IsSame(browser)) {
        m_PopupBrowsers.erase(bit);
        break;
      }
    }
  }

  if (--m_BrowserCount == 0) {
    // All browser windows have closed.
    // Remove and delete message router handlers.
    MessageHandlerSet::const_iterator it = message_handler_set_.begin();
    for (; it != message_handler_set_.end(); ++it) {
      message_router_->RemoveHandler(*(it));
      delete *(it);
    }
    message_handler_set_.clear();
    message_router_ = NULL;

    // Quit the application message loop.
    AppQuitMessageLoop();
  }
}

void ClientHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  REQUIRE_UI_THREAD();

  SetLoading(isLoading);
  SetNavState(canGoBack, canGoForward);

  if (!isLoading) {
    // Continue the DOM test.
    if (browser->GetMainFrame()->GetURL() == dom_test::kTestUrl)
      dom_test::OnLoadEnd(browser);
  }
}

void ClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  REQUIRE_UI_THREAD();

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED)
    return;

  // Don't display an error for external protocols that we allow the OS to
  // handle. See OnProtocolExecution().
  if (errorCode == ERR_UNKNOWN_URL_SCHEME) {
    std::string urlStr = frame->GetURL();
    if (urlStr.find("spotify:") == 0)
      return;
  }

  // Display a load error message.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL " << std::string(failedUrl) <<
        " with error " << std::string(errorText) << " (" << errorCode <<
        ").</h2></body></html>";
  frame->LoadString(ss.str(), failedUrl);
}

bool ClientHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   bool is_redirect) {
  message_router_->OnBeforeBrowse(browser, frame);
  return false;
}

CefRefPtr<CefResourceHandler> ClientHandler::GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) {
  std::string url = request->GetURL();
  if (url.find(kTestOrigin) == 0) {
    // Handle URLs in the test origin.
    std::string file_name, mime_type;
    if (ParseTestUrl(url, &file_name, &mime_type)) {
      if (file_name == "request.html") {
        // Show the request contents.
        std::string dump;
        DumpRequestContents(request, dump);
        std::string str = "<html><body bgcolor=\"white\"><pre>" + dump +
                          "</pre></body></html>";
        CefRefPtr<CefStreamReader> stream =
            CefStreamReader::CreateForData(
                static_cast<void*>(const_cast<char*>(str.c_str())),
                str.size());
        ASSERT(stream.get());
        return new CefStreamResourceHandler("text/html", stream);
      } else {
        // Load the resource from file.
        CefRefPtr<CefStreamReader> stream =
            GetBinaryResourceReader(file_name.c_str());
        if (stream.get())
          return new CefStreamResourceHandler(mime_type, stream);
      }
    }
  }

  return NULL;
}

bool ClientHandler::OnQuotaRequest(CefRefPtr<CefBrowser> browser,
                                   const CefString& origin_url,
                                   int64 new_size,
                                   CefRefPtr<CefQuotaCallback> callback) {
  static const int64 max_size = 1024 * 1024 * 20;  // 20mb.

  // Grant the quota request if the size is reasonable.
  callback->Continue(new_size <= max_size);
  return true;
}

void ClientHandler::OnProtocolExecution(CefRefPtr<CefBrowser> browser,
                                        const CefString& url,
                                        bool& allow_os_execution) {
  std::string urlStr = url;

  // Allow OS execution of Spotify URIs.
  if (urlStr.find("spotify:") == 0)
    allow_os_execution = true;
}

void ClientHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                              TerminationStatus status) {
  message_router_->OnRenderProcessTerminated(browser);

  // Load the startup URL if that's not the website that we terminated on.
  CefRefPtr<CefFrame> frame = browser->GetMainFrame();
  std::string url = frame->GetURL();
  std::transform(url.begin(), url.end(), url.begin(), tolower);

  std::string startupURL = GetStartupURL();
  if (startupURL != "chrome://crash" && !url.empty() &&
      url.find(startupURL) != 0) {
    frame->LoadURL(startupURL);
  }
}

bool ClientHandler::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
    CefRect& rect) {
  if (!m_OSRHandler.get())
    return false;
  return m_OSRHandler->GetRootScreenRect(browser, rect);
}

bool ClientHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
  if (!m_OSRHandler.get())
    return false;
  return m_OSRHandler->GetViewRect(browser, rect);
}

bool ClientHandler::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                   int viewX,
                                   int viewY,
                                   int& screenX,
                                   int& screenY) {
  if (!m_OSRHandler.get())
    return false;
  return m_OSRHandler->GetScreenPoint(browser, viewX, viewY, screenX, screenY);
}

bool ClientHandler::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                  CefScreenInfo& screen_info) {
  if (!m_OSRHandler.get())
    return false;
  return m_OSRHandler->GetScreenInfo(browser, screen_info);
}

void ClientHandler::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                bool show) {
  if (!m_OSRHandler.get())
    return;
  return m_OSRHandler->OnPopupShow(browser, show);
}

void ClientHandler::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                const CefRect& rect) {
  if (!m_OSRHandler.get())
    return;
  return m_OSRHandler->OnPopupSize(browser, rect);
}

void ClientHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const void* buffer,
                            int width,
                            int height) {
  if (!m_OSRHandler.get())
    return;
  m_OSRHandler->OnPaint(browser, type, dirtyRects, buffer, width, height);
}

void ClientHandler::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                   CefCursorHandle cursor) {
  if (!m_OSRHandler.get())
    return;
  m_OSRHandler->OnCursorChange(browser, cursor);
}

void ClientHandler::SetMainHwnd(CefWindowHandle hwnd) {
  AutoLock lock_scope(this);
  m_MainHwnd = hwnd;
}

void ClientHandler::SetEditHwnd(CefWindowHandle hwnd) {
  AutoLock lock_scope(this);
  m_EditHwnd = hwnd;
}

void ClientHandler::SetButtonHwnds(CefWindowHandle backHwnd,
                                   CefWindowHandle forwardHwnd,
                                   CefWindowHandle reloadHwnd,
                                   CefWindowHandle stopHwnd) {
  AutoLock lock_scope(this);
  m_BackHwnd = backHwnd;
  m_ForwardHwnd = forwardHwnd;
  m_ReloadHwnd = reloadHwnd;
  m_StopHwnd = stopHwnd;
}

void ClientHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI,
        NewCefRunnableMethod(this, &ClientHandler::CloseAllBrowsers,
                             force_close));
    return;
  }

  if (!m_PopupBrowsers.empty()) {
    // Request that any popup browsers close.
    BrowserList::const_iterator it = m_PopupBrowsers.begin();
    for (; it != m_PopupBrowsers.end(); ++it)
      (*it)->GetHost()->CloseBrowser(force_close);
  }

  if (m_Browser.get()) {
    // Request that the main browser close.
    m_Browser->GetHost()->CloseBrowser(force_close);
  }
}

std::string ClientHandler::GetLogFile() {
  AutoLock lock_scope(this);
  return m_LogFile;
}

void ClientHandler::SetLastDownloadFile(const std::string& fileName) {
  AutoLock lock_scope(this);
  m_LastDownloadFile = fileName;
}

std::string ClientHandler::GetLastDownloadFile() {
  AutoLock lock_scope(this);
  return m_LastDownloadFile;
}

void ClientHandler::ShowDevTools(CefRefPtr<CefBrowser> browser) {
  CefWindowInfo windowInfo;
  CefBrowserSettings settings;

#if defined(OS_WIN)
  windowInfo.SetAsPopup(browser->GetHost()->GetWindowHandle(), "DevTools");
#endif

  browser->GetHost()->ShowDevTools(windowInfo, this, settings);
}

void ClientHandler::CloseDevTools(CefRefPtr<CefBrowser> browser) {
  browser->GetHost()->CloseDevTools();
}

void ClientHandler::BeginTracing() {
  if (CefCurrentlyOn(TID_UI)) {
    CefBeginTracing(CefString());
  } else {
    CefPostTask(TID_UI,
        NewCefRunnableMethod(this, &ClientHandler::BeginTracing));
  }
}

void ClientHandler::EndTracing() {
  if (CefCurrentlyOn(TID_UI)) {
    class Client : public CefEndTracingCallback,
                   public CefRunFileDialogCallback {
     public:
      explicit Client(CefRefPtr<ClientHandler> handler)
          : handler_(handler) {
        RunDialog();
      }

      void RunDialog() {
        static const char kDefaultFileName[] = "trace.txt";
        std::string path = handler_->GetDownloadPath(kDefaultFileName);
        if (path.empty())
          path = kDefaultFileName;

        // Results in a call to OnFileDialogDismissed.
        handler_->GetBrowser()->GetHost()->RunFileDialog(
            FILE_DIALOG_SAVE, CefString(), path, std::vector<CefString>(),
            this);
      }

      virtual void OnFileDialogDismissed(
          CefRefPtr<CefBrowserHost> browser_host,
          const std::vector<CefString>& file_paths) OVERRIDE {
        if (!file_paths.empty()) {
          // File selected. Results in a call to OnEndTracingComplete.
          CefEndTracingAsync(file_paths.front(), this);
        } else {
          // No file selected. Discard the trace data.
          CefEndTracingAsync(CefString(), NULL);
        }
      }

      virtual void OnEndTracingComplete(const CefString& tracing_file) OVERRIDE {
        handler_->SetLastDownloadFile(tracing_file.ToString());
        handler_->SendNotification(NOTIFY_DOWNLOAD_COMPLETE);
      }

     private:
      CefRefPtr<ClientHandler> handler_;

      IMPLEMENT_REFCOUNTING(Callback);
    };

    new Client(this);
  } else {
    CefPostTask(TID_UI,
        NewCefRunnableMethod(this, &ClientHandler::EndTracing));
  }
}

bool ClientHandler::Save(const std::string& path, const std::string& data) {
  FILE* f = fopen(path.c_str(), "w");
  if (!f)
    return false;
  size_t total = 0;
  do {
    size_t write = fwrite(data.c_str() + total, 1, data.size() - total, f);
    if (write == 0)
      break;
    total += write;
  } while (total < data.size());
  fclose(f);
  return true;
}

// static
void ClientHandler::CreateMessageHandlers(MessageHandlerSet& handlers) {
  // Create the dialog test handlers.
  dialog_test::CreateMessageHandlers(handlers);

  // Create the binding test handlers.
  binding_test::CreateMessageHandlers(handlers);

  // Create the window test handlers.
  window_test::CreateMessageHandlers(handlers);
}

void ClientHandler::BuildTestMenu(CefRefPtr<CefMenuModel> model) {
  if (model->GetCount() > 0)
    model->AddSeparator();

  // Build the sub menu.
  CefRefPtr<CefMenuModel> submenu =
      model->AddSubMenu(CLIENT_ID_TESTMENU_SUBMENU, "Context Menu Test");
  submenu->AddCheckItem(CLIENT_ID_TESTMENU_CHECKITEM, "Check Item");
  submenu->AddRadioItem(CLIENT_ID_TESTMENU_RADIOITEM1, "Radio Item 1", 0);
  submenu->AddRadioItem(CLIENT_ID_TESTMENU_RADIOITEM2, "Radio Item 2", 0);
  submenu->AddRadioItem(CLIENT_ID_TESTMENU_RADIOITEM3, "Radio Item 3", 0);

  // Check the check item.
  if (m_TestMenuState.check_item)
    submenu->SetChecked(CLIENT_ID_TESTMENU_CHECKITEM, true);

  // Check the selected radio item.
  submenu->SetChecked(
      CLIENT_ID_TESTMENU_RADIOITEM1 + m_TestMenuState.radio_item, true);
}

bool ClientHandler::ExecuteTestMenu(int command_id) {
  if (command_id == CLIENT_ID_TESTMENU_CHECKITEM) {
    // Toggle the check item.
    m_TestMenuState.check_item ^= 1;
    return true;
  } else if (command_id >= CLIENT_ID_TESTMENU_RADIOITEM1 &&
             command_id <= CLIENT_ID_TESTMENU_RADIOITEM3) {
    // Store the selected radio item.
    m_TestMenuState.radio_item = (command_id - CLIENT_ID_TESTMENU_RADIOITEM1);
    return true;
  }

  // Allow default handling to proceed.
  return false;
}

