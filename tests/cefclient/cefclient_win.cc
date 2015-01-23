// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <direct.h>
#include <shlobj.h> 
#include <sstream>
#include <string>

#include "include/base/cef_bind.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_sandbox_win.h"
#include "include/wrapper/cef_closure_task.h"
#include "cefclient/client_app.h"
#include "cefclient/client_handler.h"
#include "cefclient/client_switches.h"
#include "cefclient/main_context_impl.h"
#include "cefclient/main_message_loop_multithreaded_win.h"
#include "cefclient/main_message_loop_std.h"
#include "cefclient/osr_widget_win.h"
#include "cefclient/resource.h"
#include "cefclient/test_runner.h"
#include "cefclient/util_win.h"


// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically if using the required compiler version. Pass -DUSE_SANDBOX=OFF
// to the CMake command-line to disable use of the sandbox.
// Uncomment this line to manually enable sandbox support.
// #define CEF_USE_SANDBOX 1

#if defined(CEF_USE_SANDBOX)
// The cef_sandbox.lib static library is currently built with VS2013. It may not
// link successfully with other VS versions.
#pragma comment(lib, "cef_sandbox.lib")
#endif

namespace client {
namespace {

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH  255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
HINSTANCE hInst;   // current instance
TCHAR szTitle[MAX_LOADSTRING];  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];  // the main window class name
TCHAR szOSRWindowClass[MAX_LOADSTRING];  // the OSR window class name
UINT uFindMsg;  // Message identifier for find events.
HWND hFindDlg = NULL;  // Handle for the find dialog.

// Forward declarations of functions included in this code module.
ATOM RegisterMainClass(HINSTANCE hInstance);
BOOL CreateMainWindow(HINSTANCE, int);
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK FindWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutWndProc(HWND, UINT, WPARAM, LPARAM);

// The global ClientHandler reference.
CefRefPtr<ClientHandler> g_handler;

// Used by off-screen rendering to find the associated CefBrowser.
class MainBrowserProvider : public OSRBrowserProvider {
  virtual CefRefPtr<CefBrowser> GetBrowser() {
    if (g_handler.get())
      return g_handler->GetBrowser();

    return NULL;
  }
} g_main_browser_provider;


int RunMain(HINSTANCE hInstance, int nCmdShow) {
  void* sandbox_info = NULL;

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif

  CefMainArgs main_args(hInstance);
  CefRefPtr<ClientApp> app(new ClientApp);

  // Execute the secondary process, if any.
  int exit_code = CefExecuteProcess(main_args, app.get(), sandbox_info);
  if (exit_code >= 0)
    return exit_code;

  // Create the main context object.
  scoped_ptr<MainContextImpl> context(new MainContextImpl(0, NULL));

  CefSettings settings;

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  // Populate the settings based on command line arguments.
  context->PopulateSettings(&settings);

  // Create the main message loop object.
  scoped_ptr<MainMessageLoop> message_loop;
  if (settings.multi_threaded_message_loop)
    message_loop.reset(new MainMessageLoopMultithreadedWin);
  else
    message_loop.reset(new MainMessageLoopStd);

  // Initialize CEF.
  CefInitialize(main_args, settings, app.get(), sandbox_info);

  // Register scheme handlers.
  test_runner::RegisterSchemeHandlers();

  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_CEFCLIENT, szWindowClass, MAX_LOADSTRING);
  LoadString(hInstance, IDS_OSR_WIDGET_CLASS, szOSRWindowClass, MAX_LOADSTRING);
  RegisterMainClass(hInstance);

  // Perform application initialization
  if (!CreateMainWindow(hInstance, nCmdShow))
    return FALSE;

  // Register the find event message.
  uFindMsg = RegisterWindowMessage(FINDMSGSTRING);

  // Run the message loop. This will block until Quit() is called.
  int result = message_loop->Run();

  // Shut down CEF.
  CefShutdown();

  // Release objects in reverse order of creation.
  message_loop.reset();
  context.reset();

  return result;
}

// Register the main window class.
ATOM RegisterMainClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style         = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc   = MainWndProc;
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

// Create and show the main window.
BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
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

