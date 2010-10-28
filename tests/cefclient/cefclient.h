// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_H
#define _CEFCLIENT_H

#include "include/cef.h"
#include "download_handler.h"


// Client implementation of the browser handler class
class ClientHandler : public CefThreadSafeBase<CefHandler>
{
public:
  // Implements the DownloadListener interface.
  class ClientDownloadListener : public CefThreadSafeBase<DownloadListener>
  {
  public:
    ClientDownloadListener(ClientHandler* handler) : handler_(handler) {}

    // Called when the download is complete.
    virtual void NotifyDownloadComplete(const std::wstring& fileName);

    // Called if the download fails.
    virtual void NotifyDownloadError(const std::wstring& fileName);

  private:
    ClientHandler* handler_;
  };

  ClientHandler();
  ~ClientHandler();

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
  virtual RetVal HandleAfterCreated(CefRefPtr<CefBrowser> browser);

  // Event called when the address bar changes. The return value is currently
  // ignored.
  virtual RetVal HandleAddressChange(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const std::wstring& url);

  // Event called when the page title changes. The return value is currently
  // ignored.
  virtual RetVal HandleTitleChange(CefRefPtr<CefBrowser> browser,
                                   const std::wstring& title);

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
                                 CefRefPtr<CefFrame> frame);

  // Event called when the browser is done loading a page. The |frame| pointer
  // will be empty if the event represents the overall load status and not the
  // load status for a particular frame. This event will be generated
  // irrespective of whether the request completes successfully. The return
  // value is currently ignored.
  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame);

  // Called when the browser fails to load a resource.  |errorCode| is the
  // error code number and |failedUrl| is the URL that failed to load.  To
  // provide custom error text assign the text to |errorText| and return
  // RV_HANDLED.  Otherwise, return RV_CONTINUE for the default error text.
  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 ErrorCode errorCode,
                                 const std::wstring& failedUrl,
                                 std::wstring& errorText);

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
                                          int loadFlags);

  // Called when a server indicates via the 'Content-Disposition' header that a
  // response represents a file to download. |mime_type| is the mime type for
  // the download, |file_name| is the suggested target file name and
  // |content_length| is either the value of the 'Content-Size' header or -1 if
  // no size was provided. Set |handler| to the CefDownloadHandler instance that
  // will recieve the file contents.  Return RV_CONTINUE to download the file
  // or RV_HANDLED to cancel the file download.
  /*--cef()--*/
  virtual RetVal HandleDownloadResponse(CefRefPtr<CefBrowser> browser,
                                        const std::wstring& mimeType,
                                        const std::wstring& fileName,
                                        int64 contentLength,
                                        CefRefPtr<CefDownloadHandler>& handler);

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

  // Event called to allow customization of standard print options before the
  // print dialog is displayed. |printOptions| allows specification of paper
  // size, orientation and margins. Note that the specified margins may be
  // adjusted if they are outside the range supported by the printer. All units
  // are in inches. Return RV_CONTINUE to display the default print options or
  // RV_HANDLED to display the modified |printOptions|.
  virtual RetVal HandlePrintOptions(CefRefPtr<CefBrowser> browser,
                                    CefPrintOptions& printOptions)
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
                                         std::wstring& bottomRight);

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

  // Event called for binding to a frame's JavaScript global object. The
  // return value is currently ignored.
  virtual RetVal HandleJSBinding(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Value> object);

  // Called just before a window is closed. The return value is currently
  // ignored.
  virtual RetVal HandleBeforeWindowClose(CefRefPtr<CefBrowser> browser);

  // Called when the browser component is about to loose focus. For instance,
  // if focus was on the last HTML element and the user pressed the TAB key.
  // The return value is currently ignored.
  virtual RetVal HandleTakeFocus(CefRefPtr<CefBrowser> browser, bool reverse)
  {
    return RV_CONTINUE;
  }

  // Called when the browser component is requesting focus. |isWidget| will be
  // true if the focus is requested for a child widget of the browser window.
  // Return RV_CONTINUE to allow the focus to be set or RV_HANDLED to cancel
  // setting the focus.
  virtual RetVal HandleSetFocus(CefRefPtr<CefBrowser> browser,
                                bool isWidget)
  {
    return RV_CONTINUE;
  }

  // Event called when the browser is about to display a tooltip.  |text|
  // contains the text that will be displayed in the tooltip.  To handle
  // the display of the tooltip yourself return RV_HANDLED.  Otherwise,
  // you can optionally modify |text| and then return RV_CONTINUE to allow
  // the browser to display the tooltip.
  virtual RetVal HandleTooltip(CefRefPtr<CefBrowser> browser,
                               std::wstring& text)
  {
    return RV_CONTINUE;
  }

  // Called when the browser component receives a keyboard event.
  // |type| is the type of keyboard event (see |KeyEventType|).
  // |code| is the windows scan-code for the event.
  // |modifiers| is a set of bit-flags describing any pressed modifier keys.
  // |isSystemKey| is set if Windows considers this a 'system key' message;
  //   (see http://msdn.microsoft.com/en-us/library/ms646286(VS.85).aspx)
  // Return RV_HANDLED if the keyboard event was handled or RV_CONTINUE
  // to allow the browser component to handle the event.
  RetVal HandleKeyEvent(CefRefPtr<CefBrowser> browser,
                        KeyEventType type,
                        int code,
                        int modifiers,
                        bool isSystemKey)
  {
    return RV_CONTINUE;
  }

  // Called to display a console message. Return RV_HANDLED to stop the message
  // from being output to the console.
  RetVal HandleConsoleMessage(CefRefPtr<CefBrowser> browser,
                              const std::wstring& message,
                              const std::wstring& source, int line);

  // Called to report find results returned by CefBrowser::Find(). |identifer|
  // is the identifier passed to CefBrowser::Find(), |count| is the number of
  // matches currently identified, |selectionRect| is the location of where the
  // match was found (in window coordinates), |activeMatchOrdinal| is the
  // current position in the search results, and |finalUpdate| is true if this
  // is the last find notification.  The return value is currently ignored.
  virtual RetVal HandleFindResult(CefRefPtr<CefBrowser> browser,
                                  int identifier, int count,
                                  const CefRect& selectionRect,
                                  int activeMatchOrdinal, bool finalUpdate)
  {
    return RV_CONTINUE;
  }

  // Retrieve the current navigation state flags
  void GetNavState(bool &isLoading, bool &canGoBack, bool &canGoForward);
  void SetMainHwnd(CefWindowHandle hwnd);
  CefWindowHandle GetMainHwnd() { return m_MainHwnd; }
  void SetEditHwnd(CefWindowHandle hwnd);
  CefRefPtr<CefBrowser> GetBrowser() { return m_Browser; }
  CefWindowHandle GetBrowserHwnd() { return m_BrowserHwnd; }

  std::wstring GetLogFile();

  void SetLastDownloadFile(const std::wstring& fileName);
  std::wstring GetLastDownloadFile();

  // Send a notification to the application. Notifications should not block the
  // caller.
  enum NotificationType
  {
    NOTIFY_CONSOLE_MESSAGE,
    NOTIFY_DOWNLOAD_COMPLETE,
    NOTIFY_DOWNLOAD_ERROR,
  };
  void SendNotification(NotificationType type);

