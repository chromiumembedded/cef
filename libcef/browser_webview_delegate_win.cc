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
#include "cef_context.h"

#include <objidl.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "gfx/gdi_util.h"
#include "gfx/native_widget_types.h"
#include "gfx/point.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

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
    ShowWindow(root, SW_SHOWNORMAL);
    SetWindowPos(root, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
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
    HMODULE hModule = ::GetModuleHandle(L"libcef.dll");
    if(!hModule)
      hModule = ::GetModuleHandle(NULL);
    host->SetCursor(current_cursor_.GetCursor(hModule));
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

webkit::npapi::WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type)
{
  HWND hwnd = browser_->GetWebViewHost() ?
      browser_->GetWebViewHost()->view_handle() : NULL;
  if (!hwnd)
    return NULL;

  return webkit::npapi::WebPluginDelegateImpl::Create(file_path, mime_type, hwnd);
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
    const webkit::npapi::WebPluginGeometry& move) {
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
  CefString actual_label(label);
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
    memset(&menuInfo, 0, sizeof(menuInfo));

    CefString linkStr(std::string(data.linkURL.spec()));
    CefString imageStr(std::string(data.srcURL.spec()));
    CefString pageStr(std::string(data.pageURL.spec()));
    CefString frameStr(std::string(data.frameURL.spec()));
    CefString selectedTextStr(string16(data.selectedText));
    CefString misspelledWordStr(string16(data.misspelledWord));
    CefString securityInfoStr(std::string(data.securityInfo));
    
    menuInfo.typeFlags = type_flags;
    menuInfo.x = screen_pt.x;
    menuInfo.y = screen_pt.y;
    cef_string_set(linkStr.c_str(), linkStr.length(), &menuInfo.linkUrl, false);
    cef_string_set(imageStr.c_str(), imageStr.length(), &menuInfo.imageUrl,
        false);
    cef_string_set(pageStr.c_str(), pageStr.length(), &menuInfo.pageUrl, false);
    cef_string_set(frameStr.c_str(), frameStr.length(), &menuInfo.frameUrl,
        false);
    cef_string_set(selectedTextStr.c_str(), selectedTextStr.length(),
        &menuInfo.selectionText, false);
    cef_string_set(misspelledWordStr.c_str(), misspelledWordStr.length(),
        &menuInfo.misspelledWord, false);
    menuInfo.editFlags = edit_flags;
    cef_string_set(securityInfoStr.c_str(), securityInfoStr.length(),
        &menuInfo.securityInfo, false);
   
    // Notify the handler that a context menu is requested
    CefHandler::RetVal rv = handler->HandleBeforeMenu(browser_, menuInfo);
    if(rv == RV_HANDLED)
      goto end;
  }

  // Build the correct default context menu
  if (type_flags &  MENUTYPE_EDITABLE) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, MENU_ID_UNDO, L"Undo",
      !!(edit_flags & MENU_CAN_UNDO), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_REDO, L"Redo",
      !!(edit_flags & MENU_CAN_REDO), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, MENU_ID_CUT, L"Cut",
      !!(edit_flags & MENU_CAN_CUT), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_COPY, L"Copy",
      !!(edit_flags & MENU_CAN_COPY), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_PASTE, L"Paste",
      !!(edit_flags & MENU_CAN_PASTE), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_DELETE, L"Delete",
      !!(edit_flags & MENU_CAN_DELETE), label_list);
    AddMenuSeparator(menu, -1);
    AddMenuItem(browser_, menu, -1, MENU_ID_SELECTALL, L"Select All",
      !!(edit_flags & MENU_CAN_SELECT_ALL), label_list);
  } else if(type_flags & MENUTYPE_SELECTION) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, MENU_ID_COPY, L"Copy",
      !!(edit_flags & MENU_CAN_COPY), label_list);
  } else if(type_flags & (MENUTYPE_PAGE | MENUTYPE_FRAME)) {
    menu = CreatePopupMenu();
    AddMenuItem(browser_, menu, -1, MENU_ID_NAV_BACK, L"Back",
      !!(edit_flags & MENU_CAN_GO_BACK), label_list);
    AddMenuItem(browser_, menu, -1, MENU_ID_NAV_FORWARD, L"Forward",
      !!(edit_flags & MENU_CAN_GO_FORWARD), label_list);
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
                                                 const CefString& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  std::wstring messageStr = message;
  std::wstring titleStr = browser_->UIT_GetTitle();
  MessageBox(browser_->GetMainWndHandle(), messageStr.c_str(), titleStr.c_str(),
      MB_OK | MB_ICONWARNING);
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(WebFrame* webframe,
                                                   const CefString& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  std::wstring messageStr = message;
  std::wstring titleStr = browser_->UIT_GetTitle();
  int rv = MessageBox(browser_->GetMainWndHandle(), messageStr.c_str(),
      titleStr.c_str(), MB_YESNO | MB_ICONQUESTION);
  return (rv == IDYES);
}

bool BrowserWebViewDelegate::ShowJavaScriptPrompt(WebFrame* webframe,
                                                  const CefString& message,
                                                 const CefString& default_value,
                                                  CefString* result)
{
  // TODO(cef): Implement a default prompt dialog
  return false;
}

namespace 
{

// from chrome/browser/views/shell_dialogs_win.cc

bool RunOpenFileDialog(const std::wstring& filter, HWND owner, FilePath* path)
{
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH];
  base::wcslcpy(filename, path->value().c_str(), arraysize(filename));

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
  }
  bool success = !!GetOpenFileName(&ofn);
  if (success)
    *path = FilePath(filename);
  return success;
}

bool RunOpenMultiFileDialog(const std::wstring& filter, HWND owner,
                            std::vector<FilePath>* paths)
{
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH] = L"";

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER
               | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT;

  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
  }
  bool success = !!GetOpenFileName(&ofn);

  if (success) {
    std::vector<FilePath> files;
    const wchar_t* selection = ofn.lpstrFile;
    while (*selection) {  // Empty string indicates end of list.
      files.push_back(FilePath(selection));
      // Skip over filename and null-terminator.
      selection += files.back().value().length() + 1;
    }
    if (files.empty()) {
      success = false;
    } else if (files.size() == 1) {
      // When there is one file, it contains the path and filename.
      paths->swap(files);
    } else {
      // Otherwise, the first string is the path, and the remainder are
      // filenames.
      std::vector<FilePath>::iterator path = files.begin();
      for (std::vector<FilePath>::iterator file = path + 1;
           file != files.end(); ++file) {
        paths->push_back(path->Append(*file));
      }
    }
  }
  return success;
}

}

bool BrowserWebViewDelegate::ShowFileChooser(std::vector<FilePath>& file_names, 
                                             const bool multi_select, 
                                             const WebKit::WebString& title, 
                                             const FilePath& default_file)
{
  bool result = false;
  
  if (multi_select) {
    result = RunOpenMultiFileDialog(L"", browser_->GetMainWndHandle(),
        &file_names);
  } else {
    FilePath file_name;
    result = RunOpenFileDialog(L"", browser_->GetMainWndHandle(), &file_name);
    if (result)
      file_names.push_back(file_name);
  }

  return result;
}

