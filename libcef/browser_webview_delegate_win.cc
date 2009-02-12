// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "precompiled_libcef.h"
#include "browser_webview_delegate.h"
#include "browser_drag_delegate.h"
#include "browser_drop_delegate.h"
#include "browser_navigation_controller.h"
#include "browser_impl.h"
#include "context.h"

#include <objidl.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "base/gfx/gdi_util.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/window_open_disposition.h"

// WebViewDelegate -----------------------------------------------------------

BrowserWebViewDelegate::~BrowserWebViewDelegate() {
  RevokeDragDrop(browser_->UIT_GetWebViewWndHandle());
}

WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type) {
  HWND hwnd = gfx::NativeViewFromId(GetContainingView(webview));
  if (!hwnd)
    return NULL;

  bool allow_wildcard = true;
  
  // first, look for plugins using the normal plugin list
  WebPluginInfo info;
  if (NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, clsid,
                                                    allow_wildcard, &info,
                                                    actual_mime_type)) {
    if (actual_mime_type && !actual_mime_type->empty())
      return WebPluginDelegateImpl::Create(info.path, *actual_mime_type, hwnd);
    else
      return WebPluginDelegateImpl::Create(info.path, mime_type, hwnd);
  }

  return NULL;
}

void BrowserWebViewDelegate::Show(WebWidget* webwidget, WindowOpenDisposition) {
  if (webwidget == browser_->UIT_GetWebView()) {
    ShowWindow(browser_->UIT_GetMainWndHandle(), SW_SHOW);
    UpdateWindow(browser_->UIT_GetMainWndHandle());
  } else if (webwidget == browser_->UIT_GetPopup()) {
    ShowWindow(browser_->UIT_GetPopupWndHandle(), SW_SHOW);
    UpdateWindow(browser_->UIT_GetPopupWndHandle());
  }
}

void BrowserWebViewDelegate::CloseWidgetSoon(WebWidget* webwidget) {
  if (webwidget == browser_->UIT_GetWebView()) {
    PostMessage(browser_->UIT_GetMainWndHandle(), WM_CLOSE, 0, 0);
  } else if (webwidget == browser_->UIT_GetPopup()) {
    browser_->UIT_ClosePopupWidget();
  }
}

void BrowserWebViewDelegate::SetCursor(WebWidget* webwidget,
                                    const WebCursor& cursor) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    current_cursor_ = cursor;
    HINSTANCE mod_handle = GetModuleHandle(NULL);
    host->SetCursor(current_cursor_.GetCursor(mod_handle));
  }
}

void BrowserWebViewDelegate::GetWindowRect(WebWidget* webwidget,
                                        gfx::Rect* out_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    RECT rect;
    ::GetWindowRect(host->window_handle(), &rect);
    *out_rect = gfx::Rect(rect);
  }
}

void BrowserWebViewDelegate::SetWindowRect(WebWidget* webwidget,
                                        const gfx::Rect& rect) {
  if (webwidget == browser_->UIT_GetWebView()) {
    // ignored
  } else if (webwidget == browser_->UIT_GetPopup()) {
    MoveWindow(browser_->UIT_GetPopupWndHandle(),
               rect.x(), rect.y(), rect.width(), rect.height(), FALSE);
  }
}

void BrowserWebViewDelegate::GetRootWindowRect(WebWidget* webwidget,
                                            gfx::Rect* out_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    RECT rect;
    HWND root_window = ::GetAncestor(host->window_handle(), GA_ROOT);
    ::GetWindowRect(root_window, &rect);
    *out_rect = gfx::Rect(rect);
  }
}

void BrowserWebViewDelegate::GetRootWindowResizerRect(WebWidget* webwidget, 
                                                      gfx::Rect* out_rect) {
  // Not necessary on Windows.
  *out_rect = gfx::Rect();
}

