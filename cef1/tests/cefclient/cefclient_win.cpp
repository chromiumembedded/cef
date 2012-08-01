// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
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
#include "cefclient/binding_test.h"
#include "cefclient/client_handler.h"
#include "cefclient/extension_test.h"
#include "cefclient/osrplugin_test.h"
#include "cefclient/plugin_test.h"
#include "cefclient/resource.h"
#include "cefclient/scheme_test.h"
#include "cefclient/string_util.h"
#include "cefclient/uiplugin_test.h"

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH  255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
HINSTANCE hInst;   // current instance
TCHAR szTitle[MAX_LOADSTRING];  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];  // the main window class name
char szWorkingDir[MAX_PATH];  // The current working directory
UINT uFindMsg;  // Message identifier for find events.
HWND hFindDlg = NULL;  // Handle for the find dialog.

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// The global ClientHandler reference.
extern CefRefPtr<ClientHandler> g_handler;

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

  // Retrieve the current working directory.
  if (_getcwd(szWorkingDir, MAX_PATH) == NULL)
    szWorkingDir[0] = 0;

  // Parse command line arguments. The passed in values are ignored on Windows.
  AppInitCommandLine(0, NULL);

  CefSettings settings;
  CefRefPtr<CefApp> app;

  // Populate the settings based on command line arguments.
  AppGetSettings(settings, app);

  // Initialize CEF.
  CefInitialize(settings, app);

  // Register the internal client plugin.
  InitPluginTest();

  // Register the internal UI client plugin.
  InitUIPluginTest();

  // Register the internal OSR client plugin.
  InitOSRPluginTest();

  // Register the V8 extension handler.
  InitExtensionTest();

  // Register the scheme handler.
  InitSchemeTest();

  HACCEL hAccelTable;

  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_CEFCLIENT, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // Perform application initialization
  if (!InitInstance (hInstance, nCmdShow))
    return FALSE;

  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CEFCLIENT));

  // Register the find event message.
  uFindMsg = RegisterWindowMessage(FINDMSGSTRING);

  int result = 0;

  if (!settings.multi_threaded_message_loop) {
    // Run the CEF message loop. This function will block until the application
    // recieves a WM_QUIT message.
    CefRunMessageLoop();
  } else {
    MSG msg;

    // Run the application message loop.
    while (GetMessage(&msg, NULL, 0, 0)) {
      // Allow processing of find dialog messages.
      if (hFindDlg && IsDialogMessage(hFindDlg, &msg))
        continue;

      if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

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
  } else if (message == uFindMsg) {
    // Find event.
    LPFINDREPLACE lpfr = (LPFINDREPLACE)lParam;

    if (lpfr->Flags & FR_DIALOGTERM) {
      // The find dialog box has been dismissed so invalidate the handle and
      // reset the search results.
      hFindDlg = NULL;
      if (g_handler.get()) {
        g_handler->GetBrowser()->StopFinding(true);
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
          g_handler->GetBrowser()->StopFinding(true);
          findNext = false;
        }
        lastMatchCase = matchCase;
        wcscpy_s(szLastFindWhat, sizeof(szLastFindWhat)/sizeof(WCHAR),
            szFindWhat);
      }

      g_handler->GetBrowser()->Find(0, lpfr->lpstrFindWhat,
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

      // Populate the settings based on command line arguments.
      AppGetBrowserSettings(settings);

      // Initialize window info to the defaults for a child window
      info.SetAsChild(hWnd, rect);

      // Creat the new child browser window
      CefBrowser::CreateBrowser(info,
          static_cast<CefRefPtr<CefClient> >(g_handler),
          "http://www.google.com", settings);

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
        DestroyWindow(hWnd);
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
        } else {
          // Give focus to the existing find dialog.
          ::SetFocus(hFindDlg);
        }
        return 0;
      case ID_PRINT:
        if (browser.get())
          browser->GetMainFrame()->Print();
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
      case ID_TESTS_JAVASCRIPT_BINDING:  // Test the V8 binding handler
        if (browser.get())
          RunBindingTest(browser);
        return 0;
      case ID_TESTS_JAVASCRIPT_EXTENSION:  // Test the V8 extension handler
        if (browser.get())
          RunExtensionTest(browser);
        return 0;
      case ID_TESTS_JAVASCRIPT_PERFORMANCE:  // Test the V8 performance
        if (browser.get())
          RunExtensionPerfTest(browser);
        return 0;
      case ID_TESTS_JAVASCRIPT_EXECUTE:  // Test execution of javascript
        if (browser.get())
          RunJavaScriptExecuteTest(browser);
        return 0;
      case ID_TESTS_JAVASCRIPT_INVOKE:
        if (browser.get())
          RunJavaScriptInvokeTest(browser);
        return 0;
      case ID_TESTS_PLUGIN:  // Test the custom plugin
        if (browser.get())
          RunPluginTest(browser);
        return 0;
      case ID_TESTS_PLUGIN_INFO:  // Test plugin info
        if (browser.get())
          RunPluginInfoTest(browser);
        return 0;
      case ID_TESTS_POPUP:  // Test a popup window
        if (browser.get())
          RunPopupTest(browser);
        return 0;
      case ID_TESTS_TRANSPARENT_POPUP:  // Test a transparent popup window
        if (browser.get())
          RunTransparentPopupTest(browser);
        return 0;
      case ID_TESTS_REQUEST:  // Test a request
        if (browser.get())
          RunRequestTest(browser);
        return 0;
      case ID_TESTS_SCHEME_HANDLER:  // Test the scheme handler
        if (browser.get())
          RunSchemeTest(browser);
        return 0;
      case ID_TESTS_UIAPP:  // Test the UI app
        if (browser.get())
          RunUIPluginTest(browser);
        return 0;
      case ID_TESTS_OSRAPP:  // Test the OSR app
        if (browser.get())
          RunOSRPluginTest(browser, false);
        return 0;
      case ID_TESTS_TRANSPARENT_OSRAPP:  // Test the OSR app with transparency
        if (browser.get())
          RunOSRPluginTest(browser, true);
        return 0;
      case ID_TESTS_DOMACCESS:  // Test DOM access
        if (browser.get())
          RunDOMAccessTest(browser);
        return 0;
      case ID_TESTS_LOCALSTORAGE:  // Test localStorage
        if (browser.get())
          RunLocalStorageTest(browser);
        return 0;
      case ID_TESTS_ACCELERATED2DCANVAS:  // Test accelerated 2d canvas
        if (browser.get())
          RunAccelerated2DCanvasTest(browser);
        return 0;
      case ID_TESTS_ACCELERATEDLAYERS:  // Test accelerated layers
        if (browser.get())
          RunAcceleratedLayersTest(browser);
        return 0;
      case ID_TESTS_WEBGL:  // Test WebGL
        if (browser.get())
          RunWebGLTest(browser);
        return 0;
      case ID_TESTS_HTML5VIDEO:  // Test HTML5 video
        if (browser.get())
          RunHTML5VideoTest(browser);
        return 0;
      case ID_TESTS_DRAGDROP:  // Test drag & drop
        if (browser.get())
          RunDragDropTest(browser);
        return 0;
      case ID_TESTS_XMLHTTPREQUEST:  // Test XMLHttpRequest
        if (browser.get())
          RunXMLHTTPRequestTest(browser);
        return 0;
      case ID_TESTS_WEBURLREQUEST:
        if (browser.get())
          RunWebURLRequestTest(browser);
        return 0;
      case ID_TESTS_ZOOM_IN:
        if (browser.get())
          browser->SetZoomLevel(browser->GetZoomLevel() + 0.5);
        return 0;
      case ID_TESTS_ZOOM_OUT:
        if (browser.get())
          browser->SetZoomLevel(browser->GetZoomLevel() - 0.5);
        return 0;
      case ID_TESTS_ZOOM_RESET:
        if (browser.get())
          browser->SetZoomLevel(0.0);
        return 0;
      case ID_TESTS_DEVTOOLS_SHOW:
        if (browser.get())
          browser->ShowDevTools();
        return 0;
      case ID_TESTS_DEVTOOLS_CLOSE:
        if (browser.get())
          browser->CloseDevTools();
        return 0;
      case ID_TESTS_MODALDIALOG:
        if (browser.get())
          RunModalDialogTest(browser);
        return 0;
      case ID_TESTS_GETIMAGE:
        if (browser.get())
          RunGetImageTest(browser);
        return 0;
      }
      break;
    }

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      return 0;

    case WM_SETFOCUS:
      if (g_handler.get() && g_handler->GetBrowserHwnd()) {
        // Pass focus to the browser window
        PostMessage(g_handler->GetBrowserHwnd(), WM_SETFOCUS, wParam, NULL);
      }
      return 0;

    case WM_SIZE:
      if (g_handler.get() && g_handler->GetBrowserHwnd()) {
        // Resize the browser window and address bar to match the new frame
        // window size
        RECT rect;
        GetClientRect(hWnd, &rect);
        rect.top += URLBAR_HEIGHT;

        int urloffset = rect.left + BUTTON_WIDTH * 4;

        HDWP hdwp = BeginDeferWindowPos(1);
        hdwp = DeferWindowPos(hdwp, editWnd, NULL, urloffset,
          0, rect.right - urloffset, URLBAR_HEIGHT, SWP_NOZORDER);
        hdwp = DeferWindowPos(hdwp, g_handler->GetBrowserHwnd(), NULL,
          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
          SWP_NOZORDER);
        EndDeferWindowPos(hdwp);
      }
      break;

    case WM_ERASEBKGND:
      if (g_handler.get() && g_handler->GetBrowserHwnd()) {
        // Dont erase the background if the browser window has been loaded
        // (this avoids flashing)
        return 0;
      }
      break;

    case WM_CLOSE:
      if (g_handler.get()) {
        CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
        if (browser.get()) {
          // Let the browser window know we are about to destroy it.
          browser->ParentWindowWillClose();
        }
      }
      break;

    case WM_DESTROY:
      // The frame window has exited
      PostQuitMessage(0);
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


