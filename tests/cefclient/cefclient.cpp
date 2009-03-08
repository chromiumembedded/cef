// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.


#include "stdafx.h"
#include "cefclient.h"
#include "clientplugin.h"
#include "cef.h"

#include <sstream>

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH  255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Define this value to run CEF with messages processed using the current
// application's message loop.
#define TEST_SINGLE_THREADED_MESSAGE_LOOP

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef TEST_SINGLE_THREADED_MESSAGE_LOOP
  // Initialize the CEF with messages processed using the current application's
  // message loop.
  CefInitialize(false, std::wstring());
#else
  // Initialize the CEF with messages processed using a separate UI thread.
  CefInitialize(true, std::wstring());
#endif

  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  plugin_info.display_name = L"Client Plugin";
  plugin_info.unique_name = L"client_plugin";
  plugin_info.version = L"1, 0, 0, 1";
  plugin_info.description = L"My Example Client Plugin";

  CefPluginMimeType mime_type;
  mime_type.mime_type = L"application/x-client-plugin";
  mime_type.file_extensions.push_back(L"*");
  plugin_info.mime_types.push_back(mime_type);

  plugin_info.np_getentrypoints = NP_GetEntryPoints;
  plugin_info.np_initialize = NP_Initialize;
  plugin_info.np_shutdown = NP_Shutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);

  MSG msg;
  HACCEL hAccelTable;

  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_CEFCLIENT, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // Perform application initialization
  if (!InitInstance (hInstance, nCmdShow))
  {
    return FALSE;
  }

  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CEFCLIENT));

  // Main message loop
  while (GetMessage(&msg, NULL, 0, 0))
  {
#ifdef TEST_SINGLE_THREADED_MESSAGE_LOOP
    // Allow the CEF to do its message loop processing.
    CefDoMessageLoopWork();
#endif

    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  // Shut down the CEF
  CefShutdown();

	return (int) msg.wParam;
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
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CEFCLIENT));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_CEFCLIENT);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

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
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle,
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0, CW_USEDEFAULT,
      0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

// Client implementation of the JS handler class
// Select the "JavaScript" option from the "Tests" menu for an example
class ClientJSHandler : public CefThreadSafeBase<CefJSHandler>
{
public:
  ClientJSHandler()
  {
  }
  ~ClientJSHandler()
  {
  }

  // Return true if the specified method exists.
  virtual bool HasMethod(CefRefPtr<CefBrowser> browser,
                         const std::wstring& name)
  {
    // We have a method called "mymethod"
    return (name.compare(L"mymethod") == 0);
  }
  