void BrowserWebViewDelegate::DidMove(WebWidget* webwidget,
                                  const WebPluginGeometry& move) {
  HRGN hrgn = ::CreateRectRgn(move.clip_rect.x(),
                              move.clip_rect.y(),
                              move.clip_rect.right(),
                              move.clip_rect.bottom());
  gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

  // Note: System will own the hrgn after we call SetWindowRgn,
  // so we don't need to call DeleteObject(hrgn)
  ::SetWindowRgn(move.window, hrgn, FALSE);

  unsigned long flags = 0;
  if (move.visible)
    flags |= SWP_SHOWWINDOW;
  else
    flags |= SWP_HIDEWINDOW;

  ::SetWindowPos(move.window,
                 NULL,
                 move.window_rect.x(),
                 move.window_rect.y(),
                 move.window_rect.width(),
                 move.window_rect.height(),
                 flags);

}

void BrowserWebViewDelegate::RunModal(WebWidget* webwidget) {
  Show(webwidget, NEW_WINDOW);

  CefContext::BrowserList *list;
  CefContext::BrowserList::const_iterator i;

  _Context->Lock();
  list = _Context->GetBrowserList();
  i = list->begin();
  for (; i != list->end(); ++i) {
    if (i->get()->IsPopup())
      EnableWindow(i->get()->UIT_GetMainWndHandle(), FALSE);
  }
  _Context->Unlock();
  
  browser_->UIT_SetIsModal(true);
  MessageLoop::current()->Run();

  _Context->Lock();
  list = _Context->GetBrowserList();
  i = list->begin();
  for (; i != list->end(); ++i)
    EnableWindow(i->get()->UIT_GetMainWndHandle(), TRUE);
  _Context->Unlock();
}

static void AddMenuItem(CefRefPtr<CefBrowser> browser, HMENU menu, int index,
                        CefHandler::MenuId id, const wchar_t* label,
                        bool enabled, std::list<std::wstring>& label_list)
{
  std::wstring actual_label = label;
  CefRefPtr<CefHandler> handler = browser->GetHandler();
  if(handler.get()) {
    // Let the handler change the label if desired
    handler->HandleGetMenuLabel(browser, id, actual_label);
  }
  
  // store the label in a list to simplify memory management
  label_list.push_back(actual_label);
  
  MENUITEMINFO mii;
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
  mii.fType = MFT_STRING;
  if(!enabled) {
    mii.fMask |= MIIM_STATE;
    mii.fState = MFS_GRAYED;
  }
  mii.wID = id;
  mii.dwTypeData = const_cast<wchar_t*>(label_list.back().c_str());

  InsertMenuItem(menu, index, TRUE, &mii);
}

static void AddMenuSeparator(HMENU menu, int index)
{
  MENUITEMINFO mii;
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  mii.fType = MFT_SEPARATOR;

  InsertMenuItem(menu, index, TRUE, &mii);
}

