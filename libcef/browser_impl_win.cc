// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"
#include "browser_settings.h"
#include "printing/units.h"

#include "skia/ext/vector_canvas.h"
#include "skia/ext/vector_platform_device_emf_win.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/win/hwnd_util.h"
#include "webkit/glue/webpreferences.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <wininet.h>
#include <winspool.h>

using WebKit::WebRect;
using WebKit::WebSize;


LPCTSTR CefBrowserImpl::GetWndClass()
{
  return L"CefBrowserWindow";
}

LRESULT CALLBACK CefBrowserImpl::WndProc(HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam)
{
  CefBrowserImpl* browser =
      static_cast<CefBrowserImpl*>(ui::GetWindowUserData(hwnd));

  switch (message) {
  case WM_CLOSE:
    if(browser) {
      bool handled(false);

      if (browser->client_.get()) {
        CefRefPtr<CefLifeSpanHandler> handler =
            browser->client_->GetLifeSpanHandler();
        if (handler.get()) {
          // Give the client a chance to handle this one.
          handled = handler->DoClose(browser);
        }
      }

      if (handled)
        return 0;

      // We are our own parent in this case.
      browser->ParentWindowWillClose();
    }
    break;

  case WM_DESTROY:
    if (browser) {
      // Clear the user data pointer.
      ui::SetWindowUserData(hwnd, NULL);

      // Destroy the browser.
      browser->UIT_DestroyBrowser();
    }
    return 0;

  case WM_SIZE:
    if (browser && browser->UIT_GetWebView()) {
      // resize the web view window to the full size of the browser window
      RECT rc;
      GetClientRect(hwnd, &rc);
      MoveWindow(browser->UIT_GetWebViewWndHandle(), 0, 0, rc.right, rc.bottom,
          TRUE);
    }
    return 0;

  case WM_SETFOCUS:
    if (browser) {
      WebViewHost* host = browser->UIT_GetWebViewHost();
      if (host) {
        bool handled = false;
        CefRefPtr<CefClient> client = browser->GetClient();
        if (client.get()) {
          CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
          if (handler.get())
            handled = handler->OnSetFocus(browser, false);
        }

        if (!handled)
          browser->UIT_SetFocus(host, true);
      }
    }
    return 0;

  case WM_ERASEBKGND:
    return 0;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

void CefBrowserImpl::ParentWindowWillClose()
{
  // We must re-enable the opener (owner of the modal window) before we close
  // the popup to avoid focus/activation/z-order issues.
  if (opener_ && opener_was_disabled_by_modal_loop_) {
    HWND owner = ::GetAncestor(opener_, GA_ROOT);
    ::EnableWindow(owner, TRUE);
  }
}

CefWindowHandle CefBrowserImpl::GetWindowHandle()
{
  AutoLock lock_scope(this);
  return window_info_.m_hWnd;
}

bool CefBrowserImpl::IsWindowRenderingDisabled()
{
  return (window_info_.m_bWindowRenderingDisabled ? true : false);
}

gfx::NativeWindow CefBrowserImpl::UIT_GetMainWndHandle() {
  REQUIRE_UIT();
  return window_info_.m_bWindowRenderingDisabled ?
      window_info_.m_hWndParent : window_info_.m_hWnd;
}

void CefBrowserImpl::UIT_CreateBrowser(const CefString& url)
{
  REQUIRE_UIT();
  Lock();

  if (!window_info_.m_bWindowRenderingDisabled) {
    std::wstring windowName(CefString(&window_info_.m_windowName));

    // Create the new browser window
    window_info_.m_hWnd = CreateWindowEx(window_info_.m_dwExStyle,
        GetWndClass(), windowName.c_str(), window_info_.m_dwStyle,
        window_info_.m_x, window_info_.m_y, window_info_.m_nWidth,
        window_info_.m_nHeight, window_info_.m_hWndParent, window_info_.m_hMenu,
        ::GetModuleHandle(NULL), NULL);
    DCHECK(window_info_.m_hWnd != NULL);

    // Set window user data to this object for future reference from the window
    // procedure
    ui::SetWindowUserData(window_info_.m_hWnd, this);
  } else {
    // Create a new paint delegate.
    paint_delegate_.reset(new PaintDelegate(this));
  }

  if (!settings_.developer_tools_disabled)
    dev_tools_agent_.reset(new BrowserDevToolsAgent());

  // Add a reference that will be released in UIT_DestroyBrowser().
  AddRef();

  // Add the new browser to the list maintained by the context
  _Context->AddBrowser(this);

  WebPreferences prefs;
  BrowserToWebSettings(settings_, prefs);

  // Create the webview host object
  webviewhost_.reset(
      WebViewHost::Create(window_info_.m_hWnd, gfx::Rect(), delegate_.get(),
                          paint_delegate_.get(), dev_tools_agent_.get(),
                          prefs));

  if (!settings_.developer_tools_disabled)
    dev_tools_agent_->SetWebView(webviewhost_->webview());

  Unlock();

  if (!window_info_.m_bWindowRenderingDisabled) {
    if (!settings_.drag_drop_disabled)
      delegate_->RegisterDragDrop();
    
    // Size the web view window to the browser window
    RECT cr;
    GetClientRect(window_info_.m_hWnd, &cr);

    // Respect the WS_VISIBLE window style when setting the window's position
    UINT flags = SWP_NOZORDER;
    if (window_info_.m_dwStyle & WS_VISIBLE)
      flags |= SWP_SHOWWINDOW;
    else
      flags |= SWP_NOACTIVATE;

    SetWindowPos(UIT_GetWebViewWndHandle(), NULL, cr.left, cr.top, cr.right,
                 cr.bottom, flags);
  }

  if(client_.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      // Notify the handler that we're done creating the new window
      handler->OnAfterCreated(this);
    }
  }

  if(url.length() > 0)
    UIT_LoadURL(GetMainFrame(), url);
}

void CefBrowserImpl::UIT_SetFocus(WebWidgetHost* host, bool enable)
{
  REQUIRE_UIT();
  if (!host)
    return;

  if (enable)
    ::SetFocus(host->view_handle());
  else if (::GetFocus() == host->view_handle())
    ::SetFocus(NULL);
}

static void WriteTextToFile(const std::string& data,
                            const std::wstring& file_path)
{
  FILE* fp;
  errno_t err = _wfopen_s(&fp, file_path.c_str(), L"wt");
  if (err)
      return;
  fwrite(data.c_str(), 1, data.size(), fp);
  fclose(fp);
}

bool CefBrowserImpl::UIT_ViewDocumentString(WebKit::WebFrame *frame)
{
  REQUIRE_UIT();
  
  DWORD dwRetVal;
  DWORD dwBufSize = 512;
  TCHAR lpPathBuffer[512];
  UINT uRetVal;
  TCHAR szTempName[512];
    
  dwRetVal = GetTempPath(dwBufSize,     // length of the buffer
                         lpPathBuffer); // buffer for path 
  if (dwRetVal > dwBufSize || (dwRetVal == 0))
    return false;

  // Create a temporary file. 
  uRetVal = GetTempFileName(lpPathBuffer, // directory for tmp files
                            TEXT("src"),  // temp file name prefix 
                            0,            // create unique name 
                            szTempName);  // buffer for name 
  if (uRetVal == 0)
    return false;
 
  size_t len = wcslen(szTempName);
  wcscpy(szTempName + len - 3, L"txt");
  std::string markup = frame->contentAsMarkup().utf8();
  WriteTextToFile(markup, szTempName);

  int errorCode = (int)ShellExecute(UIT_GetMainWndHandle(), L"open", szTempName,
    NULL, NULL, SW_SHOWNORMAL);
  if(errorCode <= 32)
    return false;

  return true;
}

void CefBrowserImpl::UIT_PrintPage(int page_number, int total_pages,
                                   const gfx::Size& canvas_size,
                                   WebKit::WebFrame* frame) {
  REQUIRE_UIT();

  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  settings.RenderParams(&params);

  const int src_size_x = canvas_size.width();
  const int src_size_y = canvas_size.height();

  const int dest_size_x =
      settings.page_setup_pixels().printable_area().width();
  const int dest_size_y =
      settings.page_setup_pixels().printable_area().height();

  print_context_.NewPage();

  HDC hDC = print_context_.context();
  BOOL res;
  
  // Save the state to make sure the context this function call does not modify
  // the device context.
  int saved_state = SaveDC(hDC);
  DCHECK_NE(saved_state, 0);

  skia::PlatformDevice* device =
      skia::VectorPlatformDeviceEmfFactory::CreateDevice(dest_size_x,
                                                         dest_size_y,
                                                         true, hDC);
  DCHECK(device);
  skia::VectorCanvas canvas(device);

  // The hDC 0 coord is the left most printeable area and not physical area of
  // the paper so subtract that out of our canvas translate.
  const int left_margin_offset =
      settings.page_setup_pixels().effective_margins().left -
      settings.page_setup_pixels().printable_area().x();
  const int top_margin_offset =
      settings.page_setup_pixels().effective_margins().top -
      settings.page_setup_pixels().printable_area().y();

  // Adjust for the margin offset.
  canvas.translate(static_cast<float>(left_margin_offset), 
      static_cast<float>(top_margin_offset));
  
  // Apply the print scaling factor.
  const float print_scale_x =
      static_cast<float>(settings.page_setup_pixels().content_area().width())
      / src_size_x;
  const float print_scale_y =
      static_cast<float>(settings.page_setup_pixels().content_area().height())
      / src_size_y;
  canvas.scale(print_scale_x, print_scale_y);

  // Apply the WebKit scaling factor.
  const float webkit_scale = frame->getPrintPageShrink(page_number);
  if (webkit_scale <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  }
  canvas.scale(webkit_scale, webkit_scale);
  
  frame->printPage(page_number, &canvas);

  res = RestoreDC(hDC, saved_state);
  DCHECK_NE(res, 0);

  CefRefPtr<CefPrintHandler> handler;
  if (client_.get())
    handler = client_->GetPrintHandler();

  if(handler.get()) {
    saved_state = SaveDC(hDC);
    DCHECK_NE(saved_state, 0);

    // Gather print header state information
    RECT rect;
    rect.left = left_margin_offset;
    rect.top = settings.page_setup_pixels().effective_margins().header -
        settings.page_setup_pixels().printable_area().y();
    rect.right = left_margin_offset +
        settings.page_setup_pixels().content_area().width();
    rect.bottom = settings.page_setup_pixels().printable_area().height() -
        (settings.page_setup_pixels().effective_margins().footer -
            (settings.page_setup_pixels().physical_size().height() -
             settings.page_setup_pixels().printable_area().bottom()));

    const double scale = static_cast<double>(settings.dpi()) / 
        static_cast<double>(settings.desired_dpi);

    CefPrintInfo printInfo;
    
    printInfo.m_hDC = hDC;
    printInfo.m_Rect = rect;
    printInfo.m_Scale = scale;

    CefString url(frame->document().url().spec());
    CefString title = title_;

    CefString topLeft, topCenter, topRight;
    CefString bottomLeft, bottomCenter, bottomRight;

    // Allow the handler to format print header and/or footer.
    bool handled = handler->GetPrintHeaderFooter(this, UIT_GetCefFrame(frame),
      printInfo, url, title, page_number+1, total_pages, topLeft, topCenter,
      topRight, bottomLeft, bottomCenter, bottomRight);

    if (!handled) {
      // Draw handler-defined headers and/or footers.
      LOGFONT lf;
      memset(&lf, 0, sizeof(lf));
      lf.lfHeight = (int)ceil(10. * scale);
      lf.lfPitchAndFamily = FF_SWISS; 
      HFONT hFont = CreateFontIndirect(&lf);

      HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
      COLORREF hOldColor = SetTextColor(hDC, RGB(0,0,0));
      int hOldBkMode = SetBkMode(hDC, TRANSPARENT);

      // TODO(cef): Keep the header strings inside a reasonable bounding box
      // so that they don't overlap each other.
      if(topLeft.length() > 0) {
        std::wstring topLeftStr(topLeft);
        DrawText(hDC, topLeftStr.c_str(), topLeftStr.length(), &rect,
          DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(topCenter.length() > 0) {
        std::wstring topCenterStr(topCenter);
        DrawText(hDC, topCenterStr.c_str(), topCenterStr.length(), &rect,
          DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(topRight.length() > 0) {
        std::wstring topRightStr(topRight);
        DrawText(hDC, topRightStr.c_str(), topRightStr.length(), &rect,
          DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(bottomLeft.length() > 0) {
        std::wstring bottomLeftStr(bottomLeft);
        DrawText(hDC, bottomLeftStr.c_str(), bottomLeftStr.length(), &rect,
          DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(bottomCenter.length() > 0) {
        std::wstring bottomCenterStr(bottomCenter);
        DrawText(hDC, bottomCenterStr.c_str(), bottomCenterStr.length(), &rect,
          DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(bottomRight.length() > 0) {
        std::wstring bottomRightStr(bottomRight);
        DrawText(hDC, bottomRightStr.c_str(), bottomRightStr.length(), &rect,
          DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }

      SetTextColor(hDC, hOldColor);
      SelectObject(hDC, hOldFont);
      DeleteObject(hFont);
      SetBkMode(hDC, hOldBkMode);
    }

    res = RestoreDC(hDC, saved_state);
    DCHECK_NE(res, 0);
  }

  print_context_.PageDone();
}

void CefBrowserImpl::UIT_PrintPages(WebKit::WebFrame* frame) {
  REQUIRE_UIT();

  print_context_.Init();
  {
    // Make a copy of settings.
    printing::PrintSettings settings = print_context_.settings();
    cef_print_options_t print_options;
    memset(&print_options, 0, sizeof(print_options));
    settings.UpdatePrintOptions(print_options);  
    
    CefRefPtr<CefPrintHandler> handler;
    if (client_.get())
      handler = client_->GetPrintHandler();
    
    // Ask the handler if they want to update the print options.
    if (handler.get() && handler->GetPrintOptions(this, print_options)) {
      settings.UpdateFromPrintOptions(print_options);
      print_context_.InitWithSettings(settings);
    }
  }

  if(print_context_.AskUserForSettings(
      UIT_GetMainWndHandle(), UIT_GetPagesCount(frame), false)
      != printing::PrintingContext::OK)
    return;

  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  
  settings.RenderParams(&params);

  int page_count = 0;
  gfx::Size canvas_size;
  
  canvas_size.set_width(
      printing::ConvertUnit(
          settings.page_setup_pixels().content_area().width(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  canvas_size.set_height(
      printing::ConvertUnit(
          settings.page_setup_pixels().content_area().height(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  page_count = frame->printBegin(WebSize(canvas_size));

  if (page_count) {
    bool old_state = MessageLoop::current()->NestableTasksAllowed();
    MessageLoop::current()->SetNestableTasksAllowed(false);

    if (print_context_.NewDocument(title_) == printing::PrintingContext::OK) {
      if(settings.ranges.size() > 0) {
        for (unsigned x = 0; x < settings.ranges.size(); ++x) {
          const printing::PageRange& range = settings.ranges[x];
          for(int i = range.from; i <= range.to; ++i)
            UIT_PrintPage(i, page_count, canvas_size, frame);
        }
      } else {
        for(int i = 0; i < page_count; ++i)
          UIT_PrintPage(i, page_count, canvas_size, frame);
      }
      print_context_.DocumentDone();
    }

    MessageLoop::current()->SetNestableTasksAllowed(old_state);
  }

  frame->printEnd();
}

int CefBrowserImpl::UIT_GetPagesCount(WebKit::WebFrame* frame)
{
  REQUIRE_UIT();
  
  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  
  settings.RenderParams(&params);

  // The dbi will be 0 if no default printer is configured.
  if(params.dpi == 0)
    return 0;

  int page_count = 0;
  gfx::Size canvas_size;
  
  canvas_size.set_width(
      printing::ConvertUnit(
          settings.page_setup_pixels().content_area().width(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  canvas_size.set_height(
      printing::ConvertUnit(
          settings.page_setup_pixels().content_area().height(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  page_count = frame->printBegin(WebSize(canvas_size));
  frame->printEnd();

  return page_count;
}

// static
void CefBrowserImpl::UIT_CloseView(gfx::NativeView view)
{
  PostMessage(view, WM_CLOSE, 0, 0);
}

// static
bool CefBrowserImpl::UIT_IsViewVisible(gfx::NativeView view)
{
  return IsWindowVisible(view) ? true : false;
}
