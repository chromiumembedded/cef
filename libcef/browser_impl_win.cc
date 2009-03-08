// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "context.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "stream_impl.h"

#include "base/string_util.h"
#include "base/win_util.h"
#include "skia/ext/vector_canvas.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <wininet.h>
#include <winspool.h>


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
      // Remove the browser from the list maintained by the context
      _Context->RemoveBrowser(browser);
    }
    return 0;

  case WM_SIZE:
    if (browser && browser->UIT_GetWebView()) {
      // resize the web view window to the full size of the browser window
      RECT rc;
      GetClientRect(browser->UIT_GetMainWndHandle(), &rc);
      MoveWindow(browser->UIT_GetWebViewWndHandle(), 0, 0, rc.right, rc.bottom,
          TRUE);

      if(browser->UIT_IsWebViewDisabled()) {
        // recreate the capture bitmap at the correct size
				HBITMAP bitmap;
				SIZE size;
        browser->UIT_CaptureWebViewBitmap(bitmap, size);
				browser->UIT_SetWebViewBitmap(bitmap, size);
			}
    }
    return 0;
  
  case WM_PAINT:
		if(browser->UIT_IsWebViewDisabled()) {
      // when web view is disabled draw the capture bitmap
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			HBITMAP bitmap;
			SIZE size;
			browser->UIT_GetWebViewBitmap(bitmap, size);

      HDC hMemDC = CreateCompatibleDC(hdc);
			HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, bitmap);
			BitBlt(hdc, 0, 0, size.cx, size.cy, hMemDC, 0, 0, SRCCOPY);

			SelectObject(hMemDC, hOldBmp);
			DeleteDC(hMemDC);
			
			EndPaint(hwnd, &ps);
			return 0;
		}
		break;

	case WM_SETFOCUS:
		if (browser && browser->UIT_GetWebView())
			 browser->UIT_GetWebView()->SetFocus(true);
		return 0;

	case WM_KILLFOCUS:
		if (browser && browser->UIT_GetWebView())
			 browser->UIT_GetWebView()->SetFocus(false);
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

std::wstring CefBrowserImpl::GetSource(TargetFrame targetFrame)
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
    stream->AddRef();
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_GetDocumentStringNotify, targetFrame, stream.get(),
        hEvent));

    // Wait for the UI thread callback to tell us that the data is available
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);

    return UTF8ToWide(
        static_cast<CefBytesWriter*>(stream.get())->GetDataString());
  }
  else
  {
    // Retrieve the frame contents directly
    WebFrame* frame;
    if(targetFrame == TF_FOCUSED)
      frame = UIT_GetWebView()->GetFocusedFrame();
    else
      frame = UIT_GetWebView()->GetMainFrame();

    // Retrieve the document string
    return UTF8ToWide(webkit_glue::GetDocumentString(frame));
  }
}

