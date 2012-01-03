// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "test_handler.h"
#include "include/cef_runnable.h"

namespace {

void NotifyEvent(base::WaitableEvent* event)
{
  event->Signal();
}

} // namespace


// TestHandler

TestHandler::TestHandler()
  : browser_hwnd_(NULL),
    completion_event_(true, false)
{
}

TestHandler::~TestHandler()
{
}

void TestHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
  AutoLock lock_scope(this);
  if(!browser->IsPopup())
  {
    // Keep the main child window, but not popup windows
    browser_ = browser;
    browser_hwnd_ = browser->GetWindowHandle();
  }
}

void TestHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
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

bool TestHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    CefString& redirectUrl,
                                    CefRefPtr<CefStreamReader>& resourceStream,
                                    CefRefPtr<CefResponse> response,
                                    int loadFlags)
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

void TestHandler::ExecuteTest()
{
  // Run the test
  RunTest();

  // Wait for the test to complete
  completion_event_.Wait();

  // Reset the event so the same test can be executed again.
  completion_event_.Reset();
}

void TestHandler::DestroyTest()
{
  AutoLock lock_scope(this);
  if(browser_hwnd_ != NULL)
    browser_->CloseBrowser();
}

void TestHandler::CreateBrowser(const CefString& url)
{
  CefWindowInfo windowInfo;
  CefBrowserSettings settings;
#if defined(OS_WIN)
  windowInfo.SetAsPopup(NULL, "CefUnitTest");
  windowInfo.m_dwStyle |= WS_VISIBLE;
#endif
  CefBrowser::CreateBrowser(windowInfo, this, url, settings);
}

void TestHandler::AddResource(const CefString& key, const std::string& value,
                              const CefString& mimeType)
{
  resource_map_.insert(std::make_pair(key, std::make_pair(value, mimeType)));
}

void TestHandler::ClearResources()
{
  resource_map_.clear();
}


// global functions

void WaitForThread(CefThreadId thread_id)
{
  base::WaitableEvent event(true, false);
  CefPostTask(thread_id, NewCefRunnableFunction(&NotifyEvent, &event));
  event.Wait();
}
