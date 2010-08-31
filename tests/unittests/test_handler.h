// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _TEST_HANDLER_H
#define _TEST_HANDLER_H

#include "include/cef.h"
#include "base/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

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
                                     CefRefPtr<CefHandler>& handler,
                                     std::wstring& url)
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
                                     const std::wstring& url)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleTitleChange(CefRefPtr<CefBrowser> browser,
                                   const std::wstring& title)
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
                               CefRefPtr<CefFrame> frame)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 ErrorCode errorCode,
                                 const std::wstring& failedUrl,
                                 std::wstring& errorText)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefRequest> request,
                                          std::wstring& redirectUrl,
                                          CefRefPtr<CefStreamReader>& resourceStream,
                                          std::wstring& mimeType,
                                          int loadFlags)
  {
    Lock();
    if(resource_map_.size() > 0) {
      std::wstring url = request->GetURL();
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

  virtual RetVal HandleBeforeMenu(CefRefPtr<CefBrowser> browser,
                                  const MenuInfo& menuInfo)
  {
    return RV_CONTINUE;
  }


  virtual RetVal HandleGetMenuLabel(CefRefPtr<CefBrowser> browser,
                                    MenuId menuId, std::wstring& label)
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
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSAlert(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               const std::wstring& message)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSConfirm(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const std::wstring& message, bool& retval)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSPrompt(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                const std::wstring& message,
                                const std::wstring& defaultValue,
                                bool& retval,
                                std::wstring& result)
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
                               std::wstring& text)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleConsoleMessage(CefRefPtr<CefBrowser> browser,
                                      const std::wstring& message,
                                      const std::wstring& source, int line)
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

  CefRefPtr<CefBrowser> GetBrowser()
  {
    return browser_;
  }

  HWND GetBrowserHwnd()
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

  void CreateBrowser(const std::wstring& url)
  {
    CefWindowInfo windowInfo;
#if defined(OS_WIN)
    windowInfo.SetAsPopup(NULL, L"CefUnitTest");
    windowInfo.m_dwStyle |= WS_VISIBLE;
#endif
    CefBrowser::CreateBrowser(windowInfo, false, this, url);
  }

  void AddResource(const std::wstring& key, const std::string& value,
                   const std::wstring& mimeType)
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
  typedef std::map<std::wstring, std::pair<std::string, std::wstring>> ResourceMap;
  ResourceMap resource_map_;
};

#endif // _TEST_HANDLER_H