std::wstring CefBrowserImpl::GetText(TargetFrame targetFrame)
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
    stream->AddRef();
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &CefBrowserImpl::UIT_GetDocumentTextNotify, targetFrame, stream.get(),
        hEvent));

    // Wait for the UI thread callback to tell us that the data is available
    WaitForSingleObject(hEvent, INFINITE);
    CloseHandle(hEvent);

    return UTF8ToWide(
        static_cast<CefBytesWriter*>(stream.get())->GetDataString());
  }
  else
  {
    // Retrieve the frame contents directly
    WebFrame* frame;
    if(targetFrame == TF_FOCUSED)
      frame = UIT_GetWebView()->GetFocusedFrame();
    else
      frame = UIT_GetWebView()->GetMainFrame();

    // Retrieve the document string
    return webkit_glue::DumpDocumentText(frame);
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

void CefBrowserImpl::UIT_CreateBrowser()
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
  UIT_GetWebView()->SetUseEditorDelegate(true);
  delegate_->RegisterDragDrop();
    
  // Size the web view window to the browser window
	RECT cr;
	GetClientRect(window_info_.m_hWnd, &cr);
	SetWindowPos(UIT_GetWebViewWndHandle(), NULL, cr.left, cr.top, cr.right,
      cr.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

  if(handler_.get()) {
    // Notify the handler that we're done creating the new window
    handler_->HandleAfterCreated(this);
  }

  if(url_.size() > 0)
    UIT_LoadURL(url_.c_str());
}

void CefBrowserImpl::UIT_SetFocus(WebWidgetHost* host, bool enable)
{
  REQUIRE_UIT();
    
  if (enable)
    ::SetFocus(host->window_handle());
  else if (::GetFocus() == host->window_handle())
    ::SetFocus(NULL);
}

WebWidget* CefBrowserImpl::UIT_CreatePopupWidget(WebView* webview)
{
  REQUIRE_UIT();
  
  DCHECK(!popuphost_);
  popuphost_ = WebWidgetHost::Create(NULL, delegate_.get());
  ShowWindow(UIT_GetPopupWndHandle(), SW_SHOW);

  return popuphost_->webwidget();
}

void CefBrowserImpl::UIT_ClosePopupWidget()
{
  REQUIRE_UIT();
  
  PostMessage(UIT_GetPopupWndHandle(), WM_CLOSE, 0, 0);
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
  WriteTextToFile(webkit_glue::GetDocumentString(frame), szTempName);

  int errorCode = (int)ShellExecute(UIT_GetMainWndHandle(), L"open", szTempName,
    NULL, NULL, SW_SHOWNORMAL);
  if(errorCode <= 32)
    return false;

  return true;
}

void CefBrowserImpl::UIT_GetDocumentStringNotify(TargetFrame targetFrame,
                                                 CefStreamWriter* writer,
                                                 HANDLE hEvent)
{
  REQUIRE_UIT();
  
  WebFrame* frame;
  if(targetFrame == TF_FOCUSED)
    frame = UIT_GetWebView()->GetFocusedFrame();
  else
    frame = UIT_GetWebView()->GetMainFrame();

  // Retrieve the document string
  std::string str = webkit_glue::GetDocumentString(frame);
  // Write the document string to the stream
  writer->Write(str.c_str(), str.size(), 1);
  
  // Notify the calling thread that the data is now available
  SetEvent(hEvent);

  writer->Release();
}

void CefBrowserImpl::UIT_GetDocumentTextNotify(TargetFrame targetFrame,
                                               CefStreamWriter* writer,
                                               HANDLE hEvent)
{
  REQUIRE_UIT();
  
  WebFrame* frame;
  if(targetFrame == TF_FOCUSED)
    frame = UIT_GetWebView()->GetFocusedFrame();
  else
    frame = UIT_GetWebView()->GetMainFrame();

  // Retrieve the document string
  std::wstring str = webkit_glue::DumpDocumentText(frame);
  std::string cstr = WideToUTF8(str);
  // Write the document string to the stream
  writer->Write(cstr.c_str(), cstr.size(), 1);
  
  // Notify the calling thread that the data is now available
  SetEvent(hEvent);

  writer->Release();
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

int CefBrowserImpl::UIT_SwitchFrameToPrintMediaType(WebFrame* frame) {
  REQUIRE_UIT();
  
  printing::PrintParams params;
  print_context_.settings().RenderParams(&params);

  float ratio = static_cast<float>(params.desired_dpi / params.dpi);
  float paper_width = params.printable_size.width() * ratio;
  float paper_height = params.printable_size.height() * ratio;
  float minLayoutWidth = static_cast<float>(paper_width * params.min_shrink);
  float maxLayoutWidth = static_cast<float>(paper_width * params.max_shrink);

  // Safari uses: 765 & 1224. Margins aren't exactly the same either.
  // Scale = 2.222 for MDI printer.
  int pages;
  int width;
  if (!frame->SetPrintingMode(true,
                              minLayoutWidth,
                              maxLayoutWidth,
                              &width)) {
    NOTREACHED();
    pages = 0;
  } else {
    // Force to recalculate the height, otherwise it reuse the current window
    // height as the default.
    float effective_shrink = static_cast<float>(width) / paper_width;
    gfx::Size page_size(width,
                        static_cast<int>(paper_height * effective_shrink) - 1);
    WebView* view = frame->GetView();
    if (view) {
      // Hack around an issue where if the current view height is higher than
      // the page height, empty pages will be printed even if the bottom of the
      // web page is empty.
      printing_view_size_ = view->GetSize();
      view->Resize(page_size);
      view->Layout();
    }
    pages = frame->ComputePageRects(params.printable_size);
    DCHECK(pages);
  }
  return pages;
}

void CefBrowserImpl::UIT_SwitchFrameToDisplayMediaType(WebFrame* frame) {
  REQUIRE_UIT();

  // TODO(cef): Figure out how to make the frame redraw properly after printing
  // to PDF file (currently leaves a white rectangle behind the save as dialog).
  
  // Set the layout back to "normal" document; i.e. CSS media type = "screen".
  frame->SetPrintingMode(false, 0, 0, NULL);
  WebView* view = frame->GetView();
  if (view) {
    // Restore from the hack described at SwitchFrameToPrintMediaType().
    view->Resize(printing_view_size_);
    view->Layout();
    printing_view_size_.SetSize(0, 0);
  }
}

void CefBrowserImpl::UIT_PrintPage(int page_number, WebFrame* frame,
                                        int total_pages) {
  REQUIRE_UIT();
  
  if (printing_view_size_.width() < 0) {
    NOTREACHED();
    return;
  }

  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  settings.RenderParams(&params);

  gfx::Size src_size = frame->GetView()->GetSize();
  double src_size_x = src_size.width();
  double src_size_y = src_size.height();
  double src_margin = .1 * src_size_x;

  double dest_size_x = settings.page_setup_pixels().physical_size().width();
  double dest_size_y = settings.page_setup_pixels().physical_size().height();
  double dest_margin = .1 * dest_size_x;

  print_context_.NewPage();

  HDC hDC = print_context_.context();
  BOOL res;
  
  // Save the state to make sure the context this function call does not modify
  // the device context.
  int saved_state = SaveDC(hDC);
  DCHECK_NE(saved_state, 0);

  // 100% GDI based.
  skia::VectorCanvas canvas(hDC, (int)ceil(dest_size_x), (int)ceil(dest_size_y));
  canvas.translate(SkDoubleToScalar(dest_margin),
      SkDoubleToScalar(dest_margin));
  canvas.scale(SkDoubleToScalar((dest_size_x - dest_margin * 2) / src_size_x),
      SkDoubleToScalar((dest_size_y - dest_margin * 2) / src_size_y));
  
  // Set the clipping region to be sure to not overflow.
  SkRect clip_rect;
  clip_rect.set(0, 0, SkDoubleToScalar(src_size_x),
      SkDoubleToScalar(src_size_y));
  canvas.clipRect(clip_rect);
  
  if (!frame->SpoolPage(page_number-1, &canvas)) {
    NOTREACHED() << "Printing page " << page_number << " failed.";
    return;
  }

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
    CefHandler::RetVal rv = handler_->HandlePrintHeaderFooter(this, printInfo,
      url, title, page_number, total_pages, topLeft, topCenter, topRight,
      bottomLeft, bottomCenter, bottomRight);

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
      UIT_GetMainWndHandle(), UIT_GetPagesCount(frame))
      != printing::PrintingContext::OK)
    return;

  printing::PrintParams params;
  const printing::PrintSettings &settings = print_context_.settings();
  
  settings.RenderParams(&params);

  // disable the web view so we don't see printing related layout changes on
  // the screen
  UIT_DisableWebView(true);
  
  int pages = UIT_SwitchFrameToPrintMediaType(frame);
  if (pages) {
    bool old_state = MessageLoop::current()->NestableTasksAllowed();
    MessageLoop::current()->SetNestableTasksAllowed(false);

    // TODO(cef): Use the page title as the document name
	  print_context_.NewDocument(L"New Document");
	  if(settings.ranges.size() > 0) {
		  for (unsigned i = 0; i < settings.ranges.size(); ++i) {
			  const printing::PageRange& range = settings.ranges[i];
			  for(int i = range.from; i <= range.to; ++i)
				  UIT_PrintPage(i, frame, pages);
		  }
	  } else {
		  for(int i = 1; i <= pages; ++i)
			  UIT_PrintPage(i, frame, pages);
	  }
	  print_context_.DocumentDone();

	  MessageLoop::current()->SetNestableTasksAllowed(old_state);
  }
  UIT_SwitchFrameToDisplayMediaType(frame);

  // re-enable web view
  UIT_DisableWebView(false);
}

