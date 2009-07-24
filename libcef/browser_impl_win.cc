// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "context.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "stream_impl.h"
#include "printing/units.h"
#include "../patch/patch_state.h"

#include "base/string_util.h"
#include "base/win_util.h"
#include "skia/ext/vector_canvas.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"

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
      static_cast<CefBrowserImpl*>(win_util::GetWindowUserData(hwnd));

  switch (message) {
  case WM_COMMAND:
    {
      int wmId    = LOWORD(wParam);
      int wmEvent = HIWORD(wParam);


    }
    break;

  case WM_DESTROY:
    {
      CefRefPtr<CefHandler> handler = browser->GetHandler();
      if(handler.get()) {
        // Notify the handler that the window is about to be closed
        handler->HandleBeforeWindowClose(browser);
      }
      RevokeDragDrop(browser->GetWebViewWndHandle());

      // Call GC twice to clean up garbage.
      browser->GetWebView()->GetMainFrame()->CallJSGC();
      browser->GetWebView()->GetMainFrame()->CallJSGC();

      // Clean up anything associated with the WebViewHost widget.
      browser->GetWebViewHost()->webwidget()->close();

      // Remove the browser from the list maintained by the context
      _Context->RemoveBrowser(browser);
    }
    return 0;

  case WM_SIZE:
    if (browser && browser->GetWebView()) {
      // resize the web view window to the full size of the browser window
      RECT rc;
      GetClientRect(browser->GetMainWndHandle(), &rc);
      MoveWindow(browser->GetWebViewWndHandle(), 0, 0, rc.right, rc.bottom,
          TRUE);
    }
    return 0;

  case WM_SETFOCUS:
    if (browser && browser->GetWebView())
      browser->GetWebView()->setFocus(true);
    return 0;

  case WM_KILLFOCUS:
    if (browser && browser->GetWebView())
      browser->GetWebView()->setFocus(false);
    return 0;

  case WM_ERASEBKGND:
    return 0;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

CefWindowHandle CefBrowserImpl::GetWindowHandle()
{
  Lock();
  CefWindowHandle handle = window_info_.m_hWnd;
  Unlock();
  return handle;
}

std::wstring CefBrowserImpl::GetSource(CefRefPtr<CefFrame> frame)
{
  if(!_Context->RunningOnUIThread())
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    DCHECK(hEvent != NULL);

    CefRefPtr<CefStreamWriter> stream(new CefBytesWriter(BUFFER_SIZE));

    // Request the data from the UI thread
    frame->AddRef();
    stream->AddRef();
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_GetDocumentStringNotify, frame.get(), stream.get(),
        hEvent));

    // Wait for the UI thread callback to tell us that the data is available
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);

    return UTF8ToWide(
        static_cast<CefBytesWriter*>(stream.get())->GetDataString());
  }
  else
  {
    // Retrieve the document string directly
    WebFrame* web_frame = GetWebFrame(frame);
    if(web_frame)
      return UTF8ToWide(web_frame->GetFullPageHtml());
    return std::wstring();
  }
}

std::wstring CefBrowserImpl::GetText(CefRefPtr<CefFrame> frame)
{
  if(!_Context->RunningOnUIThread())
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    DCHECK(hEvent != NULL);

    CefRefPtr<CefStreamWriter> stream(new CefBytesWriter(BUFFER_SIZE));

    // Request the data from the UI thread
    frame->AddRef();
    stream->AddRef();
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_GetDocumentTextNotify, frame.get(), stream.get(),
        hEvent));

    // Wait for the UI thread callback to tell us that the data is available
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);

    return UTF8ToWide(
        static_cast<CefBytesWriter*>(stream.get())->GetDataString());
  }
  else
  {
    // Retrieve the document text directly
    WebFrame* web_frame = GetWebFrame(frame);
    if(web_frame)
      webkit_glue::DumpDocumentText(web_frame);
    return std::wstring();
  }
}

