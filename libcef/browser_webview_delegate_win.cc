// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of BrowserWebViewDelegate, which serves
// as the WebViewDelegate for the BrowserWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "browser_drag_delegate_win.h"
#include "browser_navigation_controller.h"
#include "browser_impl.h"
#include "browser_webview_delegate.h"
#include "cef_context.h"
#include "web_drop_target_win.h"

#include <objidl.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

using webkit::npapi::WebPluginDelegateImpl;
using WebKit::WebContextMenuData;
using WebKit::WebCursorInfo;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebExternalPopupMenu;
using WebKit::WebExternalPopupMenuClient;
using WebKit::WebFrame;
using WebKit::WebImage;
using WebKit::WebNavigationPolicy;
using WebKit::WebPoint;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

static const wchar_t kPluginWindowClassName[] = L"WebPluginHost";

// WebViewClient --------------------------------------------------------------

WebExternalPopupMenu* BrowserWebViewDelegate::createExternalPopupMenu(
    const WebPopupMenuInfo& info,
    WebExternalPopupMenuClient* client)  {
  NOTREACHED();
  return NULL;
}

// WebWidgetClient ------------------------------------------------------------

void BrowserWebViewDelegate::show(WebNavigationPolicy) {
  if (this == browser_->UIT_GetWebViewDelegate()) {
    if (!browser_->IsWindowRenderingDisabled()) {
      // Restore the window and bring it to the top if the window is currently
      // visible.
      HWND root = GetAncestor(browser_->UIT_GetMainWndHandle(), GA_ROOT);
      if(IsWindowVisible(root)) {
        ShowWindow(root, SW_SHOWNORMAL);
        SetWindowPos(root, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
      }
    }
  } else if (this == browser_->UIT_GetPopupDelegate()) {
    if (!browser_->IsWindowRenderingDisabled()) {
      // Show popup widgets without activation.
      ShowWindow(browser_->UIT_GetPopupWndHandle(), SW_SHOWNA);
    } else {
      // Notify the handler of popup visibility change.
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
        if (handler.get())
          handler->OnPopupShow(browser_, true);
      }
    }
  }
}

void BrowserWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  if (WebWidgetHost* host = GetWidgetHost()) {
    current_cursor_.InitFromCursorInfo(cursor_info);
    HMODULE hModule = ::GetModuleHandle(L"libcef.dll");
    if(!hModule)
      hModule = ::GetModuleHandle(NULL);
    HCURSOR hCursor = current_cursor_.GetCursor(hModule);

    if (!browser_->IsWindowRenderingDisabled()) {
      host->SetCursor(hCursor);
    } else {
      // Notify the handler of cursor change.
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
        if (handler.get())
          handler->OnCursorChange(browser_, hCursor);
      }
    }
  }
}

WebRect BrowserWebViewDelegate::windowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    if (!browser_->IsWindowRenderingDisabled()) {
      RECT rect;
      ::GetWindowRect(host->view_handle(), &rect);
      return gfx::Rect(rect);
    } else {
      // Retrieve the view rectangle from the handler.
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
        if (handler.get()) {
          CefRect rect(0, 0, 0, 0);
          if (handler->GetViewRect(browser_, rect))
            return WebRect(rect.x, rect.y, rect.width, rect.height);
        }
      }
    }
  }
  return WebRect();
}

void BrowserWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == browser_->UIT_GetWebViewDelegate()) {
    // ignored
  } else if (this == browser_->UIT_GetPopupDelegate()) {
    if (!browser_->IsWindowRenderingDisabled()) {
      MoveWindow(browser_->UIT_GetPopupWndHandle(), rect.x, rect.y, rect.width,
          rect.height, FALSE);
    } else {
      browser_->set_popup_rect(rect);
      browser_->UIT_GetPopupHost()->SetSize(rect.width, rect.height);
        
      // Notify the handler of popup size change.
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
        if (handler.get()) {
          handler->OnPopupSize(browser_,
              CefRect(rect.x, rect.y, rect.width, rect.height));
        }
      }
    }
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

void BrowserWebViewDelegate::startDragging(
    const WebDragData& data,
    WebDragOperationsMask mask,
    const WebImage& image,
    const WebPoint& image_offset) {
  // Dragging is not supported when window rendering is disabled.
  if (browser_->IsWindowRenderingDisabled()) {
    EndDragging();
    return;
  }

  drag_delegate_ = new BrowserDragDelegate(this);
  drag_delegate_->StartDragging(WebDropData(data), mask, image.getSkBitmap(),
                                image_offset);
}

