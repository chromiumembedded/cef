// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wininet.h>
#include <winspool.h>

#include "libcef/browser/thread_util.h"

#include "base/i18n/case_conversion.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/file_chooser_params.h"
#include "grit/cef_strings.h"
#include "grit/ui_strings.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/hwnd_util.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

bool IsAeroGlassEnabled() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  BOOL enabled = FALSE;
  return SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled;
}

void SetAeroGlass(HWND hWnd) {
  if (!IsAeroGlassEnabled())
    return;

  // Make the whole window transparent.
  MARGINS mgMarInset = { -1, -1, -1, -1 };
  DwmExtendFrameIntoClientArea(hWnd, &mgMarInset);
}

void WriteTextToFile(const std::string& data, const std::wstring& file_path) {
  FILE* fp;
  errno_t err = _wfopen_s(&fp, file_path.c_str(), L"wt");
  if (err)
      return;
  fwrite(data.c_str(), 1, data.size(), fp);
  fclose(fp);
}

// From ui/base/dialogs/select_file_dialog_win.cc.

// Get the file type description from the registry. This will be "Text Document"
// for .txt files, "JPEG Image" for .jpg files, etc. If the registry doesn't
// have an entry for the file type, we return false, true if the description was
// found. 'file_ext' must be in form ".txt".
static bool GetRegistryDescriptionFromExtension(const std::wstring& file_ext,
                                                std::wstring* reg_description) {
  DCHECK(reg_description);
  base::win::RegKey reg_ext(HKEY_CLASSES_ROOT, file_ext.c_str(), KEY_READ);
  std::wstring reg_app;
  if (reg_ext.ReadValue(NULL, &reg_app) == ERROR_SUCCESS && !reg_app.empty()) {
    base::win::RegKey reg_link(HKEY_CLASSES_ROOT, reg_app.c_str(), KEY_READ);
    if (reg_link.ReadValue(NULL, reg_description) == ERROR_SUCCESS)
      return true;
  }
  return false;
}

// Set up a filter for a Save/Open dialog, which will consist of |file_ext| file
// extensions (internally separated by semicolons), |ext_desc| as the text
// descriptions of the |file_ext| types (optional), and (optionally) the default
// 'All Files' view. The purpose of the filter is to show only files of a
// particular type in a Windows Save/Open dialog box. The resulting filter is
// returned. The filters created here are:
//   1. only files that have 'file_ext' as their extension
//   2. all files (only added if 'include_all_files' is true)
// Example:
//   file_ext: { "*.txt", "*.htm;*.html" }
//   ext_desc: { "Text Document" }
//   returned: "Text Document\0*.txt\0HTML Document\0*.htm;*.html\0"
//             "All Files\0*.*\0\0" (in one big string)
// If a description is not provided for a file extension, it will be retrieved
// from the registry. If the file extension does not exist in the registry, it
// will be omitted from the filter, as it is likely a bogus extension.
std::wstring FormatFilterForExtensions(
    const std::vector<std::wstring>& file_ext,
    const std::vector<std::wstring>& ext_desc,
    bool include_all_files) {
  const std::wstring all_ext = L"*.*";
  const std::wstring all_desc =
      l10n_util::GetStringUTF16(IDS_APP_SAVEAS_ALL_FILES) +
      L" (" + all_ext + L")";

  DCHECK(file_ext.size() >= ext_desc.size());

  if (file_ext.empty())
    include_all_files = true;

  std::wstring result;

  for (size_t i = 0; i < file_ext.size(); ++i) {
    std::wstring ext = file_ext[i];
    std::wstring desc;
    if (i < ext_desc.size())
      desc = ext_desc[i];

    if (ext.empty()) {
      // Force something reasonable to appear in the dialog box if there is no
      // extension provided.
      include_all_files = true;
      continue;
    }

    if (desc.empty()) {
      DCHECK(ext.find(L'.') != std::wstring::npos);
      std::wstring first_extension = ext.substr(ext.find(L'.'));
      size_t first_separator_index = first_extension.find(L';');
      if (first_separator_index != std::wstring::npos)
        first_extension = first_extension.substr(0, first_separator_index);

      // Find the extension name without the preceeding '.' character.
      std::wstring ext_name = first_extension;
      size_t ext_index = ext_name.find_first_not_of(L'.');
      if (ext_index != std::wstring::npos)
        ext_name = ext_name.substr(ext_index);

      if (!GetRegistryDescriptionFromExtension(first_extension, &desc)) {
        // The extension doesn't exist in the registry. Create a description
        // based on the unknown extension type (i.e. if the extension is .qqq,
        // the we create a description "QQQ File (.qqq)").
        include_all_files = true;
        desc = l10n_util::GetStringFUTF16(
            IDS_APP_SAVEAS_EXTENSION_FORMAT,
            base::i18n::ToUpper(WideToUTF16(ext_name)),
            ext_name);
      }
    }

    if (!desc.empty())
      desc += L" (" + ext + L")";
    else
      desc = ext;

    result.append(desc.c_str(), desc.size() + 1);  // Append NULL too.
    result.append(ext.c_str(), ext.size() + 1);
  }

  if (include_all_files) {
    result.append(all_desc.c_str(), all_desc.size() + 1);
    result.append(all_ext.c_str(), all_ext.size() + 1);
  }

  result.append(1, '\0');  // Double NULL required.
  return result;
}