bool CefBrowserImpl::CanGoBack()
{
  if(!_Context->RunningOnUIThread())
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    DCHECK(hEvent != NULL);

    bool retVal = true;

    // Request the data from the UI thread
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_CanGoBackNotify, &retVal, hEvent));

    // Wait for the UI thread callback to tell us that the data is available
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);

    return retVal;
  }
  else
  {
    // Call the method directly
    return UIT_CanGoBack();
  }
}

bool CefBrowserImpl::CanGoForward()
{
  if(!_Context->RunningOnUIThread())
  {
    // We need to send the request to the UI thread and wait for the result

    // Event that will be used to signal that data is available. Start
    // in non-signaled mode so that the event will block.
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    DCHECK(hEvent != NULL);

    bool retVal = true;

    // Request the data from the UI thread
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_CanGoForwardNotify, &retVal, hEvent));

    // Wait for the UI thread callback to tell us that the data is available
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);

    return retVal;
  }
  else
  {
    // Call the method directly
    return UIT_CanGoForward();
  }
}

void CefBrowserImpl::UIT_CreateBrowser(const std::wstring& url)
{
  REQUIRE_UIT();
  
  // Create the new browser window
  window_info_.m_hWnd = CreateWindowEx(window_info_.m_dwExStyle, GetWndClass(),
      window_info_.m_windowName, window_info_.m_dwStyle,
      window_info_.m_x, window_info_.m_y, window_info_.m_nWidth,
      window_info_.m_nHeight, window_info_.m_hWndParent, window_info_.m_hMenu,
      _Context->GetInstanceHandle(), NULL);
  DCHECK(window_info_.m_hWnd != NULL);

  // Set window user data to this object for future reference from the window
  // procedure
  win_util::SetWindowUserData(window_info_.m_hWnd, this);

  // Add the new browser to the list maintained by the context
  _Context->AddBrowser(this);

  // Create the webview host object
  webviewhost_.reset(
      WebViewHost::Create(window_info_.m_hWnd, delegate_.get(),
      *_Context->GetWebPreferences()));
  GetWebView()->SetUseEditorDelegate(true);
  delegate_->RegisterDragDrop();
    
  // Size the web view window to the browser window
  RECT cr;
  GetClientRect(window_info_.m_hWnd, &cr);
  SetWindowPos(GetWebViewWndHandle(), NULL, cr.left, cr.top, cr.right,
      cr.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

  if(handler_.get()) {
    // Notify the handler that we're done creating the new window
    handler_->HandleAfterCreated(this);
  }

  if(url.size() > 0) {
    CefRefPtr<CefFrame> frame = GetMainFrame();
    frame->AddRef();
    UIT_LoadURL(frame, url.c_str());
  }
}

void CefBrowserImpl::UIT_SetFocus(WebWidgetHost* host, bool enable)
{
  REQUIRE_UIT();
    
  if (enable)
    ::SetFocus(host->view_handle());
  else if (::GetFocus() == host->view_handle())
    ::SetFocus(NULL);
}

WebKit::WebWidget* CefBrowserImpl::UIT_CreatePopupWidget(WebView* webview)
{
  REQUIRE_UIT();
  
  DCHECK(!popuphost_);
  popuphost_ = WebWidgetHost::Create(NULL, popup_delegate_.get());
  ShowWindow(GetPopupWndHandle(), SW_SHOW);

  return popuphost_->webwidget();
}

void CefBrowserImpl::UIT_ClosePopupWidget()
{
  REQUIRE_UIT();
  
  PostMessage(GetPopupWndHandle(), WM_CLOSE, 0, 0);
  popuphost_ = NULL;
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

bool CefBrowserImpl::UIT_ViewDocumentString(WebFrame *frame)
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
  WriteTextToFile(frame->GetFullPageHtml(), szTempName);

  int errorCode = (int)ShellExecute(GetMainWndHandle(), L"open", szTempName,
    NULL, NULL, SW_SHOWNORMAL);
  if(errorCode <= 32)
    return false;

  return true;
}

void CefBrowserImpl::UIT_GetDocumentStringNotify(CefFrame* frame,
                                                 CefStreamWriter* writer,
                                                 HANDLE hEvent)
{
  REQUIRE_UIT();
  
  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    // Retrieve the document string
    std::string str = web_frame->GetFullPageHtml();
    // Write the document string to the stream
    writer->Write(str.c_str(), str.size(), 1);
  }
  
  // Notify the calling thread that the data is now available
  SetEvent(hEvent);

  writer->Release();
  frame->Release();
}