void BrowserWebViewDelegate::runModal() {
  WebWidgetHost* host = GetWidgetHost();
  if (!host)
    return;

  show(WebKit::WebNavigationPolicyNewWindow);

  browser_->UIT_SetIsModal(true);

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefLifeSpanHandler> handler;

  if( client.get())
    handler = client->GetLifeSpanHandler();
  
  bool handled(false);

  if (handler.get()) {
    // Let the client override the modal message loop.
    handled = handler->RunModal(browser_);
  }

  if (!handled) {
    HWND child = ::GetAncestor(browser_->UIT_GetMainWndHandle(), GA_ROOT);
    HWND owner = ::GetAncestor(browser_->opener_window(), GA_ROOT);

    if (child && owner) {
      // Set the owner so that Windows keeps this window above the owner.
      ::SetWindowLong(child, GWL_HWNDPARENT, (LONG)owner);
      // Disable the owner if it is enabled so that you can't interact with it.
      // while this child window is open.
      if (::IsWindowEnabled(owner)) {
        ::EnableWindow(owner, FALSE);
        browser_->set_opener_was_disabled_by_modal_loop(true);
      }
      DWORD dwStyle = ::GetWindowLong(child, GWL_STYLE);
      DWORD dwNewStyle = dwStyle | WS_POPUP;
      if(dwStyle != dwNewStyle)
        ::SetWindowLong(child, GWL_STYLE, dwNewStyle);
    }

    // Tell the browser to exit this message loop when this window closes.
    browser_->set_internal_modal_message_loop_is_active(true);

    // Start a new message loop here and return when this window closes.
    MessageLoop* message_loop = MessageLoop::current();
    bool old_state = message_loop->NestableTasksAllowed();
    message_loop->SetNestableTasksAllowed(true);
    message_loop->Run();
    message_loop->SetNestableTasksAllowed(old_state);
  }
}

// WebPluginPageDelegate ------------------------------------------------------

webkit::npapi::WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type)
{
  WebViewHost* host = browser_->UIT_GetWebViewHost();
  if (!host)
    return NULL;

  HWND hwnd;

  if (!browser_->IsWindowRenderingDisabled()) {
    // Parent the plugin container to the existing browser window.
    hwnd = browser_->UIT_GetWebViewHost()->view_handle();
    DCHECK(hwnd != NULL);
  } else {
    // Parent the plugin container to the main window handle provided by the
    // user.
    hwnd = browser_->UIT_GetMainWndHandle();
    DCHECK(hwnd != NULL);
  }

  return WebPluginDelegateImpl::Create(file_path, mime_type, hwnd);
}

void BrowserWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle handle) {
  if (browser_->IsWindowRenderingDisabled()) {
    static bool registered_class = false;
    if (!registered_class) {
      WNDCLASSEX wcex = {0};
      wcex.cbSize        = sizeof(wcex);
      wcex.style         = CS_DBLCLKS;
      wcex.lpfnWndProc   = DefWindowProc;
      wcex.hInstance     = GetModuleHandle(NULL);
      wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
      wcex.lpszClassName = kPluginWindowClassName;
      RegisterClassEx(&wcex);
      registered_class = true;
    }

    // Parent windowed plugin containers to a hidden window.
    HWND parent = CreateWindow(kPluginWindowClassName, NULL,
                               WS_OVERLAPPED|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
                               0, 0, 0, 0, NULL, NULL,
                               GetModuleHandle(NULL), NULL);
    DCHECK(parent != NULL);
    SetParent(handle, parent);

    WebViewHost* host = browser_->UIT_GetWebViewHost();
    if (host)
      host->AddWindowedPlugin(handle);
  }
}

void BrowserWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle handle) {
  if (browser_->IsWindowRenderingDisabled()) {
    WebViewHost* host = browser_->UIT_GetWebViewHost();
    if (host)
      host->RemoveWindowedPlugin(handle);

    // Destroy the hidden parent window.
    DestroyWindow(GetParent(handle));
  }
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

  if (browser_->IsWindowRenderingDisabled()) {
    WebViewHost* host = browser_->UIT_GetWebViewHost();
    if (host) {
      host->MoveWindowedPlugin(move);
    }
  }
}