void BrowserWebViewDelegate::ShowContextMenu(WebView* webview,
                                          ContextNode in_node,
                                          int x,
                                          int y,
                                          const GURL& link_url,
                                          const GURL& image_url,
                                          const GURL& page_url,
                                          const GURL& frame_url,
                                          const std::wstring& selection_text,
                                          const std::wstring& misspelled_word,
                                          int edit_flags,
                                          const std::string& security_info) {

  POINT screen_pt = { x, y };
	MapWindowPoints(browser_->UIT_GetMainWndHandle(), HWND_DESKTOP,
      &screen_pt, 1);

  HMENU menu = NULL;
  std::list<std::wstring> label_list;

  // Enable recursive tasks on the message loop so we can get updates while
	// the context menu is being displayed.
	bool old_state = MessageLoop::current()->NestableTasksAllowed();
	MessageLoop::current()->SetNestableTasksAllowed(true);

  if(browser_->UIT_CanGoBack())
    edit_flags |= CefHandler::CAN_GO_BACK;
	if(browser_->UIT_CanGoForward())
    edit_flags |= CefHandler::CAN_GO_FORWARD;
	
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Gather menu information
    CefHandler::MenuInfo menuInfo;
    menuInfo.typeFlags = in_node.type;
    menuInfo.x = screen_pt.x;
    menuInfo.y = screen_pt.y;
    menuInfo.linkUrl = UTF8ToWide(link_url.spec().c_str()).c_str();
    menuInfo.imageUrl = UTF8ToWide(image_url.spec().c_str()).c_str();
    menuInfo.pageUrl = UTF8ToWide(page_url.spec().c_str()).c_str();
    menuInfo.frameUrl = UTF8ToWide(frame_url.spec().c_str()).c_str();
    menuInfo.selectionText = selection_text;
    menuInfo.misspelledWord = misspelled_word;
    menuInfo.editFlags = edit_flags;
    menuInfo.securityInfo = security_info;
   
    // Notify the handler that a context menu is requested
    CefHandler::RetVal rv = handler->HandleBeforeMenu(browser_, menuInfo);
    if(rv == CefHandler::RV_HANDLED)
      goto end;
  }

  // Build the correct default context menu
  if (in_node.type & ContextNode::EDITABLE) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, CefHandler::ID_UNDO, L"Undo",
      !!(edit_flags & ContextNode::CAN_UNDO), label_list);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_REDO, L"Redo",
      !!(edit_flags & ContextNode::CAN_REDO), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_CUT, L"Cut",
      !!(edit_flags & ContextNode::CAN_CUT), label_list);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_COPY, L"Copy",
      !!(edit_flags & ContextNode::CAN_COPY), label_list);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_PASTE, L"Paste",
      !!(edit_flags & ContextNode::CAN_PASTE), label_list);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_DELETE, L"Delete",
      !!(edit_flags & ContextNode::CAN_DELETE), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_SELECTALL, L"Select All",
      !!(edit_flags & ContextNode::CAN_SELECT_ALL), label_list);
  } else if(in_node.type & ContextNode::SELECTION) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, CefHandler::ID_COPY, L"Copy",
      !!(edit_flags & ContextNode::CAN_COPY), label_list);
  } else if(in_node.type & (ContextNode::PAGE | ContextNode::FRAME)) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, CefHandler::ID_NAV_BACK, L"Back",
      browser_->UIT_CanGoBack(), label_list);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_NAV_FORWARD, L"Forward",
      browser_->UIT_CanGoForward(), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_PRINT, L"Print",
      true, label_list);
    AddMenuItem(browser_, menu, -1, CefHandler::ID_VIEWSOURCE, L"View Source",
      true, label_list);
  }

  if(menu) {
    // show the context menu
    int selected_id = TrackPopupMenu(menu,
      TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_RECURSE,
      screen_pt.x, screen_pt.y, 0, browser_->UIT_GetMainWndHandle(), NULL);

    if(selected_id != 0) {
      // An action was chosen
      CefHandler::MenuId menuId = static_cast<CefHandler::MenuId>(selected_id);
      bool handled = false;
      if(handler.get()) {
        // Ask the handler if it wants to handle the action
        CefHandler::RetVal rv = handler->HandleMenuAction(browser_, menuId);
        handled = (rv == CefHandler::RV_HANDLED);
      }

      if(!handled) {
        // Execute the action
        browser_->UIT_HandleAction(menuId, CefBrowser::TF_FOCUSED);
      }
    }
  }

  DestroyMenu(menu);

end:
	MessageLoop::current()->SetNestableTasksAllowed(old_state);
}


// Private methods -----------------------------------------------------------

void BrowserWebViewDelegate::ShowJavaScriptAlert(WebView* webview,
                                                 const std::wstring& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  MessageBox(browser_->UIT_GetMainWndHandle(), message.c_str(),
             browser_->UIT_GetTitle().c_str(), MB_OK | MB_ICONWARNING);
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(WebView* webview,
                                                   const std::wstring& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  int rv = MessageBox(browser_->UIT_GetMainWndHandle(), message.c_str(),
                      browser_->UIT_GetTitle().c_str(),
                      MB_YESNO | MB_ICONQUESTION);
  return (rv == IDYES);
}

bool BrowserWebViewDelegate::ShowJavaScriptPrompt(WebView* webview,
                                                  const std::wstring& message,
                                                  const std::wstring& default_value,
                                                  std::wstring* result)
{
  // TODO(cef): Implement a default prompt dialog
  return false;
}
