// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
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


// Implementation of the V8 handler class for the "cef.test" extension.
class ClientV8ExtensionHandler : public CefThreadSafeBase<CefV8Handler>
{
public:
  ClientV8ExtensionHandler() : test_param_(L"An initial string value.") {}
  virtual ~ClientV8ExtensionHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const std::wstring& name,
                       CefRefPtr<CefV8Value> object,
                       CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       std::wstring& exception)
  {
    if(name == L"SetTestParam")
    {
      // Handle the SetTestParam native function by saving the string argument
      // into the local member.
      if(arguments.size() != 1 || !arguments[0]->IsString())
        return false;
      
      test_param_ = arguments[0]->GetStringValue();
      return true;
    }
    else if(name == L"GetTestParam")
    {
      // Handle the GetTestParam native function by returning the local member
      // value.
      retval = CefV8Value::CreateString(test_param_);
      return true;
    }
    else if(name == L"GetTestObject")
    {
      // Handle the GetTestObject native function by creating and returning a
      // new V8 object.
      retval = CefV8Value::CreateObject(NULL);
      // Add a string parameter to the new V8 object.
      retval->SetValue(L"param", CefV8Value::CreateString(
          L"Retrieving a parameter on a native object succeeded."));
      // Add a function to the new V8 object.
      retval->SetValue(L"GetMessage",
          CefV8Value::CreateFunction(L"GetMessage", this));
      return true;
    }
    else if(name == L"GetMessage")
    {
      // Handle the GetMessage object function by returning a string.
      retval = CefV8Value::CreateString(
          L"Calling a function on a native object succeeded.");
      return true;
    }
    return false;
  }

private:
  std::wstring test_param_;
};

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

  // Register a V8 extension with the below JavaScript code that calls native
  // methods implemented in ClientV8ExtensionHandler.
  std::wstring code = L"var cef;"
    L"if (!cef)"
    L"  cef = {};"
    L"if (!cef.test)"
    L"  cef.test = {};"
    L"(function() {"
    L"  cef.test.__defineGetter__('test_param', function() {"
    L"    native function GetTestParam();"
    L"    return GetTestParam();"
    L"  });"
    L"  cef.test.__defineSetter__('test_param', function(b) {"
    L"    native function SetTestParam();"
    L"    if(b) SetTestParam(b);"
    L"  });"
    L"  cef.test.test_object = function() {"
    L"    native function GetTestObject();"
    L"    return GetTestObject();"
    L"  };"
    L"})();";
  CefRegisterExtension(L"v8/test", code, new ClientV8ExtensionHandler());

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