static void AddMenuItem(CefRefPtr<CefBrowser> browser, HMENU menu, int index,
                        cef_handler_menuid_t id, const wchar_t* label,
                        bool enabled, std::list<std::wstring>& label_list)
{
  CefString actual_label(label);
  CefRefPtr<CefClient> client = browser->GetClient();
  if (client.get()) {
    CefRefPtr<CefMenuHandler> handler = client->GetMenuHandler();
    if(handler.get()) {
      // Let the handler change the label if desired
      handler->GetMenuLabel(browser, id, actual_label);
    }
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
  int screenX = -1, screenY = -1;
  
  POINT mouse_pt = {data.mousePosition.x, data.mousePosition.y};
  if (!browser_->IsWindowRenderingDisabled()) {
    // Perform the conversion to screen coordinates only if window rendering is
    // enabled.
    MapWindowPoints(browser_->UIT_GetWebViewWndHandle(), HWND_DESKTOP,
        &mouse_pt, 1);
    screenX = mouse_pt.x;
    screenY = mouse_pt.y;
  }

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

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefMenuHandler> handler;
  if (client.get())
    handler = client->GetMenuHandler();

  if (handler.get()) {
    // Gather menu information
    cef_handler_menuinfo_t menuInfo;
    memset(&menuInfo, 0, sizeof(menuInfo));

    CefString linkStr(std::string(data.linkURL.spec()));
    CefString imageStr(std::string(data.srcURL.spec()));
    CefString pageStr(std::string(data.pageURL.spec()));
    CefString frameStr(std::string(data.frameURL.spec()));
    CefString selectedTextStr(string16(data.selectedText));
    CefString misspelledWordStr(string16(data.misspelledWord));
    CefString securityInfoStr(std::string(data.securityInfo));

    menuInfo.typeFlags = type_flags;
    menuInfo.x = mouse_pt.x;
    menuInfo.y = mouse_pt.y;
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
    if (handler->OnBeforeMenu(browser_, menuInfo))
      goto end;
  }

  if (client.get() && browser_->IsWindowRenderingDisabled()) {
    CefRefPtr<CefRenderHandler> render_handler = client->GetRenderHandler();
    if (render_handler.get() &&
        !render_handler->GetScreenPoint(browser_, mouse_pt.x, mouse_pt.y,
                                        screenX, screenY)) {
      goto end;
    }
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

  if (menu) {
    // show the context menu
    int selected_id = TrackPopupMenu(menu,
        TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_RECURSE,
        screenX, screenY, 0, browser_->UIT_GetMainWndHandle(), NULL);

    if (selected_id != 0) {
      // An action was chosen
      cef_handler_menuid_t menuId =
          static_cast<cef_handler_menuid_t>(selected_id);
      bool handled = false;
      if (handler.get()) {
        // Ask the handler if it wants to handle the action
        handled = handler->OnMenuAction(browser_, menuId);
      }

      if(!handled) {
        // Execute the action
        browser_->UIT_HandleAction(menuId, browser_->GetFocusedFrame());
      }
    }
  }

  DestroyMenu(menu);

end:
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

// Private methods ------------------------------------------------------------

void BrowserWebViewDelegate::RegisterDragDrop() {
  DCHECK(!drop_target_);
  drop_target_ = new WebDropTarget(browser_->UIT_GetWebViewWndHandle(),
                                   browser_->UIT_GetWebView());
}

void BrowserWebViewDelegate::RevokeDragDrop() {
  if (drop_target_.get())
    ::RevokeDragDrop(browser_->UIT_GetWebViewWndHandle());
}

void BrowserWebViewDelegate::EndDragging() {
  browser_->UIT_GetWebView()->dragSourceSystemDragEnded();
  drag_delegate_ = NULL;
}

void BrowserWebViewDelegate::ShowJavaScriptAlert(WebFrame* webframe,
                                                 const CefString& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  std::wstring messageStr = message;
  std::wstring titleStr = browser_->UIT_GetTitle();
  MessageBox(browser_->UIT_GetMainWndHandle(), messageStr.c_str(),
      titleStr.c_str(), MB_OK | MB_ICONWARNING);
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(WebFrame* webframe,
                                                   const CefString& message)
{
  // TODO(cef): Think about what we should be showing as the prompt caption
  std::wstring messageStr = message;
  std::wstring titleStr = browser_->UIT_GetTitle();
  int rv = MessageBox(browser_->UIT_GetMainWndHandle(), messageStr.c_str(),
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
    result = RunOpenMultiFileDialog(L"", browser_->UIT_GetMainWndHandle(),
        &file_names);
  } else {
    FilePath file_name;
    result = RunOpenFileDialog(L"", browser_->UIT_GetMainWndHandle(),
        &file_name);
    if (result)
      file_names.push_back(file_name);
  }

  return result;
}