// Global functions

std::string AppGetWorkingDirectory() {
  return szWorkingDir;
}

void RunTransparentPopupTest(CefRefPtr<CefBrowser> browser) {
  CefWindowInfo info;
  CefBrowserSettings settings;

  // Initialize window info to the defaults for a popup window
  info.SetAsPopup(NULL, "TransparentPopup");
  info.SetTransparentPainting(TRUE);
  info.m_nWidth = 500;
  info.m_nHeight = 500;

  // Creat the popup browser window
  CefBrowser::CreateBrowser(info,
      static_cast<CefRefPtr<CefClient> >(g_handler),
      "http://tests/transparency", settings);
}

namespace {

// Determine a temporary path for the bitmap file.
bool GetBitmapTempPath(LPWSTR szTempName) {
  DWORD dwRetVal;
  DWORD dwBufSize = 512;
  TCHAR lpPathBuffer[512];
  UINT uRetVal;

  dwRetVal = GetTempPath(dwBufSize,      // length of the buffer
                         lpPathBuffer);  // buffer for path
  if (dwRetVal > dwBufSize || (dwRetVal == 0))
    return false;

  // Create a temporary file.
  uRetVal = GetTempFileName(lpPathBuffer,  // directory for tmp files
                            L"image",      // temp file name prefix
                            0,             // create unique name
                            szTempName);   // buffer for name
  if (uRetVal == 0)
    return false;

  size_t len = wcslen(szTempName);
  wcscpy(szTempName + len - 3, L"bmp");
  return true;
}

void UIT_RunGetImageTest(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  int width, height;
  bool success = false;

  // Retrieve the image size.
  if (browser->GetSize(PET_VIEW, width, height)) {
    void* bits;

    // Populate the bitmap info header.
    BITMAPINFOHEADER info;
    info.biSize = sizeof(BITMAPINFOHEADER);
    info.biWidth = width;
    info.biHeight = -height;  // minus means top-down bitmap
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;  // no compression
    info.biSizeImage = 0;
    info.biXPelsPerMeter = 1;
    info.biYPelsPerMeter = 1;
    info.biClrUsed = 0;
    info.biClrImportant = 0;

    // Create the bitmap and retrieve the bit buffer.
    HDC screen_dc = GetDC(NULL);
    HBITMAP bitmap =
        CreateDIBSection(screen_dc, reinterpret_cast<BITMAPINFO*>(&info),
                         DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen_dc);

    // Read the image into the bit buffer.
    if (bitmap && browser->GetImage(PET_VIEW, width, height, bits)) {
      // Populate the bitmap file header.
      BITMAPFILEHEADER file;
      file.bfType = 0x4d42;
      file.bfSize = sizeof(BITMAPFILEHEADER);
      file.bfReserved1 = 0;
      file.bfReserved2 = 0;
      file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

      TCHAR temp_path[512];
      if (GetBitmapTempPath(temp_path)) {
        // Write the bitmap to file.
        HANDLE file_handle =
            CreateFile(temp_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, 0);
        if (file_handle != INVALID_HANDLE_VALUE) {
          DWORD bytes_written = 0;
          WriteFile(file_handle, &file, sizeof(file), &bytes_written, 0);
          WriteFile(file_handle, &info, sizeof(info), &bytes_written, 0);
          WriteFile(file_handle, bits, width * height * 4, &bytes_written, 0);

          CloseHandle(file_handle);

          // Open the bitmap in the default viewer.
          ShellExecute(NULL, L"open", temp_path, NULL, NULL, SW_SHOWNORMAL);
          success = true;
        }
      }
    }

    DeleteObject(bitmap);
  }

  if (!success) {
    browser->GetMainFrame()->ExecuteJavaScript(
        "alert('Failed to create image!');",
        browser->GetMainFrame()->GetURL(), 0);
  }
}

}  // namespace

void RunGetImageTest(CefRefPtr<CefBrowser> browser) {
  // Execute the test function on the UI thread.
  CefPostTask(TID_UI, NewCefRunnableFunction(UIT_RunGetImageTest, browser));
}