std::wstring GetDescriptionFromMimeType(const std::string& mime_type) {
  // Check for wild card mime types and return an appropriate description.
  static const struct {
    const char* mime_type;
    int string_id;
  } kWildCardMimeTypes[] = {
    { "audio", IDS_APP_AUDIO_FILES },
    { "image", IDS_APP_IMAGE_FILES },
    { "text", IDS_APP_TEXT_FILES },
    { "video", IDS_APP_VIDEO_FILES },
  };

  for (size_t i = 0; i < arraysize(kWildCardMimeTypes); ++i) {
    if (mime_type == std::string(kWildCardMimeTypes[i].mime_type) + "/*")
      return l10n_util::GetStringUTF16(kWildCardMimeTypes[i].string_id);
  }

  return std::wstring();
}

std::wstring GetFilterStringFromAcceptTypes(
    const std::vector<string16>& accept_types) {
  std::vector<std::wstring> extensions;
  std::vector<std::wstring> descriptions;

  for (size_t i = 0; i < accept_types.size(); ++i) {
    std::string ascii_type = UTF16ToASCII(accept_types[i]);
    if (ascii_type.length()) {
      // Just treat as extension if contains '.' as the first character.
      if (ascii_type[0] == '.') {
        extensions.push_back(L"*" + ASCIIToWide(ascii_type));
        descriptions.push_back(std::wstring());
      } else {
        // Otherwise convert mime type to one or more extensions.
        std::vector<FilePath::StringType> ext;
        std::wstring ext_str;
        net::GetExtensionsForMimeType(ascii_type, &ext);
        if (ext.size() > 0) {
          for (size_t x = 0; x < ext.size(); ++x) {
            if (x != 0)
              ext_str += L";";
            ext_str += L"*." + ext[x];
          }
          extensions.push_back(ext_str);
          descriptions.push_back(GetDescriptionFromMimeType(ascii_type));
        }
      }
    }
  }

  return FormatFilterForExtensions(extensions, descriptions, true);
}

// from chrome/browser/views/shell_dialogs_win.cc

bool RunOpenFileDialog(const content::FileChooserParams& params,
                       HWND owner,
                       FilePath* path) {
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  // Consider default file name if any.
  FilePath default_file_name(params.default_file_name);

  wchar_t filename[MAX_PATH] = {0};

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  std::wstring directory;
  if (!default_file_name.empty()) {
    base::wcslcpy(filename, default_file_name.value().c_str(),
        arraysize(filename));

    directory = default_file_name.DirName().value();
    ofn.lpstrInitialDir = directory.c_str();
  }

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_OPEN_FILE_DIALOG_TITLE);
  if (!title.empty())
    ofn.lpstrTitle = title.c_str();

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER |
              OFN_ENABLESIZING;

  std::wstring filter = GetFilterStringFromAcceptTypes(params.accept_types);
  if (!filter.empty())
    ofn.lpstrFilter = filter.c_str();

  bool success = !!GetOpenFileName(&ofn);
  if (success)
    *path = FilePath(filename);
  return success;
}

bool RunOpenMultiFileDialog(const content::FileChooserParams& params,
                            HWND owner,
                            std::vector<FilePath>* paths) {
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  scoped_array<wchar_t> filename(new wchar_t[UNICODE_STRING_MAX_CHARS]);
  filename[0] = 0;

  ofn.lpstrFile = filename.get();
  ofn.nMaxFile = UNICODE_STRING_MAX_CHARS;

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_OPEN_FILES_DIALOG_TITLE);
  if (!title.empty())
    ofn.lpstrTitle = title.c_str();

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER |
              OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING;

  std::wstring filter = GetFilterStringFromAcceptTypes(params.accept_types);
  if (!filter.empty())
    ofn.lpstrFilter = filter.c_str();

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

