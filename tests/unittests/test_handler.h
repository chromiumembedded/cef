// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _TEST_HANDLER_H
#define _TEST_HANDLER_H

#include "include/cef.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

class TrackCallback
{
public:
  TrackCallback(): gotit_(false) {}
  void yes() { gotit_ = true; }
  bool isSet() { return gotit_; }
  operator bool() const { return gotit_; }
protected:
  bool gotit_;
};

// Base implementation of CefHandler for unit tests.
class TestHandler : public CefThreadSafeBase<CefHandler>
{
public:
  TestHandler() : browser_hwnd_(NULL), completion_event_(true, false)
  {
  }

  virtual ~TestHandler()
  {
  }

  // Implement this method to run the test
  virtual void RunTest() =0;

  virtual RetVal HandleBeforeCreated(CefRefPtr<CefBrowser> parentBrowser,
                                     CefWindowInfo& createInfo, bool popup,
                                     const CefPopupFeatures& popupFeatures,
                                     CefRefPtr<CefHandler>& handler,
                                     CefString& url,
                                     CefBrowserSettings& settings)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleAfterCreated(CefRefPtr<CefBrowser> browser)
  {
    Lock();
    if(!browser->IsPopup())
    {
      // Keep the main child window, but not popup windows
      browser_ = browser;
      browser_hwnd_ = browser->GetWindowHandle();
    }
    Unlock();
    return RV_CONTINUE;
  }

