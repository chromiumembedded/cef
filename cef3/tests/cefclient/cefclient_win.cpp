// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/cefclient.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <direct.h>
#include <sstream>
#include <string>
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_runnable.h"
#include "cefclient/cefclient_osr_widget_win.h"
#include "cefclient/client_handler.h"
#include "cefclient/client_switches.h"
#include "cefclient/resource.h"
#include "cefclient/scheme_test.h"
#include "cefclient/string_util.h"

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH  255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
HINSTANCE hInst;   // current instance
TCHAR szTitle[MAX_LOADSTRING];  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];  // the main window class name
TCHAR szOSRWindowClass[MAX_LOADSTRING];  // the OSR window class name
char szWorkingDir[MAX_PATH];  // The current working directory

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// Used for processing messages on the main application thread while running
// in multi-threaded message loop mode.
HWND hMessageWnd = NULL;
HWND CreateMessageWindow(HINSTANCE hInstance);
LRESULT CALLBACK MessageWndProc(HWND, UINT, WPARAM, LPARAM);

// The global ClientHandler reference.
extern CefRefPtr<ClientHandler> g_handler;

class MainBrowserProvider : public OSRBrowserProvider {
  virtual CefRefPtr<CefBrowser> GetBrowser() {
    if (g_handler.get())
      return g_handler->GetBrowser();

    return NULL;
  }
} g_main_browser_provider;

#if defined(OS_WIN)
// Add Common Controls to the application manifest because it's required to
// support the default tooltip implementation.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")  // NOLINT(whitespace/line_length)
#endif

// Program entry point function.
int APIENTRY wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  CefMainArgs main_args(hInstance);
  CefRefPtr<ClientApp> app(new ClientApp);

  // Execute the secondary process, if any.
  int exit_code = CefExecuteProcess(main_args, app.get());
  if (exit_code >= 0)
    return exit_code;

  // Retrieve the current working directory.
  if (_getcwd(szWorkingDir, MAX_PATH) == NULL)
    szWorkingDir[0] = 0;

  // Parse command line arguments. The passed in values are ignored on Windows.
  AppInitCommandLine(0, NULL);

  CefSettings settings;

  // Populate the settings based on command line arguments.
  AppGetSettings(settings);

  // Initialize CEF.
  CefInitialize(main_args, settings, app.get());

  // Register the scheme handler.
  scheme_test::InitTest();

  HACCEL hAccelTable;

  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_CEFCLIENT, szWindowClass, MAX_LOADSTRING);
  LoadString(hInstance, IDS_OSR_WIDGET_CLASS, szOSRWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // Perform application initialization
  if (!InitInstance (hInstance, nCmdShow))
    return FALSE;

  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CEFCLIENT));

  int result = 0;

  if (!settings.multi_threaded_message_loop) {
    // Run the CEF message loop. This function will block until the application
    // recieves a WM_QUIT message.
    CefRunMessageLoop();
  } else {
    // Create a hidden window for message processing.
    hMessageWnd = CreateMessageWindow(hInstance);
    ASSERT(hMessageWnd);

    MSG msg;

    // Run the application message loop.
    while (GetMessage(&msg, NULL, 0, 0)) {
      if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    DestroyWindow(hMessageWnd);
    hMessageWnd = NULL;

    result = static_cast<int>(msg.wParam);
  }

  // Shut down CEF.
  CefShutdown();

  return result;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this
//    function so that the application will get 'well formed' small icons
//    associated with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style         = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc   = WndProc;
  wcex.cbClsExtra    = 0;
  wcex.cbWndExtra    = 0;
  wcex.hInstance     = hInstance;
  wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CEFCLIENT));
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wcex.lpszMenuName  = MAKEINTRESOURCE(IDC_CEFCLIENT);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
  HWND hWnd;

  hInst = hInstance;  // Store instance handle in our global variable

  hWnd = CreateWindow(szWindowClass, szTitle,
                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0,
                      CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

  if (!hWnd)
    return FALSE;

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  return TRUE;
}