// Implementation of the V8 handler class for the "window.cef_test.Dump"
// function.
class ClientV8FunctionHandler : public CefThreadSafeBase<CefV8Handler>
{
public:
  ClientV8FunctionHandler() {}
  virtual ~ClientV8FunctionHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const std::wstring& name,
                       CefRefPtr<CefV8Value> object,
                       CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       std::wstring& exception)
  {
    if(name == L"Dump")
    {
      // The "Dump" function will return a human-readable dump of the input
      // arguments.
      std::wstringstream stream;
      size_t i;

      for(i = 0; i < arguments.size(); ++i)
      {
        stream << L"arg[" << i << L"] = ";
        PrintValue(arguments[i], stream, 0);
        stream << L"\n";
      }

      retval = CefV8Value::CreateString(stream.str());
      return true;
    }
    else if(name == L"Call")
    {
      // The "Call" function will execute a function to get an object and then
      // return the result of calling a function belonging to that object.  The
      // first arument is the function that will return an object and the second
      // argument is the function that will be called on that returned object.
      int argSize = arguments.size();
      if(argSize < 2 || !arguments[0]->IsFunction()
          || !arguments[1]->IsString())
        return false;

      CefV8ValueList argList;
      
      // Execute the function stored in the first argument to retrieve an
      // object.
      CefRefPtr<CefV8Value> objectPtr;
      if(!arguments[0]->ExecuteFunction(object, argList, objectPtr, exception))
        return false;
      // Verify that the returned value is an object.
      if(!objectPtr.get() || !objectPtr->IsObject())
        return false;

      // Retrieve the member function specified by name in the second argument
      // from the object.
      CefRefPtr<CefV8Value> funcPtr =
          objectPtr->GetValue(arguments[1]->GetStringValue());
      // Verify that the returned value is a function.
      if(!funcPtr.get() || !funcPtr->IsFunction())
        return false;

      // Pass any additional arguments on to the member function.
      for(int i = 2; i < argSize; ++i)
        argList.push_back(arguments[i]);
      
      // Execute the member function.
      return funcPtr->ExecuteFunction(arguments[0], argList, retval, exception);
    }
    return false;
  }

  // Simple function for formatted output of a V8 value.
  void PrintValue(CefRefPtr<CefV8Value> value, std::wstringstream &stream,
                  int indent)
  {
    std::wstringstream indent_stream;
    for(int i = 0; i < indent; ++i)
      indent_stream << L"  ";
    std::wstring indent_str = indent_stream.str();
    
    if(value->IsUndefined())
      stream << L"(undefined)";
    else if(value->IsNull())
      stream << L"(null)";
    else if(value->IsBool())
      stream << L"(bool) " << (value->GetBoolValue() ? L"true" : L"false");
    else if(value->IsInt())
      stream << L"(int) " << value->GetIntValue();
    else if(value->IsDouble())
      stream << L"(double) " << value->GetDoubleValue();
    else if(value->IsString())
      stream << L"(string) " << value->GetStringValue().c_str();
    else if(value->IsFunction())
      stream << L"(function) " << value->GetFunctionName().c_str();
    else if(value->IsArray()) {
      stream << L"(array) [";
      int len = value->GetArrayLength();
      for(int i = 0; i < len; ++i) {
        stream << L"\n  " << indent_str.c_str() << i << L" = ";
        PrintValue(value->GetValue(i), stream, indent+1);
      }
      stream << L"\n" << indent_str.c_str() << L"]";
    } else if(value->IsObject()) {
      stream << L"(object) [";
      std::vector<std::wstring> keys;
      if(value->GetKeys(keys)) {
        for(size_t i = 0; i < keys.size(); ++i) {
          stream << L"\n  " << indent_str.c_str() << keys[i].c_str() << L" = ";
          PrintValue(value->GetValue(keys[i]), stream, indent+1);
        }
      }
      stream << L"\n" << indent_str.c_str() << L"]";
    }
  }
};

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
    Unlock();
    return RV_CONTINUE;
  }

  // Event called when the address bar changes. The return value is currently
  // ignored.
  virtual RetVal HandleAddressChange(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const std::wstring& url)
  {
    if(m_BrowserHwnd == browser->GetWindowHandle() && frame->IsMain())
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
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    NavType navType, bool isRedirect)
  {
    return RV_CONTINUE;
  }

  // Event called when the browser begins loading a page.  The |frame| pointer
  // will be empty if the event represents the overall load status and not the
  // load status for a particular frame.  The return value is currently ignored.
  virtual RetVal HandleLoadStart(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame)
  {
    if(!frame.get())
    {
      Lock();
      // We've just started loading a page
      m_bLoading = true;
      m_bCanGoBack = false;
      m_bCanGoForward = false;
      Unlock();
    }
    return RV_CONTINUE;
  }

  // Event called when the browser is done loading a page. The |frame| pointer
  // will be empty if the event represents the overall load status and not the
  // load status for a particular frame. This event will be generated
  // irrespective of whether the request completes successfully. The return
  // value is currently ignored.
  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame)
  {
    if(!frame.get())
    {
      Lock();
      // We've just finished loading a page
      m_bLoading = false;
      m_bCanGoBack = browser->CanGoBack();
      m_bCanGoForward = browser->CanGoForward();
      Unlock();
    }
    return RV_CONTINUE;
  }

  // Called when the browser fails to load a resource.  |errorCode| is the
  // error code number and |failedUrl| is the URL that failed to load.  To
  // provide custom error text assign the text to |errorText| and return
  // RV_HANDLED.  Otherwise, return RV_CONTINUE for the default error text.
  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
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
                                         CefRefPtr<CefFrame> frame,
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
                               CefRefPtr<CefFrame> frame,
                               const std::wstring& message)
  {
    return RV_CONTINUE;
  }

  // Run a JS confirm request.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.  If you handled the alert
  // set |retval| to true if the user accepted the confirmation.
  virtual RetVal HandleJSConfirm(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const std::wstring& message, bool& retval)
  {
    return RV_CONTINUE;
  }

  // Run a JS prompt request.  Return RV_CONTINUE to display the default prompt
  // or RV_HANDLED if you displayed a custom prompt.  If you handled the prompt
  // set |retval| to true if the user accepted the prompt and request and
  // |result| to the resulting value.
  virtual RetVal HandleJSPrompt(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
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

  // Event called for binding to a frame's JavaScript global object. The
  // return value is currently ignored.
  virtual RetVal HandleJSBinding(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Value> object)
  {
    // Create the new V8 object.
    CefRefPtr<CefV8Value> testObjPtr = CefV8Value::CreateObject(NULL);
    // Add the new V8 object to the global window object with the name
    // "cef_test".
    object->SetValue(L"cef_test", testObjPtr);

    // Create an instance of ClientV8FunctionHandler as the V8 handler.
    CefRefPtr<CefV8Handler> handlerPtr = new ClientV8FunctionHandler();

    // Add a new V8 function to the cef_test object with the name "Dump".
    testObjPtr->SetValue(L"Dump",
        CefV8Value::CreateFunction(L"Dump", handlerPtr));
    // Add a new V8 function to the cef_test object with the name "Call".
    testObjPtr->SetValue(L"Call",
        CefV8Value::CreateFunction(L"Call", handlerPtr));

    return RV_HANDLED;
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
          browser->GetMainFrame()->LoadURL(strPtr);

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
        case ID_TESTS_JAVASCRIPT_HANDLER: // Test the V8 function handler
          if(browser.get())
          {
            std::wstring html =
              L"<html><body>ClientV8FunctionHandler says:<br><pre>"
              L"<script language=\"JavaScript\">"
              L"document.writeln(window.cef_test.Dump(false, 1, 7.6654,'bar',"
              L"  [false,true],[5, 7.654, 1, 'foo', [true, 'bar'], 8]));"
              L"document.writeln(window.cef_test.Dump(cef));"
              L"document.writeln("
              L"  window.cef_test.Call(cef.test.test_object, 'GetMessage'));"
              L"function my_object() {"
              L"  var obj = {};"
              L"  (function() {"
              L"    obj.GetMessage = function(a) {"
              L"      return 'Calling a function with value '+a+' on a user object succeeded.';"
              L"    };"
              L"  })();"
              L"  return obj;"
              L"};"
              L"document.writeln("
              L"  window.cef_test.Call(my_object, 'GetMessage', 'foobar'));"
              L"</script>"
              L"</pre></body></html>";
            browser->GetMainFrame()->LoadString(html, L"about:blank");
          }
          return 0;
        case ID_TESTS_JAVASCRIPT_HANDLER2: // Test the V8 extension handler
          if(browser.get())
          {
            std::wstring html =
              L"<html><body>ClientV8ExtensionHandler says:<br><pre>"
              L"<script language=\"JavaScript\">"
              L"cef.test.test_param ="
              L"  'Assign and retrieve a value succeeded the first time.';"
              L"document.writeln(cef.test.test_param);"
              L"cef.test.test_param ="
              L"  'Assign and retrieve a value succeeded the second time.';"
              L"document.writeln(cef.test.test_param);"
              L"var obj = cef.test.test_object();"
              L"document.writeln(obj.param);"
              L"document.writeln(obj.GetMessage());"
              L"</script>"
              L"</pre></body></html>";
            browser->GetMainFrame()->LoadString(html, L"about:blank");
          }
          return 0;
        case ID_TESTS_JAVASCRIPT_EXECUTE: // Test execution of javascript
          if(browser.get())
          {
            browser->GetMainFrame()->ExecuteJavaScript(
              L"alert('JavaScript execute works!');",
              L"about:blank", 0);
          }
          return 0;
        case ID_TESTS_PLUGIN: // Test the custom plugin
          if(browser.get())
          {
            std::wstring html =
              L"<html><body>Client Plugin:<br>"
              L"<embed type=\"application/x-client-plugin\""
              L"width=600 height=40>"
              L"</body></html>";
            browser->GetMainFrame()->LoadString(html, L"about:blank");
          }
          return 0;
        case ID_TESTS_POPUP: // Test a popup window
          if(browser.get())
          {
            browser->GetMainFrame()->ExecuteJavaScript(
              L"window.open('http://www.google.com');",
              L"about:blank", 0);
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