// Set focus to |browser| on the UI thread.
static void SetFocusToBrowser(CefRefPtr<CefBrowser> browser) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::Bind(&SetFocusToBrowser, browser));
    return;
  }

  if (!g_handler)
    return;

  if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(
          switches::kOffScreenRenderingEnabled)) {
    // Give focus to the OSR window.
    CefRefPtr<OSRWindow> osr_window =
        static_cast<OSRWindow*>(g_handler->GetOSRHandler().get());
    if (osr_window)
      ::SetFocus(osr_window->hwnd());
  } else {
    // Give focus to the browser.
    browser->GetHost()->SetFocus(true);
  }
}

// Window procedure for the main window.
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam,
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
  } else if (message == uFindMsg) {
    // Find event.
    LPFINDREPLACE lpfr = (LPFINDREPLACE)lParam;

    if (lpfr->Flags & FR_DIALOGTERM) {
      // The find dialog box has been dismissed so invalidate the handle and
      // reset the search results.
      hFindDlg = NULL;
      if (g_handler.get()) {
        g_handler->GetBrowser()->GetHost()->StopFinding(true);
        szLastFindWhat[0] = 0;
        findNext = false;
      }
      return 0;
    }

    if ((lpfr->Flags & FR_FINDNEXT) && g_handler.get())  {
      // Search for the requested string.
      bool matchCase = (lpfr->Flags & FR_MATCHCASE?true:false);
      if (matchCase != lastMatchCase ||
          (matchCase && wcsncmp(szFindWhat, szLastFindWhat,
              sizeof(szLastFindWhat)/sizeof(WCHAR)) != 0) ||
          (!matchCase && _wcsnicmp(szFindWhat, szLastFindWhat,
              sizeof(szLastFindWhat)/sizeof(WCHAR)) != 0)) {
        // The search string has changed, so reset the search results.
        if (szLastFindWhat[0] != 0) {
          g_handler->GetBrowser()->GetHost()->StopFinding(true);
          findNext = false;
        }
        lastMatchCase = matchCase;
        wcscpy_s(szLastFindWhat, sizeof(szLastFindWhat)/sizeof(WCHAR),
            szFindWhat);
      }

      g_handler->GetBrowser()->GetHost()->Find(0, lpfr->lpstrFindWhat,
          (lpfr->Flags & FR_DOWN)?true:false, matchCase, findNext);
      if (!findNext)
        findNext = true;
    }

    return 0;
  } else {
    // Callback for the main window
    switch (message) {
    case WM_CREATE: {
      // Create the single static handler class instance
      g_handler = new ClientHandler();
      g_handler->SetMainWindowHandle(hWnd);

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
          reinterpret_cast<LONG_PTR>(MainWndProc));
      g_handler->SetEditWindowHandle(editWnd);
      g_handler->SetButtonWindowHandles(
          backWnd, forwardWnd, reloadWnd, stopWnd);

      rect.top += URLBAR_HEIGHT;

      CefWindowInfo info;
      CefBrowserSettings settings;

      // Populate the browser settings based on command line arguments.
      MainContext::Get()->PopulateBrowserSettings(&settings);

      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      if (command_line->HasSwitch(switches::kOffScreenRenderingEnabled)) {
        const bool transparent =
            command_line->HasSwitch(switches::kTransparentPaintingEnabled);
        const bool show_update_rect =
            command_line->HasSwitch(switches::kShowUpdateRect);

        CefRefPtr<OSRWindow> osr_window =
            OSRWindow::Create(&g_main_browser_provider, transparent,
                              show_update_rect);
        osr_window->CreateWidget(hWnd, rect, hInst, szOSRWindowClass);
        info.SetAsWindowless(osr_window->hwnd(), transparent);
        g_handler->SetOSRHandler(osr_window.get());
      } else {
        // Initialize window info to the defaults for a child window.
        info.SetAsChild(hWnd, rect);
      }

      // Creat the new child browser window
      CefBrowserHost::CreateBrowser(info, g_handler.get(),
          g_handler->GetStartupURL(), settings, NULL);

      return 0;
    }

    case WM_COMMAND: {
      CefRefPtr<CefBrowser> browser;
      if (g_handler.get())
        browser = g_handler->GetBrowser();

      wmId    = LOWORD(wParam);
      wmEvent = HIWORD(wParam);

      if (wmId >= ID_TESTS_FIRST && wmId <= ID_TESTS_LAST) {
        test_runner::RunTest(browser, wmId);
        return 0;
      }

      // Parse the menu selections:
      switch (wmId) {
      case IDM_ABOUT:
        DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutWndProc);
        return 0;
      case IDM_EXIT:
        if (g_handler.get())
          g_handler->CloseAllBrowsers(false);
        return 0;
      case ID_FIND:
        if (!hFindDlg) {
          // Create the find dialog.
          ZeroMemory(&fr, sizeof(fr));
          fr.lStructSize = sizeof(fr);
          fr.hwndOwner = hWnd;
          fr.lpstrFindWhat = szFindWhat;
          fr.wFindWhatLen = sizeof(szFindWhat);
          fr.Flags = FR_HIDEWHOLEWORD | FR_DOWN;

          hFindDlg = FindText(&fr);

          // Override the dialog's window procedure.
          WNDPROC wndproc_old = SetWndProcPtr(hFindDlg, FindWndProc);

          // Associate |wndproc_old| with the dialog.
          SetUserDataPtr(hFindDlg, wndproc_old);
        } else {
          // Give focus to the existing find dialog.
          ::SetFocus(hFindDlg);
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
      }
      break;
    }

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      return 0;

    case WM_SETFOCUS:
      if (g_handler.get()) {
        CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
        if (browser)
          SetFocusToBrowser(browser);
      }
      return 0;

    case WM_SIZE: {
      if (!g_handler.get())
        break;

      // For off-screen browsers when the frame window is minimized set the
      // browser as hidden to reduce resource usage.
      const bool offscreen = CefCommandLine::GetGlobalCommandLine()->HasSwitch(
          switches::kOffScreenRenderingEnabled);
      if (offscreen) {
        CefRefPtr<OSRWindow> osr_window =
            static_cast<OSRWindow*>(g_handler->GetOSRHandler().get());
        if (osr_window)
          osr_window->WasHidden(wParam == SIZE_MINIMIZED);
      }

      if (g_handler->GetBrowser()) {
        // Retrieve the window handle (parent window with off-screen rendering).
        CefWindowHandle hwnd =
            g_handler->GetBrowser()->GetHost()->GetWindowHandle();
        if (hwnd) {
          if (wParam == SIZE_MINIMIZED) {
            // For windowed browsers when the frame window is minimized set the
            // browser window size to 0x0 to reduce resource usage.
            if (!offscreen) {
              SetWindowPos(hwnd, NULL,
                  0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
            }
          } else {
            // Resize the window and address bar to match the new frame size.
            RECT rect;
            GetClientRect(hWnd, &rect);
            rect.top += URLBAR_HEIGHT;

            int urloffset = rect.left + BUTTON_WIDTH * 4;

            HDWP hdwp = BeginDeferWindowPos(1);
            hdwp = DeferWindowPos(hdwp, editWnd, NULL, urloffset,
                0, rect.right - urloffset, URLBAR_HEIGHT, SWP_NOZORDER);
            hdwp = DeferWindowPos(hdwp, hwnd, NULL,
                rect.left, rect.top, rect.right - rect.left,
                rect.bottom - rect.top, SWP_NOZORDER);
            EndDeferWindowPos(hdwp);
          }
        }
      }
    } break;

    case WM_MOVING:
    case WM_MOVE:
      // Notify the browser of move events so that popup windows are displayed
      // in the correct location and dismissed when the window moves.
      if (g_handler.get() && g_handler->GetBrowser())
        g_handler->GetBrowser()->GetHost()->NotifyMoveOrResizeStarted();
      return 0;

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

// Window procedure for the find dialog.
LRESULT CALLBACK FindWndProc(HWND hWnd, UINT message, WPARAM wParam,
                             LPARAM lParam) {
  REQUIRE_MAIN_THREAD();

  WNDPROC old_wndproc = GetUserDataPtr<WNDPROC>(hWnd);
  DCHECK(old_wndproc);

  switch (message) {
    case WM_ACTIVATE:
      // Set this dialog as current when activated.
      MainMessageLoop::Get()->SetCurrentModelessDialog(
          wParam == 0 ? NULL : hWnd);
      return FALSE;
    case WM_NCDESTROY:
      // Clear the reference to |old_wndproc|.
      SetUserDataPtr(hWnd, NULL);
      break;
  }

  return CallWindowProc(old_wndproc, hWnd, message, wParam, lParam);
}

// Window procedure for the about dialog.
INT_PTR CALLBACK AboutWndProc(HWND hDlg, UINT message, WPARAM wParam,
                              LPARAM lParam) {
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

}  // namespace
}  // namespace client


// Program entry point function.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR    lpCmdLine,
                      int       nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  return client::RunMain(hInstance, nCmdShow);
}
