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
  void reset() { gotit_ = false; }
  operator bool() const { return gotit_; }
protected:
  bool gotit_;
};

// Base implementation of CefClient for unit tests. Add new interfaces as needed
// by test cases.
class TestHandler : public CefClient,
                    public CefLifeSpanHandler,
                    public CefLoadHandler,
                    public CefRequestHandler,
                    public CefJSBindingHandler
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

  // CefClient methods. Add new methods as needed by test cases.
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE
      { return this; }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE
      { return this; }
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE
      { return this; }
  virtual CefRefPtr<CefJSBindingHandler> GetJSBindingHandler() OVERRIDE
      { return this; }
  
  // CefLifeSpanHandler methods

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE
  {
    AutoLock lock_scope(this);
    if(!browser->IsPopup())
    {
      // Keep the main child window, but not popup windows
      browser_ = browser;
      browser_hwnd_ = browser->GetWindowHandle();
    }
  }

  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE
  {
    AutoLock lock_scope(this);
    if(browser_hwnd_ == browser->GetWindowHandle())
    {
      // Free the browser pointer so that the browser can be destroyed
      browser_ = NULL;
      browser_hwnd_ = NULL;

      // Signal that the test is now complete.
      completion_event_.Signal();
    }
  }

  // CefRequestHandler methods

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    CefString& redirectUrl,
                                    CefRefPtr<CefStreamReader>& resourceStream,
                                    CefRefPtr<CefResponse> response,
                                    int loadFlags) OVERRIDE
  {
    AutoLock lock_scope(this);
    if(resource_map_.size() > 0) {
      CefString url = request->GetURL();
      ResourceMap::const_iterator it = resource_map_.find(url);
      if(it != resource_map_.end()) {
        // Return the previously mapped resource
        resourceStream = CefStreamReader::CreateForData(
            (void*)it->second.first.c_str(), it->second.first.length());
        response->SetMimeType(it->second.second);
        response->SetStatus(200);
      }
    }

    return false;
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

    // Reset the event so the same test can be executed again.
    completion_event_.Reset();
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
    CefBrowserSettings settings;
#if defined(OS_WIN)
    windowInfo.SetAsPopup(NULL, "CefUnitTest");
    windowInfo.m_dwStyle |= WS_VISIBLE;
#endif
    CefBrowser::CreateBrowser(windowInfo, this, url, settings);
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

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(TestHandler);
  // Include the default locking implementation.
  IMPLEMENT_LOCKING(TestHandler);
};

#endif // _TEST_HANDLER_H