  // Return true if the specified property exists.
  virtual bool HasProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name)
  {
    return false;
  }
  
  // Set the property value. Return true if the property is accepted.
  virtual bool SetProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name,
                           const CefRefPtr<CefVariant> value)
  {
    return false;
  }
  
  // Get the property value. Return true if the value is returned.
  virtual bool GetProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name,
                           CefRefPtr<CefVariant> value)
  {
    return false;
  }

  // Execute a method with the specified argument vector and return
  // value.  Return true if the method was handled.
  virtual bool ExecuteMethod(CefRefPtr<CefBrowser> browser,
                             const std::wstring& name,
                             const VariantVector& args,
                             CefRefPtr<CefVariant> retval)
  {
    // We only handle the "mymethod" method
    if(name.compare(L"mymethod") != 0)
      return false;

    // Return a description of the input arguments
    std::wstringstream ss;
    for(size_t i = 0; i < args.size(); i++)
    {
      ss << L"arg" << i;
      switch(args[i]->GetType())
      {
      case VARIANT_TYPE_NULL:
        ss << L" null";
        break;
      case VARIANT_TYPE_BOOL:
        ss << L" bool = " << args[i]->GetBool();
        break;
      case VARIANT_TYPE_INT:
        ss << L" int = " << args[i]->GetInt();
        break;
      case VARIANT_TYPE_DOUBLE:
        ss << L" double = " << args[i]->GetDouble();
        break;
      case VARIANT_TYPE_STRING:
        ss << L" string = " << args[i]->GetString().c_str();
        break;
      case VARIANT_TYPE_BOOL_ARRAY:
        ss << L" bool array = ";
        {
          std::vector<bool> vec;
          args[i]->GetBoolArray(vec);
          for(size_t x = 0; x < vec.size(); x++)
          {
            ss << vec[x];
            if(x < vec.size()-1)
              ss << L",";
          }
        }
        break;
      case VARIANT_TYPE_INT_ARRAY:
        ss << L" int array = ";
        {
          std::vector<int> vec;
          args[i]->GetIntArray(vec);
          for(size_t x = 0; x < vec.size(); x++)
          {
            ss << vec[x];
            if(x < vec.size()-1)
              ss << L",";
          }
        }
        break;
      case VARIANT_TYPE_DOUBLE_ARRAY:
        ss << L" double array = ";
        {
          std::vector<double> vec;
          args[i]->GetDoubleArray(vec);
          for(size_t x = 0; x < vec.size(); x++)
          {
            ss << vec[x];
            if(x < vec.size()-1)
              ss << L",";
          }
        }
        break;
      case VARIANT_TYPE_STRING_ARRAY:
        ss << L" string array = ";
        {
          std::vector<std::wstring> vec;
          args[i]->GetStringArray(vec);
          for(size_t x = 0; x < vec.size(); x++)
          {
            ss << vec[x].c_str();
            if(x < vec.size()-1)
              ss << L",";
          }
        }
        break;
      }
      ss << L"\n<br>";
    }

    retval->SetString(ss.str());

    return true;
  }
};

// Load a resource of type BINARY
bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes)
{
	HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(binaryId),
                            MAKEINTRESOURCE(256));
	if(hRes)
	{
		HGLOBAL hGlob = LoadResource(hInst, hRes);
		if(hGlob)
		{
			dwSize = SizeofResource(hInst, hRes);
			pBytes = (LPBYTE)LockResource(hGlob);
			if(dwSize > 0 && pBytes)
				return true;
		}
	}

	return false;
}

// Client implementation of the browser handler class
class ClientHandler : public CefThreadSafeBase<CefHandler>
{
public:
  ClientHandler()
  {
    m_MainHwnd = NULL;
    m_BrowserHwnd = NULL;
    m_EditHwnd = NULL;
    m_bLoading = false;
    m_bCanGoBack = false;
    m_bCanGoForward = false;
  }

  ~ClientHandler()
  {
  }

  // Event called before a new window is created. The |parentBrowser| parameter
  // will point to the parent browser window, if any. The |popup| parameter
  // will be true if the new window is a popup window. If you create the window
  // yourself you should populate the window handle member of |createInfo| and
  // return RV_HANDLED.  Otherwise, return RV_CONTINUE and the framework will
  // create the window.  By default, a newly created window will recieve the
  // same handler as the parent window.  To change the handler for the new
  // window modify the object that |handler| points to.
  virtual RetVal HandleBeforeCreated(CefRefPtr<CefBrowser> parentBrowser,
                                     CefWindowInfo& createInfo, bool popup,
                                     CefRefPtr<CefHandler>& handler,
                                     std::wstring& url)
  {
    return RV_CONTINUE;
  }

  // Event called after a new window is created. The return value is currently
  // ignored.
  virtual RetVal HandleAfterCreated(CefRefPtr<CefBrowser> browser)
  {
    Lock();
    if(!browser->IsPopup())
    {
      // We need to keep the main child window, but not popup windows
      m_Browser = browser;
      m_BrowserHwnd = browser->GetWindowHandle();
    }
    // Register our JavaScript "myclass" object
    browser->AddJSHandler(L"myclass", new ClientJSHandler());
    Unlock();
    return RV_CONTINUE;
  }