int CefBrowserImpl::UIT_GetPagesCount(WebFrame* frame)
{
	REQUIRE_UIT();
  
  int pages = UIT_SwitchFrameToPrintMediaType(frame);
	UIT_SwitchFrameToDisplayMediaType(frame);
	return pages;
}

void CefBrowserImpl::UIT_CaptureWebViewBitmap(HBITMAP &bitmap, SIZE &size)
{
  REQUIRE_UIT();
  
  webkit_glue::CaptureWebViewBitmap(UIT_GetMainWndHandle(), UIT_GetWebView(),
      bitmap, size);
}

void CefBrowserImpl::UIT_SetWebViewBitmap(HBITMAP bitmap, SIZE size)
{
	REQUIRE_UIT();
  
  if(webview_bitmap_)
		DeleteObject(webview_bitmap_);
	webview_bitmap_ = bitmap;
	webview_bitmap_size_ = size;
}
	
void CefBrowserImpl::UIT_DisableWebView(bool val)
{
	REQUIRE_UIT();
  
  if(val) {
    // disable the web view window
		if(webview_bitmap_ != NULL)
			return;

		HBITMAP bitmap;
		SIZE size;
    UIT_CaptureWebViewBitmap(bitmap, size);
		UIT_SetWebViewBitmap(bitmap, size);
	
		DWORD dwStyle = GetWindowLong(UIT_GetWebViewWndHandle(), GWL_STYLE);
		SetWindowLong(UIT_GetWebViewWndHandle(), GWL_STYLE, dwStyle & ~WS_VISIBLE);
		RedrawWindow(UIT_GetMainWndHandle(), NULL, NULL, RDW_INVALIDATE);
	} else if(webview_bitmap_) {
    // enable the web view window
		SIZE size = {0,0};
		UIT_SetWebViewBitmap(NULL, size);

		DWORD dwStyle = GetWindowLong(UIT_GetWebViewWndHandle(), GWL_STYLE);
		SetWindowLong(UIT_GetWebViewWndHandle(), GWL_STYLE, dwStyle | WS_VISIBLE);
		RedrawWindow(UIT_GetMainWndHandle(), NULL, NULL, RDW_INVALIDATE);
	}
}