// Change the zoom factor on the UI thread.
static void ModifyZoom(CefRefPtr<CefBrowser> browser, double delta) {
  if (CefCurrentlyOn(TID_UI)) {
    browser->GetHost()->SetZoomLevel(
        browser->GetHost()->GetZoomLevel() + delta);
  } else {
    CefPostTask(TID_UI, NewCefRunnableFunction(ModifyZoom, browser, delta));
  }
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  static HWND backWnd = NULL, forwardWnd = NULL, reloadWnd = NULL,
      stopWnd = NULL, editWnd = NULL;
  static WNDPROC editWndOldProc = NULL;

  // Static members used for the find dialog.
  static FINDREPLACE fr;
  static WCHAR szFindWhat[80] = {0};
  static WCHAR szLastFindWhat[80] = {0};
  static bool findNext = false;
  static bool lastMatchCase = false;

  int wmId, wmEvent;
  PAINTSTRUCT ps;
  HDC hdc;

  if (hWnd == editWnd) {
    // Callback for the edit window
    switch (message) {
    case WM_CHAR:
      if (wParam == VK_RETURN && g_handler.get()) {
        // When the user hits the enter key load the URL
        CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
        wchar_t strPtr[MAX_URL_LENGTH+1] = {0};
        *((LPWORD)strPtr) = MAX_URL_LENGTH;
        LRESULT strLen = SendMessage(hWnd, EM_GETLINE, 0, (LPARAM)strPtr);
        if (strLen > 0) {
          strPtr[strLen] = 0;
          browser->GetMainFrame()->LoadURL(strPtr);
        }

        return 0;
      }
    }

    return (LRESULT)CallWindowProc(editWndOldProc, hWnd, message, wParam,
                                   lParam);
  } else {
    // Callback for the main window
    switch (message) {
    case WM_CREATE: {
      // Create the single static handler class instance
      g_handler = new ClientHandler();
      g_handler->SetMainHwnd(hWnd);

      // Create the child windows used for navigation
      RECT rect;
      int x = 0;

      GetClientRect(hWnd, &rect);

      backWnd = CreateWindow(L"BUTTON", L"Back",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
                              | WS_DISABLED, x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                              hWnd, (HMENU) IDC_NAV_BACK, hInst, 0);
      x += BUTTON_WIDTH;

      forwardWnd = CreateWindow(L"BUTTON", L"Forward",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
                                | WS_DISABLED, x, 0, BUTTON_WIDTH,
                                URLBAR_HEIGHT, hWnd, (HMENU) IDC_NAV_FORWARD,
                                hInst, 0);
      x += BUTTON_WIDTH;

      reloadWnd = CreateWindow(L"BUTTON", L"Reload",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
                                | WS_DISABLED, x, 0, BUTTON_WIDTH,
                                URLBAR_HEIGHT, hWnd, (HMENU) IDC_NAV_RELOAD,
                                hInst, 0);
      x += BUTTON_WIDTH;

      stopWnd = CreateWindow(L"BUTTON", L"Stop",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
                              | WS_DISABLED, x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                              hWnd, (HMENU) IDC_NAV_STOP, hInst, 0);
      x += BUTTON_WIDTH;

      editWnd = CreateWindow(L"EDIT", 0,
                              WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                              ES_AUTOVSCROLL | ES_AUTOHSCROLL| WS_DISABLED,
                              x, 0, rect.right - BUTTON_WIDTH * 4,
                              URLBAR_HEIGHT, hWnd, 0, hInst, 0);

      // Assign the edit window's WNDPROC to this function so that we can
      // capture the enter key
      editWndOldProc =
          reinterpret_cast<WNDPROC>(GetWindowLongPtr(editWnd, GWLP_WNDPROC));
      SetWindowLongPtr(editWnd, GWLP_WNDPROC,
          reinterpret_cast<LONG_PTR>(WndProc));
      g_handler->SetEditHwnd(editWnd);
      g_handler->SetButtonHwnds(backWnd, forwardWnd, reloadWnd, stopWnd);

      rect.top += URLBAR_HEIGHT;

      CefWindowInfo info;
      CefBrowserSettings settings;

      if (AppIsOffScreenRenderingEnabled()) {
        CefRefPtr<CefCommandLine> cmd_line = AppGetCommandLine();
        bool transparent =
            cmd_line->HasSwitch(cefclient::kTransparentPaintingEnabled);

        CefRefPtr<OSRWindow> osr_window =
            OSRWindow::Create(&g_main_browser_provider, transparent);
        osr_window->CreateWidget(hWnd, rect, hInst, szOSRWindowClass);
        info.SetAsOffScreen(osr_window->hwnd());
        info.SetTransparentPainting(transparent ? TRUE : FALSE);
        g_handler->SetOSRHandler(osr_window.get());
      } else {
        // Initialize window info to the defaults for a child window.
        info.SetAsChild(hWnd, rect);
      }

      // Creat the new child browser window
      CefBrowserHost::CreateBrowser(info, g_handler.get(),
          g_handler->GetStartupURL(), settings);

      return 0;
    }

    case WM_COMMAND: {
      CefRefPtr<CefBrowser> browser;
      if (g_handler.get())
        browser = g_handler->GetBrowser();

      wmId    = LOWORD(wParam);
      wmEvent = HIWORD(wParam);
      // Parse the menu selections:
      switch (wmId) {
      case IDM_ABOUT:
        DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        return 0;
      case IDM_EXIT:
        if (g_handler.get())
          g_handler->CloseAllBrowsers(false);
        return 0;
      case ID_WARN_CONSOLEMESSAGE:
        if (g_handler.get()) {
          std::wstringstream ss;
          ss << L"Console messages will be written to "
              << std::wstring(CefString(g_handler->GetLogFile()));
          MessageBox(hWnd, ss.str().c_str(), L"Console Messages",
              MB_OK | MB_ICONINFORMATION);
        }
        return 0;
      case ID_WARN_DOWNLOADCOMPLETE:
      case ID_WARN_DOWNLOADERROR:
        if (g_handler.get()) {
          std::wstringstream ss;
          ss << L"File \"" <<
              std::wstring(CefString(g_handler->GetLastDownloadFile())) <<
              L"\" ";

          if (wmId == ID_WARN_DOWNLOADCOMPLETE)
            ss << L"downloaded successfully.";
          else
            ss << L"failed to download.";

          MessageBox(hWnd, ss.str().c_str(), L"File Download",
              MB_OK | MB_ICONINFORMATION);
        }
        return 0;
      case IDC_NAV_BACK:   // Back button
        if (browser.get())
          browser->GoBack();
        return 0;
      case IDC_NAV_FORWARD:  // Forward button
        if (browser.get())
          browser->GoForward();
        return 0;
      case IDC_NAV_RELOAD:  // Reload button
        if (browser.get())
          browser->Reload();
        return 0;
      case IDC_NAV_STOP:  // Stop button
        if (browser.get())
          browser->StopLoad();
        return 0;
      case ID_TESTS_GETSOURCE:  // Test the GetSource function
        if (browser.get())
          RunGetSourceTest(browser);
        return 0;
      case ID_TESTS_GETTEXT:  // Test the GetText function
        if (browser.get())
          RunGetTextTest(browser);
        return 0;
      case ID_TESTS_POPUP:  // Test a popup window
        if (browser.get())
          RunPopupTest(browser);
        return 0;
      case ID_TESTS_REQUEST:  // Test a request
        if (browser.get())
          RunRequestTest(browser);
        return 0;
      case ID_TESTS_PLUGIN_INFO:  // Test plugin info
        if (browser.get())
          RunPluginInfoTest(browser);
        return 0;
      case ID_TESTS_ZOOM_IN:
        if (browser.get())
          ModifyZoom(browser, 0.5);
        return 0;
      case ID_TESTS_ZOOM_OUT:
        if (browser.get())
          ModifyZoom(browser, -0.5);
        return 0;
      case ID_TESTS_ZOOM_RESET:
        if (browser.get())
          browser->GetHost()->SetZoomLevel(0.0);
        return 0;
      case ID_TESTS_TRACING_BEGIN:
        g_handler->BeginTracing();
        return 0;
      case ID_TESTS_TRACING_END:
        g_handler->EndTracing();
        return 0;
      case ID_TESTS_OTHER_TESTS:
        if (browser.get())
          RunOtherTests(browser);
        return 0;
      }
      break;
    }

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      return 0;

    case WM_SETFOCUS:
      if (g_handler.get() && g_handler->GetBrowser()) {
        // Pass focus to the browser window
        CefWindowHandle hwnd =
            g_handler->GetBrowser()->GetHost()->GetWindowHandle();
        if (hwnd)
          PostMessage(hwnd, WM_SETFOCUS, wParam, NULL);
      }
      return 0;

    case WM_SIZE:
      // Minimizing resizes the window to 0x0 which causes our layout to go all
      // screwy, so we just ignore it.
      if (wParam != SIZE_MINIMIZED && g_handler.get() &&
          g_handler->GetBrowser()) {
        CefWindowHandle hwnd =
            g_handler->GetBrowser()->GetHost()->GetWindowHandle();
        if (hwnd) {
          // Resize the browser window and address bar to match the new frame
          // window size
          RECT rect;
          GetClientRect(hWnd, &rect);
          rect.top += URLBAR_HEIGHT;

          int urloffset = rect.left + BUTTON_WIDTH * 4;

          HDWP hdwp = BeginDeferWindowPos(1);
          hdwp = DeferWindowPos(hdwp, editWnd, NULL, urloffset,
            0, rect.right - urloffset, URLBAR_HEIGHT, SWP_NOZORDER);
          hdwp = DeferWindowPos(hdwp, hwnd, NULL,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER);
          EndDeferWindowPos(hdwp);
        }
      }
      break;

    case WM_ERASEBKGND:
      if (g_handler.get() && g_handler->GetBrowser()) {
        CefWindowHandle hwnd =
            g_handler->GetBrowser()->GetHost()->GetWindowHandle();
        if (hwnd) {
          // Dont erase the background if the browser window has been loaded
          // (this avoids flashing)
          return 0;
        }
      }
      break;

    case WM_ENTERMENULOOP:
      if (!wParam) {
        // Entering the menu loop for the application menu.
        CefSetOSModalLoop(true);
      }
      break;

    case WM_EXITMENULOOP:
      if (!wParam) {
        // Exiting the menu loop for the application menu.
        CefSetOSModalLoop(false);
      }
      break;

    case WM_CLOSE:
      if (g_handler.get() && !g_handler->IsClosing()) {
        CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
        if (browser.get()) {
          // Notify the browser window that we would like to close it. This
          // will result in a call to ClientHandler::DoClose() if the
          // JavaScript 'onbeforeunload' event handler allows it.
          browser->GetHost()->CloseBrowser(false);

          // Cancel the close.
          return 0;
        }
      }

      // Allow the close.
      break;

    case WM_DESTROY:
      // Quitting CEF is handled in ClientHandler::OnBeforeClose().
      return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
  }
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

HWND CreateMessageWindow(HINSTANCE hInstance) {
  static const wchar_t kWndClass[] = L"ClientMessageWindow";

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MessageWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kWndClass;
  RegisterClassEx(&wc);

  return CreateWindow(kWndClass, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                      hInstance, 0);
}

LRESULT CALLBACK MessageWndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  switch (message) {
    case WM_COMMAND: {
      int wmId = LOWORD(wParam);
      switch (wmId) {
        case ID_QUIT:
          PostQuitMessage(0);
          return 0;
      }
    }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}


// Global functions

std::string AppGetWorkingDirectory() {
  return szWorkingDir;
}

void AppQuitMessageLoop() {
  CefRefPtr<CefCommandLine> command_line = AppGetCommandLine();
  if (command_line->HasSwitch(cefclient::kMultiThreadedMessageLoop)) {
    // Running in multi-threaded message loop mode. Need to execute
    // PostQuitMessage on the main application thread.
    ASSERT(hMessageWnd);
    PostMessage(hMessageWnd, WM_COMMAND, ID_QUIT, 0);
  } else {
    CefQuitMessageLoop();
  }
}