  // Event called when the address bar changes. The return value is currently
  // ignored.
  virtual RetVal HandleAddressChange(CefRefPtr<CefBrowser> browser,
                                     const std::wstring& url)
  {
    if(m_BrowserHwnd == browser->GetWindowHandle())
    {
      // Set the edit window text
      SetWindowText(m_EditHwnd, url.c_str());
    }
    return RV_CONTINUE;
  }

  // Event called when the page title changes. The return value is currently
  // ignored.
  virtual RetVal HandleTitleChange(CefRefPtr<CefBrowser> browser,
                                   const std::wstring& title)
  {
    // Set the frame window title bar
    HWND hwnd = browser->GetWindowHandle();
    if(!browser->IsPopup())
    {
      // The frame window will be the parent of the browser window
      hwnd = GetParent(hwnd);
    }
    SetWindowText(hwnd, title.c_str());
    return RV_CONTINUE;
  }

  // Event called before browser navigation. The client has an opportunity to
  // modify the |request| object if desired.  Return RV_HANDLED to cancel
  // navigation.
  virtual RetVal HandleBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    NavType navType, bool isRedirect)
  {
    return RV_CONTINUE;
  }

  // Event called when the browser begins loading a page.  The return value is
  // currently ignored.
  virtual RetVal HandleLoadStart(CefRefPtr<CefBrowser> browser)
  {
    Lock();
    // We've just started loading a page
    m_bLoading = true;
    m_bCanGoBack = false;
    m_bCanGoForward = false;
    Unlock();
    return RV_CONTINUE;
  }

  // Event called when the browser is done loading a page.  This event will
  // be generated irrespective of whether the request completes successfully.
  // The return value is currently ignored.
  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser)
  {
    Lock();
    // We've just finished loading a page
    m_bLoading = false;
    m_bCanGoBack = browser->CanGoBack();
    m_bCanGoForward = browser->CanGoForward();
    Unlock();
    return RV_CONTINUE;
  }

  // Called when the browser fails to load a resource.  |errorCode is the
  // error code number and |failedUrl| is the URL that failed to load.  To
  // provide custom error text assign the text to |errorText| and return
  // RV_HANDLED.  Otherwise, return RV_CONTINUE for the default error text.
  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 ErrorCode errorCode,
                                 const std::wstring& failedUrl,
                                 std::wstring& errorText)
  {
    if(errorCode == ERR_CACHE_MISS)
    {
      // Usually caused by navigating to a page with POST data via back or
      // forward buttons.
      errorText = L"<html><head><title>Expired Form Data</title></head>"
                  L"<body><h1>Expired Form Data</h1>"
                  L"<h2>Your form request has expired. "
                  L"Click reload to re-submit the form data.</h2></body>"
                  L"</html>";
    }
    else
    {
      // All other messages.
      std::wstringstream ss;
      ss <<       L"<html><head><title>Load Failed</title></head>"
                  L"<body><h1>Load Failed</h1>"
                  L"<h2>Load of URL " << failedUrl <<
                  L"failed with error code " << static_cast<int>(errorCode) <<
                  L".</h2></body>"
                  L"</html>";
      errorText = ss.str();
    }
    return RV_HANDLED;
  }

  // Event called before a resource is loaded.  To allow the resource to load
  // normally return RV_CONTINUE. To redirect the resource to a new url
  // populate the |redirectUrl| value and return RV_CONTINUE.  To specify
  // data for the resource return a CefStream object in |resourceStream|, set
  // 'mimeType| to the resource stream's mime type, and return RV_CONTINUE.
  // To cancel loading of the resource return RV_HANDLED.
  virtual RetVal HandleBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefRequest> request,
                                          std::wstring& redirectUrl,
                                          CefRefPtr<CefStreamReader>& resourceStream,
                                          std::wstring& mimeType,
                                          int loadFlags)
  {
    std::wstring url = request->GetURL();
    if(wcsstr(url.c_str(), L"logo.gif") != NULL) {
      // Any time we find "logo.gif" in the URL substitute in our own image
      DWORD dwSize;
      LPBYTE pBytes;
      if(LoadBinaryResource(IDS_LOGO, dwSize, pBytes)) {
        resourceStream = CefStreamReader::CreateForData(pBytes, dwSize);
        mimeType = L"image/jpg";
      }
    }
    return RV_CONTINUE;
  }

  // Event called before a context menu is displayed.  To cancel display of the
  // default context menu return RV_HANDLED.
  virtual RetVal HandleBeforeMenu(CefRefPtr<CefBrowser> browser,
                                  const MenuInfo& menuInfo)
  {
    return RV_CONTINUE;
  }


  // Event called to optionally override the default text for a context menu
  // item.  |label| contains the default text and may be modified to substitute
  // alternate text.  The return value is currently ignored.
  virtual RetVal HandleGetMenuLabel(CefRefPtr<CefBrowser> browser,
                                    MenuId menuId, std::wstring& label)
  {
    return RV_CONTINUE;
  }

  // Event called when an option is selected from the default context menu.
  // Return RV_HANDLED to cancel default handling of the action.
  virtual RetVal HandleMenuAction(CefRefPtr<CefBrowser> browser,
                                  MenuId menuId)
  {
    return RV_CONTINUE;
  }

  // Event called to format print headers and footers.  |printInfo| contains
  // platform-specific information about the printer context.  |url| is the
  // URL if the currently printing page, |title| is the title of the currently
  // printing page, |currentPage| is the current page number and |maxPages| is
  // the total number of pages.  Six default header locations are provided
  // by the implementation: top left, top center, top right, bottom left,
  // bottom center and bottom right.  To use one of these default locations
  // just assign a string to the appropriate variable.  To draw the header
  // and footer yourself return RV_HANDLED.  Otherwise, populate the approprate
  // variables and return RV_CONTINUE.
  virtual RetVal HandlePrintHeaderFooter(CefRefPtr<CefBrowser> browser,
                                         CefPrintInfo& printInfo,
                                         const std::wstring& url,
                                         const std::wstring& title,
                                         int currentPage, int maxPages,
                                         std::wstring& topLeft,
                                         std::wstring& topCenter,
                                         std::wstring& topRight,
                                         std::wstring& bottomLeft,
                                         std::wstring& bottomCenter,
                                         std::wstring& bottomRight)
  {
    // Place the page title at top left
    topLeft = title;
    // Place the page URL at top right
    topRight = url;
    
    // Place "Page X of Y" at bottom center
    std::wstringstream strstream;
    strstream << L"Page " << currentPage << L" of " << maxPages;
    bottomCenter = strstream.str();

    return RV_CONTINUE;
  }

  // Run a JS alert message.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.
  virtual RetVal HandleJSAlert(CefRefPtr<CefBrowser> browser,
                               const std::wstring& message)
  {
    return RV_CONTINUE;
  }

  // Run a JS confirm request.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.  If you handled the alert
  // set |retval| to true if the user accepted the confirmation.
  virtual RetVal HandleJSConfirm(CefRefPtr<CefBrowser> browser,
                                 const std::wstring& message, bool& retval)
  {
    return RV_CONTINUE;
  }

  // Run a JS prompt request.  Return RV_CONTINUE to display the default prompt
  // or RV_HANDLED if you displayed a custom prompt.  If you handled the prompt
  // set |retval| to true if the user accepted the prompt and request and
  // |result| to the resulting value.
  virtual RetVal HandleJSPrompt(CefRefPtr<CefBrowser> browser,
                                const std::wstring& message,
                                const std::wstring& defaultValue,
                                bool& retval,
                                std::wstring& result)
  {
    return RV_CONTINUE;
  }

  // Called just before a window is closed. The return value is currently
  // ignored.
  virtual RetVal HandleBeforeWindowClose(CefRefPtr<CefBrowser> browser)
  {
    if(m_BrowserHwnd == browser->GetWindowHandle())
    {
      // Free the browser pointer so that the browser can be destroyed
      m_Browser = NULL;
    }
    return RV_CONTINUE;
  }

  // Called when the browser component is about to loose focus. For instance,
  // if focus was on the last HTML element and the user pressed the TAB key.
  // The return value is currently ignored.
  virtual RetVal HandleTakeFocus(CefRefPtr<CefBrowser> browser, bool reverse)
  {
    return RV_CONTINUE;
  }

  // Retrieve the current navigation state flags
  void GetNavState(bool &isLoading, bool &canGoBack, bool &canGoForward)
  {
    Lock();
    isLoading = m_bLoading;
    canGoBack = m_bCanGoBack;
    canGoForward = m_bCanGoForward;
    Unlock();
  }

  void SetMainHwnd(HWND hwnd)
  {
    Lock();
    m_MainHwnd = hwnd;
    Unlock();
  }

  void SetEditHwnd(HWND hwnd)
  {
    Lock();
    m_EditHwnd = hwnd;
    Unlock();
  }

  CefRefPtr<CefBrowser> GetBrowser()
  {
    return m_Browser;
  }

  HWND GetBrowserHwnd()
  {
    return m_BrowserHwnd;
  }