  virtual RetVal HandleAddressChange(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const CefString& url)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleNavStateChange(CefRefPtr<CefBrowser> browser,
                                      bool canGoBack, bool canGoForward)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    NavType navType, bool isRedirect)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleLoadStart(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 ErrorCode errorCode,
                                 const CefString& failedUrl,
                                 CefString& errorText)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefRequest> request,
                                          CefString& redirectUrl,
                                          CefRefPtr<CefStreamReader>& resourceStream,
                                          CefString& mimeType,
                                          int loadFlags)
  {
    Lock();
    if(resource_map_.size() > 0) {
      CefString url = request->GetURL();
      ResourceMap::const_iterator it = resource_map_.find(url);
      if(it != resource_map_.end()) {
        // Return the previously mapped resource
        resourceStream = CefStreamReader::CreateForData(
            (void*)it->second.first.c_str(), it->second.first.length());
        mimeType = it->second.second;
      }
    }
    Unlock();

    return RV_CONTINUE;
  }

  virtual RetVal HandleProtocolExecution(CefRefPtr<CefBrowser> browser,
                                         const CefString& url,
                                         bool* allow_os_execution)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleDownloadResponse(CefRefPtr<CefBrowser> browser,
                                        const CefString& mimeType,
                                        const CefString& fileName,
                                        int64 contentLength,
                                        CefRefPtr<CefDownloadHandler>& handler)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleAuthenticationRequest(CefRefPtr<CefBrowser> browser,
                                             bool isProxy,
                                             const CefString& host,
                                             const CefString& realm,
                                             const CefString& scheme,
                                             CefString& username,
                                             CefString& password)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeMenu(CefRefPtr<CefBrowser> browser,
                                  const MenuInfo& menuInfo)
  {
    return RV_CONTINUE;
  }


  virtual RetVal HandleGetMenuLabel(CefRefPtr<CefBrowser> browser,
                                    MenuId menuId, CefString& label)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleMenuAction(CefRefPtr<CefBrowser> browser,
                                  MenuId menuId)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandlePrintOptions(CefRefPtr<CefBrowser> browser,
                                    CefPrintOptions& printOptions)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandlePrintHeaderFooter(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefPrintInfo& printInfo,
                                         const CefString& url,
                                         const CefString& title,
                                         int currentPage, int maxPages,
                                         CefString& topLeft,
                                         CefString& topCenter,
                                         CefString& topRight,
                                         CefString& bottomLeft,
                                         CefString& bottomCenter,
                                         CefString& bottomRight)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSAlert(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               const CefString& message)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSConfirm(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& message, bool& retval)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSPrompt(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                const CefString& message,
                                const CefString& defaultValue,
                                bool& retval,
                                CefString& result)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSBinding(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Value> object)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeWindowClose(CefRefPtr<CefBrowser> browser)
  {
    Lock();
    if(browser_hwnd_ == browser->GetWindowHandle())
    {
      // Free the browser pointer so that the browser can be destroyed
      browser_ = NULL;
      browser_hwnd_ = NULL;

      // Signal that the test is now complete.
      completion_event_.Signal();
    }
    Unlock();
    return RV_CONTINUE;
  }

  virtual RetVal HandleTakeFocus(CefRefPtr<CefBrowser> browser, bool reverse)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleSetFocus(CefRefPtr<CefBrowser> browser,
                                bool isWidget)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleKeyEvent(CefRefPtr<CefBrowser> browser,
                                KeyEventType type, int code,
                                int modifiers, bool isSystemKey)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleTooltip(CefRefPtr<CefBrowser> browser,
                               CefString& text)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleStatus(CefRefPtr<CefBrowser> browser,
                              const CefString& text, StatusType type)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleConsoleMessage(CefRefPtr<CefBrowser> browser,
                                      const CefString& message,
                                      const CefString& source, int line)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleFindResult(CefRefPtr<CefBrowser> browser,
                                  int identifier, int count,
                                  const CefRect& selectionRect,
                                  int activeMatchOrdinal, bool finalUpdate)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleGetRect(CefRefPtr<CefBrowser> browser, bool screen,
                               CefRect& rect)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleGetScreenPoint(CefRefPtr<CefBrowser> browser,
                                      int viewX, int viewY, int& screenX,
                                      int& screenY)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandlePopupChange(CefRefPtr<CefBrowser> browser, bool show,
                                   const CefRect& rect)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandlePaint(CefRefPtr<CefBrowser> browser,
                             PaintElementType type, const CefRect& dirtyRect,
                             const void* buffer)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleCursorChange(CefRefPtr<CefBrowser> browser,
                                    CefCursorHandle cursor)
  {
    return RV_CONTINUE;
  }

  CefRefPtr<CefBrowser> GetBrowser()
  {
    return browser_;
  }

  CefWindowHandle GetBrowserHwnd()
  {
    return browser_hwnd_;
  }

  // Called by the test function to execute the test.  This method blocks until
  // the test is complete. Do not reference the object after this method
  // returns.
  void ExecuteTest()
  {
    // Run the test
    RunTest();

    // Wait for the test to complete
    completion_event_.Wait();
  }

protected:
  // Destroy the browser window. Once the window is destroyed test completion
  // will be signaled.
  void DestroyTest()
  {
    Lock();
#if defined(OS_WIN)
    if(browser_hwnd_ != NULL)
      PostMessage(browser_hwnd_, WM_CLOSE, 0, 0);
#endif
    Unlock();
  }

  void CreateBrowser(const CefString& url)
  {
    CefWindowInfo windowInfo;
#if defined(OS_WIN)
    windowInfo.SetAsPopup(NULL, "CefUnitTest");
    windowInfo.m_dwStyle |= WS_VISIBLE;
#endif
    CefBrowser::CreateBrowser(windowInfo, false, this, url);
  }

  void AddResource(const CefString& key, const std::string& value,
                   const CefString& mimeType)
  {
    resource_map_.insert(std::make_pair(key, std::make_pair(value, mimeType)));
  }

  void ClearResources()
  {
    resource_map_.clear();
  }

private:
  // The child browser window
  CefRefPtr<CefBrowser> browser_;

  // The browser window handle
  CefWindowHandle browser_hwnd_;

  // Handle used to notify when the test is complete
  base::WaitableEvent completion_event_;

  // Map of resources that can be automatically loaded
  typedef std::map<CefString, std::pair<std::string, CefString> > ResourceMap;
  ResourceMap resource_map_;
};

#endif // _TEST_HANDLER_H