bool RunSaveFileDialog(const content::FileChooserParams& params,
                       HWND owner,
                       FilePath* path) {
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  // Consider default file name if any.
  FilePath default_file_name(params.default_file_name);

  wchar_t filename[MAX_PATH] = {0};

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  std::wstring directory;
  if (!default_file_name.empty()) {
    base::wcslcpy(filename, default_file_name.value().c_str(),
        arraysize(filename));

    directory = default_file_name.DirName().value();
    ofn.lpstrInitialDir = directory.c_str();
  }

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_SAVE_AS_DIALOG_TITLE);
  if (!title.empty())
    ofn.lpstrTitle = title.c_str();

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
              OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

  std::wstring filter = GetFilterStringFromAcceptTypes(params.accept_types);
  if (!filter.empty())
    ofn.lpstrFilter = filter.c_str();

  bool success = !!GetSaveFileName(&ofn);
  if (success)
    *path = FilePath(filename);
  return success;
}


// According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
// "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
// ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
// support URLS of 2083 chars in length, 2K is safe."
const int kMaxAddressLengthChars = 2048;

bool HasExternalHandler(const std::string& scheme) {
  base::win::RegKey key;
  const std::wstring registry_path =
      ASCIIToWide(scheme + "\\shell\\open\\command");
  key.Open(HKEY_CLASSES_ROOT, registry_path.c_str(), KEY_READ);
  if (key.Valid()) {
    DWORD size = 0;
    key.ReadValue(NULL, NULL, &size, NULL);
    if (size > 2) {
       // ShellExecute crashes the process when the command is empty.
       // We check for "2" because it always returns the trailing NULL.
       return true;
    }
  }

  return false;
}

WORD KeyStatesToWord() {
  static const USHORT kHighBitMaskShort = 0x8000;
  WORD result = 0;

  if (GetKeyState(VK_CONTROL) & kHighBitMaskShort)
    result |= MK_CONTROL;
  if (GetKeyState(VK_SHIFT) & kHighBitMaskShort)
    result |= MK_SHIFT;
  if (GetKeyState(VK_LBUTTON) & kHighBitMaskShort)
    result |= MK_LBUTTON;
  if (GetKeyState(VK_MBUTTON) & kHighBitMaskShort)
    result |= MK_MBUTTON;
  if (GetKeyState(VK_RBUTTON) & kHighBitMaskShort)
    result |= MK_RBUTTON;
  return result;
}

}  // namespace

// static
void CefBrowserHostImpl::RegisterWindowClass() {
  // Register the window class
  WNDCLASSEX wcex = {
    /* cbSize = */ sizeof(WNDCLASSEX),
    /* style = */ CS_HREDRAW | CS_VREDRAW,
    /* lpfnWndProc = */ CefBrowserHostImpl::WndProc,
    /* cbClsExtra = */ 0,
    /* cbWndExtra = */ 0,
    /* hInstance = */ ::GetModuleHandle(NULL),
    /* hIcon = */ NULL,
    /* hCursor = */ LoadCursor(NULL, IDC_ARROW),
    /* hbrBackground = */ 0,
    /* lpszMenuName = */ NULL,
    /* lpszClassName = */ CefBrowserHostImpl::GetWndClass(),
    /* hIconSm = */ NULL,
  };
  RegisterClassEx(&wcex);
}

// static
LPCTSTR CefBrowserHostImpl::GetWndClass() {
  return L"CefBrowserWindow";
}