protected:
  // The child browser window
  CefRefPtr<CefBrowser> m_Browser;

  // The main frame window handle
  HWND m_MainHwnd;

  // The child browser window handle
  HWND m_BrowserHwnd;

  // The edit window handle
  HWND m_EditHwnd;

  // True if the page is currently loading
  bool m_bLoading;
  // True if the user can navigate backwards
  bool m_bCanGoBack;
  // True if the user can navigate forwards
  bool m_bCanGoForward;
};


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  static CefRefPtr<ClientHandler> handler;
  static HWND backWnd = NULL, forwardWnd = NULL, reloadWnd = NULL,
      stopWnd = NULL, editWnd = NULL;
  static WNDPROC editWndOldProc = NULL;

  int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

  if(hWnd == editWnd)
  {
    // Callback for the edit window
    switch (message)
    {
    case WM_CHAR:
      if (wParam == VK_RETURN && handler.get())
      {
        // When the user hits the enter key load the URL
        CefRefPtr<CefBrowser> browser = handler->GetBrowser();
        wchar_t strPtr[MAX_URL_LENGTH] = {0};
        *((LPWORD)strPtr) = MAX_URL_LENGTH; 
        LRESULT strLen = SendMessage(hWnd, EM_GETLINE, 0, (LPARAM)strPtr);
        if (strLen > 0)
          browser->LoadURL(strPtr, std::wstring());

        return 0;
      }
    }

    return (LRESULT)CallWindowProc(editWndOldProc, hWnd, message, wParam, lParam);
  }
  else
  {
    // Callback for the main window
	  switch (message)
	  {
    case WM_CREATE:
      {
        // Create the single static handler class instance
        handler = new ClientHandler();
        handler->SetMainHwnd(hWnd);

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
        handler->SetEditHwnd(editWnd);
        
        rect.top += URLBAR_HEIGHT;
         
        CefWindowInfo info;

        // Initialize window info to the defaults for a child window
        info.SetAsChild(hWnd, rect);

        // Creat the new child child browser window
        CefBrowser::CreateBrowser(info, false,
            static_cast<CefRefPtr<CefHandler>>(handler),
            L"http://www.google.com");

        // Start the timer that will be used to update child window state
        SetTimer(hWnd, 1, 250, NULL);
      }
      return 0;

    case WM_TIMER:
      if(handler.get() && handler->GetBrowserHwnd())
      {
        // Retrieve the current navigation state
        bool isLoading, canGoBack, canGoForward;
        handler->GetNavState(isLoading, canGoBack, canGoForward);

        // Update the status of child windows
        EnableWindow(editWnd, TRUE);
        EnableWindow(backWnd, canGoBack);
        EnableWindow(forwardWnd, canGoForward);
        EnableWindow(reloadWnd, !isLoading);
        EnableWindow(stopWnd, isLoading);
      }
      return 0;

	  case WM_COMMAND:
      {
        CefRefPtr<CefBrowser> browser;
        if(handler.get())
          browser = handler->GetBrowser();

		    wmId    = LOWORD(wParam);
		    wmEvent = HIWORD(wParam);
		    // Parse the menu selections:
		    switch (wmId)
		    {
		    case IDM_ABOUT:
          DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			    return 0;
		    case IDM_EXIT:
			    DestroyWindow(hWnd);
			    return 0;
        case IDC_NAV_BACK:  // Back button
          if(browser.get())
            browser->GoBack();
          return 0;
        case IDC_NAV_FORWARD: // Forward button
          if(browser.get())
            browser->GoForward();
          return 0;
        case IDC_NAV_RELOAD:  // Reload button
          if(browser.get())
            browser->Reload();
          return 0;
        case IDC_NAV_STOP:  // Stop button
		     if(browser.get())
            browser->StopLoad();
          return 0;
        case ID_TESTS_JAVASCRIPT_HANDLER: // Test our javascript handler
          if(browser.get())
          {
            std::wstring html =
              L"<html><body>ClientJSHandler says:<br>"
              L"<script language=\"JavaScript\">"
              L"document.writeln(window.myclass.mymethod(false, 1, 7.6654,"
              L"'bar',[false,true],[5, 6, 1, 8],[4.54,10.032,.054],"
              L"['one','two']));"
              L"</script>"
              L"</body></html>";
            browser->LoadString(html, L"about:blank");
          }
          return 0;
        case ID_TESTS_JAVASCRIPT_EXECUTE: // Test execution of javascript
          if(browser.get())
          {
            browser->ExecuteJavaScript(L"alert('JavaScript execute works!');",
              L"about:blank", 0, TF_MAIN);
          }
          return 0;
        case ID_TESTS_PLUGIN: // Test our custom plugin
          if(browser.get())
          {
            std::wstring html =
              L"<html><body>Client Plugin:<br>"
              L"<embed type=\"application/x-client-plugin\""
              L"width=600 height=40>"
              L"</body></html>";
            browser->LoadString(html, L"about:blank");
          }
          return 0;
        case ID_TESTS_POPUP: // Test a popup window
          if(browser.get())
          {
            browser->ExecuteJavaScript(L"window.open('http://www.google.com');",
              L"about:blank", 0, TF_MAIN);
          }
          return 0;
        }
      }
		  break;

	  case WM_PAINT:
		  hdc = BeginPaint(hWnd, &ps);
		  EndPaint(hWnd, &ps);
		  return 0;

    case WM_SETFOCUS:
      if(handler.get() && handler->GetBrowserHwnd())
      {
        // Pass focus to the browser window
        PostMessage(handler->GetBrowserHwnd(), WM_SETFOCUS, wParam, NULL);
      }
      return 0;

    case WM_SIZE:
      if(handler.get() && handler->GetBrowserHwnd())
      {
        // Resize the browser window and address bar to match the new frame
        // window size
        RECT rect;
        GetClientRect(hWnd, &rect);
        rect.top += URLBAR_HEIGHT;

        int urloffset = rect.left + BUTTON_WIDTH * 4;

        HDWP hdwp = BeginDeferWindowPos(1);
        hdwp = DeferWindowPos(hdwp, editWnd, NULL, urloffset,
          0, rect.right - urloffset, URLBAR_HEIGHT, SWP_NOZORDER);
        hdwp = DeferWindowPos(hdwp, handler->GetBrowserHwnd(), NULL,
          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
          SWP_NOZORDER);
        EndDeferWindowPos(hdwp);
      }
      break;

    case WM_ERASEBKGND:
      if(handler.get() && handler->GetBrowserHwnd())
      {
        // Dont erase the background if the browser window has been loaded
        // (this avoids flashing)
		    return 0;
      }
      break;
    
	  case WM_DESTROY:
      // The frame window has exited
      KillTimer(hWnd, 1);
		  PostQuitMessage(0);
		  return 0;
    }
  	
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