protected:
  // The child browser window
  CefRefPtr<CefBrowser> m_Browser;

  // The main frame window handle
  CefWindowHandle m_MainHwnd;

  // The child browser window handle
  CefWindowHandle m_BrowserHwnd;

  // The edit window handle
  CefWindowHandle m_EditHwnd;

  // True if the page is currently loading
  bool m_bLoading;
  // True if the user can navigate backwards
  bool m_bCanGoBack;
  // True if the user can navigate forwards
  bool m_bCanGoForward;

  std::wstring m_LogFile;

  // Support for downloading files.
  CefRefPtr<DownloadListener> m_DownloadListener;
  std::wstring m_LastDownloadFile;
};


// Returns the main browser window instance.
CefRefPtr<CefBrowser> AppGetBrowser();

// Returns the main application window handle.
CefWindowHandle AppGetMainHwnd();

// Returns the application working directory.
std::wstring AppGetWorkingDirectory();

// Implementations for various tests.
void RunGetSourceTest(CefRefPtr<CefFrame> frame);
void RunGetTextTest(CefRefPtr<CefFrame> frame);
void RunRequestTest(CefRefPtr<CefBrowser> browser);
void RunJavaScriptExecuteTest(CefRefPtr<CefBrowser> browser);
void RunPopupTest(CefRefPtr<CefBrowser> browser);

#endif // _CEFCLIENT_H
