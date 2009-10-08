// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _TEST_HANDLER_H
#define _TEST_HANDLER_H

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"

// Base implementation of CefHandler for unit tests.
class TestHandler : public CefThreadSafeBase<CefHandler>
{
public:
  TestHandler() : browser_hwnd_(NULL), completion_event_(NULL)
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

      // Just in case it wasn't called already
      NotifyTestComplete();
    }
    Unlock();
    return RV_CONTINUE;
  }

  virtual RetVal HandleTakeFocus(CefRefPtr<CefBrowser> browser, bool reverse)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSBinding(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Value> object)
  {
    return RV_CONTINUE;
  }

  virtual RetVal HandleSetFocus(CefRefPtr<CefBrowser> browser,
                                bool isWidget)
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
    // Add a reference
    AddRef();

    // Create the notification event
    Lock();
    completion_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
    ASSERT_TRUE(completion_event_ != NULL);
    Unlock();

    // Run the test
    RunTest();

    // Wait for the test to complete
    WaitForSingleObject(completion_event_, INFINITE);
    Lock();
    CloseHandle(completion_event_);
    completion_event_ = NULL;
    Unlock();

    // Remove the reference
    Release();
  }

protected:
  // Called by the implementing class when the test is complete
  void NotifyTestComplete()
  {
    // Notify that the test is complete
    Lock();
    if(completion_event_ != NULL)
      SetEvent(completion_event_);
    if(browser_hwnd_ != NULL)
      PostMessage(browser_hwnd_, WM_CLOSE, 0, 0);
    Unlock();
  }

  void CreateBrowser(const std::wstring& url)
  {
    CefWindowInfo windowInfo;
    windowInfo.SetAsPopup(NULL, L"CefUnitTest");
    windowInfo.m_dwStyle |= WS_VISIBLE;
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
  HWND browser_hwnd_;

  // Handle used to notify when the test is complete
  HANDLE completion_event_;

  // Map of resources that can be automatically loaded
  typedef std::map<std::wstring, std::pair<std::string, std::wstring>> ResourceMap;
  ResourceMap resource_map_;
};

#endif // _TEST_HANDLER_H