// static
LRESULT CALLBACK CefBrowserHostImpl::WndProc(HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam) {
  CefBrowserHostImpl* browser =
      static_cast<CefBrowserHostImpl*>(ui::GetWindowUserData(hwnd));

  switch (message) {
  case WM_CLOSE:
    if (browser) {
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
      browser->DestroyBrowser();

      // Release the reference added in PlatformCreateWindow().
      browser->Release();
    }
    return 0;

  case WM_SIZE:
    // Minimizing resizes the window to 0x0 which causes our layout to go all
    // screwy, so we just ignore it.
    if (wParam != SIZE_MINIMIZED && browser) {
      // resize the web view window to the full size of the browser window
      RECT rc;
      GetClientRect(hwnd, &rc);
      MoveWindow(browser->GetContentView(), 0, 0, rc.right, rc.bottom,
          TRUE);
    }
    return 0;

  case WM_SETFOCUS:
    if (browser)
      browser->OnSetFocus(FOCUS_SOURCE_SYSTEM);
    return 0;

  case WM_ERASEBKGND:
    return 0;

  case WM_DWMCOMPOSITIONCHANGED:
    // Message sent to top-level windows when composition has been enabled or
    // disabled.
    if (browser && browser->window_info_.transparent_painting)
      SetAeroGlass(hwnd);
    break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

bool CefBrowserHostImpl::PlatformCreateWindow() {
  std::wstring windowName(CefString(&window_info_.window_name));

  // Create the new browser window.
  window_info_.window = CreateWindowEx(window_info_.ex_style,
      GetWndClass(), windowName.c_str(), window_info_.style,
      window_info_.x, window_info_.y, window_info_.width,
      window_info_.height, window_info_.parent_window, window_info_.menu,
      ::GetModuleHandle(NULL), NULL);

  // It's possible for CreateWindowEx to fail if the parent window was
  // destroyed between the call to CreateBrowser and the above one.
  DCHECK(window_info_.window != NULL);
  if (!window_info_.window)
    return false;

  if (window_info_.transparent_painting &&
      !(window_info_.style & WS_CHILD)) {
    // Transparent top-level windows will be given "sheet of glass" effect.
    SetAeroGlass(window_info_.window);
  }

  // Set window user data to this object for future reference from the window
  // procedure.
  ui::SetWindowUserData(window_info_.window, this);

  // Add a reference that will be released in the WM_DESTROY handler.
  AddRef();

  // Parent the TabContents to the browser window.
  SetParent(web_contents_->GetView()->GetNativeView(), window_info_.window);

  // Size the web view window to the browser window.
  RECT cr;
  GetClientRect(window_info_.window, &cr);

  // Respect the WS_VISIBLE window style when setting the window's position.
  UINT flags = SWP_NOZORDER | SWP_SHOWWINDOW;
  if (!(window_info_.style & WS_VISIBLE))
    flags |= SWP_NOACTIVATE;

  SetWindowPos(GetContentView(), NULL, cr.left, cr.top, cr.right,
                cr.bottom, flags);

  return true;
}

void CefBrowserHostImpl::PlatformCloseWindow() {
  if (window_info_.window != NULL)
    PostMessage(window_info_.window, WM_CLOSE, 0, 0);
}

void CefBrowserHostImpl::PlatformSizeTo(int width, int height) {
  RECT rect = {0, 0, width, height};
  DWORD style = GetWindowLong(window_info_.window, GWL_STYLE);
  DWORD ex_style = GetWindowLong(window_info_.window, GWL_EXSTYLE);
  bool has_menu = !(style & WS_CHILD) && (GetMenu(window_info_.window) != NULL);

  // The size value is for the client area. Calculate the whole window size
  // based on the current style.
  AdjustWindowRectEx(&rect, style, has_menu, ex_style);

  // Size the window.
  SetWindowPos(window_info_.window, NULL, 0, 0, rect.right,
               rect.bottom, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return IsWindowRenderingDisabled() ?
      window_info_.parent_window :
      window_info_.window;
}

bool CefBrowserHostImpl::PlatformViewText(const std::string& text) {
  CEF_REQUIRE_UIT();

  DWORD dwRetVal;
  DWORD dwBufSize = 512;
  TCHAR lpPathBuffer[512];
  UINT uRetVal;
  TCHAR szTempName[512];

  dwRetVal = GetTempPath(dwBufSize,      // length of the buffer
                         lpPathBuffer);  // buffer for path
  if (dwRetVal > dwBufSize || (dwRetVal == 0))
    return false;

  // Create a temporary file.
  uRetVal = GetTempFileName(lpPathBuffer,  // directory for tmp files
                            TEXT("src"),   // temp file name prefix
                            0,             // create unique name
                            szTempName);   // buffer for name
  if (uRetVal == 0)
    return false;

  size_t len = wcslen(szTempName);
  wcscpy(szTempName + len - 3, L"txt");
  WriteTextToFile(text, szTempName);

  HWND frameWnd = GetAncestor(PlatformGetWindowHandle(), GA_ROOT);
  int errorCode = reinterpret_cast<int>(ShellExecute(frameWnd, L"open",
      szTempName, NULL, NULL, SW_SHOWNORMAL));
  if (errorCode <= 32)
    return false;

  return true;
}

void CefBrowserHostImpl::PlatformHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Any unhandled keyboard/character messages are sent to DefWindowProc so that
  // shortcut keys work correctly.
  DefWindowProc(event.os_event.hwnd, event.os_event.message,
                event.os_event.wParam, event.os_event.lParam);
}

void CefBrowserHostImpl::PlatformRunFileChooser(
    const content::FileChooserParams& params,
    RunFileChooserCallback callback) {
  std::vector<FilePath> files;

  if (params.mode == content::FileChooserParams::Open) {
    FilePath file;
    if (RunOpenFileDialog(params, PlatformGetWindowHandle(), &file))
      files.push_back(file);
  } else if (params.mode == content::FileChooserParams::OpenMultiple) {
    RunOpenMultiFileDialog(params, PlatformGetWindowHandle(), &files);
  } else if (params.mode == content::FileChooserParams::Save) {
    FilePath file;
    if (RunSaveFileDialog(params, PlatformGetWindowHandle(), &file))
      files.push_back(file);
  } else {
    NOTIMPLEMENTED();
  }

  callback.Run(files);
}

void CefBrowserHostImpl::PlatformHandleExternalProtocol(const GURL& url) {
  if (CEF_CURRENTLY_ON_FILET()) {
    if (!HasExternalHandler(url.scheme()))
      return;

    const std::string& address = url.spec();
    if (address.length() > kMaxAddressLengthChars)
      return;

    ShellExecuteA(NULL, "open", address.c_str(), NULL, NULL, SW_SHOWNORMAL);
  } else {
    // Execute on the FILE thread.
    CEF_POST_TASK(CEF_FILET,
        base::Bind(&CefBrowserHostImpl::PlatformHandleExternalProtocol, this,
                   url));
  }
}

// static
bool CefBrowserHostImpl::IsWindowRenderingDisabled(const CefWindowInfo& info) {
  return info.window_rendering_disabled ? true : false;
}

// static
bool CefBrowserHostImpl::PlatformTranslateKeyEvent(
    gfx::NativeEvent& native_event, const CefKeyEvent& key_event) {
  UINT message = 0;
  WPARAM wparam = key_event.windows_key_code;
  LPARAM lparam = key_event.native_key_code;

  switch (key_event.type) {
    case KEYEVENT_KEYUP:
      message = key_event.is_system_key ? WM_SYSKEYUP : WM_KEYUP;
      break;
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      message = key_event.is_system_key ? WM_SYSKEYDOWN : WM_KEYDOWN;
      break;
    case KEYEVENT_CHAR:
      message = key_event.is_system_key ? WM_SYSCHAR : WM_CHAR;
      break;
    default:
      return false;
  }

  gfx::NativeEvent ev = {NULL, message, wparam, lparam};
  native_event = ev;
  return true;
}

// static
bool CefBrowserHostImpl::PlatformTranslateClickEvent(
    WebKit::WebMouseEvent& ev,
    int x, int y, MouseButtonType type,
    bool mouseUp, int clickCount) {
  DCHECK(clickCount >=1 && clickCount <= 2);

  UINT message = 0;
  WPARAM wparam = KeyStatesToWord();
  LPARAM lparam = MAKELPARAM(x, y);

  if (type == MBT_LEFT) {
    if (mouseUp)
      message = (clickCount == 1 ? WM_LBUTTONUP : WM_LBUTTONDBLCLK);
    else
      message = WM_LBUTTONDOWN;
  } else if (type == MBT_MIDDLE) {
    if (mouseUp)
      message = (clickCount == 1 ? WM_MBUTTONUP : WM_MBUTTONDBLCLK);
    else
      message = WM_MBUTTONDOWN;
  }  else if (type == MBT_RIGHT) {
    if (mouseUp)
      message = (clickCount == 1 ? WM_RBUTTONUP : WM_RBUTTONDBLCLK);
    else
      message = WM_RBUTTONDOWN;
  }

  if (message == 0) {
    NOTREACHED();
    return false;
  }

  ev = WebKit::WebInputEventFactory::mouseEvent(NULL, message, wparam, lparam);
  return true;
}

// static
bool CefBrowserHostImpl::PlatformTranslateMoveEvent(
    WebKit::WebMouseEvent& ev,
    int x, int y, bool mouseLeave) {
  UINT message;
  WPARAM wparam = KeyStatesToWord();
  LPARAM lparam = 0;

  if (mouseLeave) {
    message = WM_MOUSELEAVE;
  } else {
    message = WM_MOUSEMOVE;
    lparam = MAKELPARAM(x, y);
  }

  ev = WebKit::WebInputEventFactory::mouseEvent(NULL, message, wparam, lparam);
  return true;
}

// static
bool CefBrowserHostImpl::PlatformTranslateWheelEvent(
    WebKit::WebMouseWheelEvent& ev,
    int x, int y, int deltaX, int deltaY) {
  WPARAM wparam = MAKEWPARAM(KeyStatesToWord(), deltaY);
  LPARAM lparam = MAKELPARAM(x, y);

  ev = WebKit::WebInputEventFactory::mouseWheelEvent(NULL, WM_MOUSEWHEEL,
                                                     wparam, lparam);
  return true;
}
