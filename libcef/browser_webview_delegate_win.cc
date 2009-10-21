// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "browser_webview_delegate.h"
#include "browser_drag_delegate.h"
#include "browser_drop_delegate.h"
#include "browser_navigation_controller.h"
#include "browser_impl.h"
#include "context.h"

#include <objidl.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "app/gfx/gdi_util.h"
#include "app/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "webkit/api/public/WebContextMenuData.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebView.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/window_open_disposition.h"

using WebKit::WebContextMenuData;
using WebKit::WebCursorInfo;
using WebKit::WebFrame;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

// WebViewClient --------------------------------------------------------------

WebWidget* BrowserWebViewDelegate::createPopupMenu(
    const WebPopupMenuInfo& info) {
  NOTREACHED();
  return NULL;
}

// WebWidgetClient ------------------------------------------------------------

void BrowserWebViewDelegate::show(WebNavigationPolicy) {
  if (WebWidgetHost* host = GetWidgetHost()) {
    HWND root = GetAncestor(host->view_handle(), GA_ROOT);
    ShowWindow(root, SW_SHOW);
    UpdateWindow(root);
  }
}

void BrowserWebViewDelegate::closeWidgetSoon() {
  if (this == browser_->GetWebViewDelegate()) {
    PostMessage(browser_->GetMainWndHandle(), WM_CLOSE, 0, 0);
  } else if (this == browser_->GetPopupDelegate()) {
    browser_->UIT_ClosePopupWidget();
  }
}

void BrowserWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  if (WebWidgetHost* host = GetWidgetHost()) {
    current_cursor_.InitFromCursorInfo(cursor_info);
    HINSTANCE mod_handle = GetModuleHandle(NULL);
    host->SetCursor(current_cursor_.GetCursor(mod_handle));
  }
}

WebRect BrowserWebViewDelegate::windowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    RECT rect;
    ::GetWindowRect(host->view_handle(), &rect);
    return gfx::Rect(rect);
  }
  return WebRect();
}

void BrowserWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == browser_->GetWebViewDelegate()) {
    // ignored
  } else if (this == browser_->GetPopupDelegate()) {
    MoveWindow(browser_->GetPopupWndHandle(),
               rect.x, rect.y, rect.width, rect.height, FALSE);
  }
}

WebRect BrowserWebViewDelegate::rootWindowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    RECT rect;
    HWND root_window = ::GetAncestor(host->view_handle(), GA_ROOT);
    ::GetWindowRect(root_window, &rect);
    return gfx::Rect(rect);
  }
  return WebRect();
}

WebRect BrowserWebViewDelegate::windowResizerRect() {
  // Not necessary on Windows.
  return WebRect();
}

void BrowserWebViewDelegate::runModal() {
  WebWidgetHost* host = GetWidgetHost();
  if (!host)
    return;

  show(WebKit::WebNavigationPolicyNewWindow);

  CefContext::BrowserList *list;
  CefContext::BrowserList::const_iterator i;

  _Context->Lock();
  list = _Context->GetBrowserList();
  i = list->begin();
  for (; i != list->end(); ++i) {
    if (i->get()->IsPopup())
      EnableWindow(i->get()->GetMainWndHandle(), FALSE);
  }
  _Context->Unlock();
  
  browser_->UIT_SetIsModal(true);
  MessageLoop::current()->Run();

  _Context->Lock();
  list = _Context->GetBrowserList();
  i = list->begin();
  for (; i != list->end(); ++i)
    EnableWindow(i->get()->GetMainWndHandle(), TRUE);
  _Context->Unlock();
}

// WebPluginPageDelegate ------------------------------------------------------

webkit_glue::WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
    const GURL& url,
    const std::string& mime_type,
    std::string* actual_mime_type) {
  HWND hwnd = browser_->GetWebViewHost()->view_handle();
  if (!hwnd)
    return NULL;

  bool allow_wildcard = true;
  WebPluginInfo info;
  if (!NPAPI::PluginList::Singleton()->GetPluginInfo(
          url, mime_type, allow_wildcard, &info, actual_mime_type)) {
    return NULL;
  }

  if (actual_mime_type && !actual_mime_type->empty())
    return WebPluginDelegateImpl::Create(info.path, *actual_mime_type, hwnd);
  else
    return WebPluginDelegateImpl::Create(info.path, mime_type, hwnd);
}

void BrowserWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle handle) {
  // ignored
}

void BrowserWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle handle) {
  // ignored
}

void BrowserWebViewDelegate::DidMovePlugin(
    const webkit_glue::WebPluginGeometry& move) {
  unsigned long flags = 0;

  if (move.rects_valid) {
    HRGN hrgn = ::CreateRectRgn(move.clip_rect.x(),
                                move.clip_rect.y(),
                                move.clip_rect.right(),
                                move.clip_rect.bottom());
    gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

    // Note: System will own the hrgn after we call SetWindowRgn,
    // so we don't need to call DeleteObject(hrgn)
    ::SetWindowRgn(move.window, hrgn, FALSE);
  } else {
    flags |= (SWP_NOSIZE | SWP_NOMOVE);
  }

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

void BrowserWebViewDelegate::showContextMenu(
    WebFrame* frame, const WebContextMenuData& data)
{
  POINT screen_pt = { data.mousePosition.x, data.mousePosition.y };
  MapWindowPoints(browser_->GetMainWndHandle(), HWND_DESKTOP,
      &screen_pt, 1);

  HMENU menu = NULL;
  std::list<std::wstring> label_list;

  // Enable recursive tasks on the message loop so we can get updates while
	// the context menu is being displayed.
	bool old_state = MessageLoop::current()->NestableTasksAllowed();
	MessageLoop::current()->SetNestableTasksAllowed(true);

  int edit_flags = data.editFlags;
  if(browser_->UIT_CanGoBack())
    edit_flags |= MENU_CAN_GO_BACK;
	if(browser_->UIT_CanGoForward())
    edit_flags |= MENU_CAN_GO_FORWARD;

  int type_flags = MENUTYPE_NONE;
  if(!data.pageURL.isEmpty())
    type_flags |= MENUTYPE_PAGE;
  if(!data.frameURL.isEmpty())
    type_flags |= MENUTYPE_FRAME;
  if(!data.linkURL.isEmpty())
    type_flags |= MENUTYPE_LINK;
  if(data.mediaType == WebContextMenuData::MediaTypeImage)
    type_flags |= MENUTYPE_IMAGE;
  if(!data.selectedText.isEmpty())
    type_flags |= MENUTYPE_SELECTION;
  if(data.isEditable)
    type_flags |= MENUTYPE_EDITABLE;
  if(data.isSpellCheckingEnabled && !data.misspelledWord.isEmpty())
    type_flags |= MENUTYPE_MISSPELLED_WORD;
  if(data.mediaType == WebContextMenuData::MediaTypeVideo)
    type_flags |= MENUTYPE_VIDEO;
  if(data.mediaType == WebContextMenuData::MediaTypeAudio)
    type_flags |= MENUTYPE_AUDIO;
	
  CefRefPtr<CefHandler> handler = browser_->GetHandler();
  if(handler.get()) {
    // Gather menu information
    CefHandler::MenuInfo menuInfo;
    std::wstring linkStr, imageStr, pageStr, frameStr;
    std::wstring selectedTextStr, misspelledWordStr, securityInfoStr;

    linkStr = UTF16ToWideHack(data.linkURL.spec().utf16());
    imageStr = UTF16ToWideHack(data.srcURL.spec().utf16());
    pageStr = UTF16ToWideHack(data.pageURL.spec().utf16());
    frameStr = UTF16ToWideHack(data.frameURL.spec().utf16());
    selectedTextStr = UTF16ToWideHack(data.selectedText);
    misspelledWordStr = UTF16ToWideHack(data.misspelledWord);
    securityInfoStr = UTF16ToWideHack(data.securityInfo.utf16());
    
    menuInfo.typeFlags = type_flags;
    menuInfo.x = screen_pt.x;
    menuInfo.y = screen_pt.y;
    menuInfo.linkUrl = linkStr.c_str();
    menuInfo.imageUrl = imageStr.c_str();
    menuInfo.pageUrl = pageStr.c_str();
    menuInfo.frameUrl = frameStr.c_str();
    menuInfo.selectionText = selectedTextStr.c_str();
    menuInfo.misspelledWord = misspelledWordStr.c_str();
    menuInfo.editFlags = edit_flags;
    menuInfo.securityInfo = securityInfoStr.c_str();
   
    // Notify the handler that a context menu is requested
    CefHandler::RetVal rv = handler->HandleBeforeMenu(browser_, menuInfo);
    if(rv == RV_HANDLED)
      goto end;
  }

  // Build the correct default context menu
  if (type_flags &  MENUTYPE_EDITABLE) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, MENU_ID_UNDO, L"Undo",
      !!(edit_flags & WebContextMenuData::CanUndo), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_REDO, L"Redo",
      !!(edit_flags & WebContextMenuData::CanRedo), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, MENU_ID_CUT, L"Cut",
      !!(edit_flags & WebContextMenuData::CanCut), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_COPY, L"Copy",
      !!(edit_flags & WebContextMenuData::CanCopy), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_PASTE, L"Paste",
      !!(edit_flags & WebContextMenuData::CanPaste), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_DELETE, L"Delete",
      !!(edit_flags & WebContextMenuData::CanDelete), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, MENU_ID_SELECTALL, L"Select All",
      !!(edit_flags & MENU_CAN_SELECT_ALL), label_list);
  } else if(type_flags & MENUTYPE_SELECTION) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, MENU_ID_COPY, L"Copy",
      !!(edit_flags & WebContextMenuData::CanCopy), label_list);
  } else if(type_flags & (MENUTYPE_PAGE | MENUTYPE_FRAME)) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, MENU_ID_NAV_BACK, L"Back",
      browser_->UIT_CanGoBack(), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_NAV_FORWARD, L"Forward",
      browser_->UIT_CanGoForward(), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, MENU_ID_PRINT, L"Print",
      true, label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_VIEWSOURCE, L"View Source",
      true, label_list);
  }

  if(menu) {
    // show the context menu
    int selected_id = TrackPopupMenu(menu,
      TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_RECURSE,
      screen_pt.x, screen_pt.y, 0, browser_->GetMainWndHandle(), NULL);

    if(selected_id != 0) {
      // An action was chosen
      CefHandler::MenuId menuId = static_cast<CefHandler::MenuId>(selected_id);
      bool handled = false;
      if(handler.get()) {
        // Ask the handler if it wants to handle the action
        CefHandler::RetVal rv = handler->HandleMenuAction(browser_, menuId);
        handled = (rv == RV_HANDLED);
      }

      if(!handled) {
        // Execute the action
        CefRefPtr<CefFrame> frame = browser_->GetFocusedFrame();
        frame->AddRef();
        browser_->UIT_HandleAction(menuId, frame.get());
      }
    }
  }

  DestroyMenu(menu);

end:
	MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

// Private methods ------------------------------------------------------------

void BrowserWebViewDelegate::ShowJavaScriptAlert(WebFrame* webframe,
                                                 const std::wstring& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  MessageBox(browser_->GetMainWndHandle(), message.c_str(),
             browser_->UIT_GetTitle().c_str(), MB_OK | MB_ICONWARNING);
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(WebFrame* webframe,
                                                   const std::wstring& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  int rv = MessageBox(browser_->GetMainWndHandle(), message.c_str(),
                      browser_->UIT_GetTitle().c_str(),
                      MB_YESNO | MB_ICONQUESTION);
  return (rv == IDYES);
}

bool BrowserWebViewDelegate::ShowJavaScriptPrompt(WebFrame* webframe,
                                                  const std::wstring& message,
                                                  const std::wstring& default_value,
                                                  std::wstring* result)
{
  // TODO(cef): Implement a default prompt dialog
  return false;
}
