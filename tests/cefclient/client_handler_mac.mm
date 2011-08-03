// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_wrapper.h"
#include "cefclient.h"
#include "client_handler.h"
#include "resource_util.h"
#include "string_util.h"
#import <Cocoa/Cocoa.h>

#ifdef TEST_REDIRECT_POPUP_URLS
#include "client_popup_handler.h"
#endif

// ClientHandler::ClientLifeSpanHandler implementation

bool ClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
                                  const CefPopupFeatures& popupFeatures,
                                  CefWindowInfo& windowInfo,
                                  const CefString& url,
                                  CefRefPtr<CefClient>& client,
                                  CefBrowserSettings& settings)
{
  REQUIRE_UI_THREAD();

#ifdef TEST_REDIRECT_POPUP_URLS
  std::string urlStr = url;
  if(urlStr.find("resources/inspector/devtools.html") == std::string::npos) {
    // Show all popup windows excluding DevTools in the current window.
    windowInfo.m_bHidden = true;
    client = new ClientPopupHandler(m_Browser);
  }
#endif // TEST_REDIRECT_POPUP_URLS

  return false;
}

bool ClientHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefRequest> request,
                                     CefString& redirectUrl,
                                     CefRefPtr<CefStreamReader>& resourceStream,
                                     CefRefPtr<CefResponse> response,
                                     int loadFlags)
{
  REQUIRE_IO_THREAD();

  std::string url = request->GetURL();
  if(url == "http://tests/request") {
    // Show the request contents
    std::string dump;
    DumpRequestContents(request, dump);
    resourceStream = CefStreamReader::CreateForData(
        (void*)dump.c_str(), dump.size());
    response->SetMimeType("text/plain");
    response->SetStatus(200);
  } else if (strstr(url.c_str(), "/ps_logo2.png") != NULL) {
    // Any time we find "ps_logo2.png" in the URL substitute in our own image
    resourceStream = GetBinaryResourceReader("logo.png");
    response->SetMimeType("image/png");
    response->SetStatus(200);
  } else if(url == "http://tests/localstorage") {
    // Show the localstorage contents
    resourceStream = GetBinaryResourceReader("localstorage.html");
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if(url == "http://tests/xmlhttprequest") {
    // Show the xmlhttprequest HTML contents
    resourceStream = GetBinaryResourceReader("xmlhttprequest.html");
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if(url == "http://tests/domaccess") {
    // Show the domaccess HTML contents
    resourceStream = GetBinaryResourceReader("domaccess.html");
    response->SetMimeType("text/html");
    response->SetStatus(200);
  }

  return false;
}

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url)
{
  REQUIRE_UI_THREAD();

  if(m_BrowserHwnd == browser->GetWindowHandle() && frame->IsMain())
  {
    // Set the edit window text
    NSTextField* textField = (NSTextField*)m_EditHwnd;
    std::string urlStr(url);
    NSString* str = [NSString stringWithUTF8String:urlStr.c_str()];
    [textField setStringValue:str];
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title)
{
  REQUIRE_UI_THREAD();

  // Set the frame window title bar
  NSView* view = (NSView*)browser->GetWindowHandle();
  NSWindow* window = [view window];
  std::string titleStr(title);
  NSString* str = [NSString stringWithUTF8String:titleStr.c_str()];
  [window setTitle:str];
}

void ClientHandler::SendNotification(NotificationType type)
{
  SEL sel = nil;
  switch(type) {
    case NOTIFY_CONSOLE_MESSAGE:
      sel = @selector(notifyConsoleMessage:);
      break;
    case NOTIFY_DOWNLOAD_COMPLETE:
      sel = @selector(notifyDownloadComplete:);
      break;
    case NOTIFY_DOWNLOAD_ERROR:
      sel = @selector(notifyDownloadError:);
      break;
  }

  if(sel == nil)
    return;

  NSWindow* window = [AppGetMainHwnd() window];
  NSObject* delegate = [window delegate];
  [delegate performSelectorOnMainThread:sel withObject:nil waitUntilDone:NO];
}

void ClientHandler::SetLoading(bool isLoading)
{
  // TODO(port): Change button status.
}

void ClientHandler::SetNavState(bool canGoBack, bool canGoForward)
{
  // TODO(port): Change button status.
}

void ClientHandler::CloseMainWindow()
{
  // TODO(port): Close window
}