void CefBrowserImpl::UIT_GetDocumentTextNotify(CefFrame* frame,
                                               CefStreamWriter* writer,
                                               HANDLE hEvent)
{
  REQUIRE_UIT();
  
  WebFrame* web_frame = GetWebFrame(frame);
  if(web_frame) {
    // Retrieve the document string
    std::wstring str = webkit_glue::DumpDocumentText(web_frame);
    std::string cstr = WideToUTF8(str);
    // Write the document string to the stream
    writer->Write(cstr.c_str(), cstr.size(), 1);
  }
  
  // Notify the calling thread that the data is now available
  SetEvent(hEvent);

  writer->Release();
  frame->Release();
}

void CefBrowserImpl::UIT_CanGoBackNotify(bool *retVal, HANDLE hEvent)
{
  REQUIRE_UIT();
  
  *retVal = UIT_CanGoBack();

  // Notify the calling thread that the data is now available
  SetEvent(hEvent);
}

void CefBrowserImpl::UIT_CanGoForwardNotify(bool *retVal, HANDLE hEvent)
{
  REQUIRE_UIT();
  
  *retVal = UIT_CanGoForward();

  // Notify the calling thread that the data is now available
  SetEvent(hEvent);
}

void CefBrowserImpl::UIT_PrintPage(int page_number, int total_pages,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
#if !CEF_PATCHES_APPLIED
  NOTREACHED() << "CEF patches must be applied to support printing.";
  return;
#endif // !CEF_PATCHES_APPLIED

  REQUIRE_UIT();

  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  settings.RenderParams(&params);

  int src_size_x = canvas_size.width();
  int src_size_y = canvas_size.height();
  float src_margin = .1f * src_size_x;

  int dest_size_x = settings.page_setup_pixels().physical_size().width();
  int dest_size_y = settings.page_setup_pixels().physical_size().height();
  float dest_margin = .1f * dest_size_x;

  print_context_.NewPage();

  HDC hDC = print_context_.context();
  BOOL res;
  
  // Save the state to make sure the context this function call does not modify
  // the device context.
  int saved_state = SaveDC(hDC);
  DCHECK_NE(saved_state, 0);

  skia::VectorCanvas canvas(hDC, dest_size_x, dest_size_y);

  // Adjust for the margin offset.
  canvas.translate(dest_margin, dest_margin);
  
  // Apply the print scaling factor.
  float print_scale = (dest_size_x - dest_margin * 2) / src_size_x;
  canvas.scale(print_scale, print_scale);
  
  // Set the clipping region to be sure to not overflow.
  SkRect clip_rect;
  clip_rect.set(0, 0, static_cast<float>(src_size_x),
      static_cast<float>(src_size_y));
  canvas.clipRect(clip_rect);
  
  // Apply the WebKit scaling factor.
  float webkit_scale = 0;
#if CEF_PATCHES_APPLIED
  webkit_scale = frame->GetPrintPageShrink(page_number);
#endif // CEF_PATCHES_APPLIED
  if (webkit_scale <= 0) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
  }
  canvas.scale(webkit_scale, webkit_scale);
  
  frame->PrintPage(page_number, &canvas);

  res = RestoreDC(hDC, saved_state);
  DCHECK_NE(res, 0);

  if(handler_.get()) {
    saved_state = SaveDC(hDC);
    DCHECK_NE(saved_state, 0);

    // Gather print header state information
    RECT rect;
    rect.left = (int)floor(dest_margin / 2);
    rect.top = rect.left;
	  rect.right = (int)ceil(dest_size_x - dest_margin / 2);
	  rect.bottom = (int)ceil(dest_size_y - dest_margin / 2);

    double scale = (double)settings.dpi() / (double)settings.desired_dpi;
    
    CefPrintInfo printInfo;
    
    printInfo.m_hDC = hDC;
    printInfo.m_Rect = rect;
    printInfo.m_Scale = scale;

    std::wstring url = UTF8ToWide(frame->GetURL().spec());
    std::wstring title = title_;

    std::wstring topLeft, topCenter, topRight;
    std::wstring bottomLeft, bottomCenter, bottomRight;

    // allow the handler to format print header and/or footer
    CefHandler::RetVal rv = handler_->HandlePrintHeaderFooter(this,
      GetCefFrame(frame), printInfo, url, title, page_number+1, total_pages,
      topLeft, topCenter, topRight, bottomLeft, bottomCenter, bottomRight);

    if(rv != RV_HANDLED) {
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
      if(topLeft.size() > 0) {
        DrawText(hDC, topLeft.c_str(), topLeft.size(), &rect,
          DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(topCenter.size() > 0) {
        DrawText(hDC, topCenter.c_str(), topCenter.size(), &rect,
          DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(topRight.size() > 0) {
        DrawText(hDC, topRight.c_str(), topRight.size(), &rect,
          DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(bottomLeft.size() > 0) {
        DrawText(hDC, bottomLeft.c_str(), bottomLeft.size(), &rect,
          DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(bottomCenter.size() > 0) {
        DrawText(hDC, bottomCenter.c_str(), bottomCenter.size(), &rect,
          DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS
          | DT_EXPANDTABS | DT_NOPREFIX);
      }
      if(bottomRight.size() > 0) {
        DrawText(hDC, bottomRight.c_str(), bottomRight.size(), &rect,
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

void CefBrowserImpl::UIT_PrintPages(WebFrame* frame) {
#if !CEF_PATCHES_APPLIED
  NOTREACHED() << "CEF patches must be applied to support printing.";
  return;
#endif // !CEF_PATCHES_APPLIED

  REQUIRE_UIT();

  TCHAR printername[512];
  DWORD size = sizeof(printername)-1;
  if(GetDefaultPrinter(printername, &size)) {
    printing::PrintSettings settings;
    settings.set_device_name(printername);
    // Initialize it.
    print_context_.InitWithSettings(settings);
  }

  if(print_context_.AskUserForSettings(
      GetMainWndHandle(), UIT_GetPagesCount(frame), false)
      != printing::PrintingContext::OK)
    return;

  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  
  settings.RenderParams(&params);

  int page_count = 0;
  gfx::Size canvas_size;
  
  canvas_size.set_width(
      printing::ConvertUnit(
          settings.page_setup_pixels().physical_size().width(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  canvas_size.set_height(
      printing::ConvertUnit(
          settings.page_setup_pixels().physical_size().height(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  page_count = frame->PrintBegin(WebSize(canvas_size));

  if (page_count) {
    bool old_state = MessageLoop::current()->NestableTasksAllowed();
    MessageLoop::current()->SetNestableTasksAllowed(false);

    // TODO(cef): Use the page title as the document name
    print_context_.NewDocument(L"New Document");
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

    MessageLoop::current()->SetNestableTasksAllowed(old_state);
  }

  frame->PrintEnd();
}

int CefBrowserImpl::UIT_GetPagesCount(WebFrame* frame)
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
          settings.page_setup_pixels().physical_size().width(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  canvas_size.set_height(
      printing::ConvertUnit(
          settings.page_setup_pixels().physical_size().height(),
          static_cast<int>(params.dpi),
          params.desired_dpi));
  page_count = frame->PrintBegin(WebSize(canvas_size));
  frame->PrintEnd();

  return page_count;
}
