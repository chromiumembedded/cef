// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native_win.h"

#include <shellapi.h>
#include <wininet.h>
#include <winspool.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/native/file_dialog_runner_win.h"
#include "libcef/browser/native/javascript_dialog_runner_win.h"
#include "libcef/browser/native/menu_runner_win.h"
#include "libcef/browser/native/window_delegate_view.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "ui/aura/window.h"
#include "ui/base/win/shell.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/platform_key_map_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

void WriteTempFileAndView(scoped_refptr<base::RefCountedString> str) {
  CEF_REQUIRE_BLOCKING();

  base::FilePath tmp_file;
  if (!base::CreateTemporaryFile(&tmp_file))
    return;

  // The shell command will look at the file extension to identify the correct
  // program to open.
  tmp_file = tmp_file.AddExtension(L"txt");

  const std::string& data = str->data();
  int write_ct = base::WriteFile(tmp_file, data.c_str(), data.size());
  DCHECK_EQ(static_cast<int>(data.size()), write_ct);

  ui::win::OpenFileViaShell(tmp_file);
}

// According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
// "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
// ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
// support URLS of 2083 chars in length, 2K is safe."
const int kMaxAddressLengthChars = 2048;

bool HasExternalHandler(const std::string& scheme) {
  base::win::RegKey key;
  const std::wstring registry_path =
      base::ASCIIToUTF16(scheme + "\\shell\\open\\command");
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

void ExecuteExternalProtocol(const GURL& url) {
  CEF_REQUIRE_BLOCKING();

  if (!HasExternalHandler(url.scheme()))
    return;

  const std::string& address = url.spec();
  if (address.length() > kMaxAddressLengthChars)
    return;

  ShellExecuteA(NULL, "open", address.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// DPI value for 1x scale factor.
#define DPI_1X 96.0f

float GetWindowScaleFactor(HWND hwnd) {
  DCHECK(hwnd);

  if (base::win::IsProcessPerMonitorDpiAware()) {
    // Let Windows tell us the correct DPI.
    static auto get_dpi_for_window_func = []() {
      return reinterpret_cast<decltype(::GetDpiForWindow)*>(
          GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForWindow"));
    }();
    if (get_dpi_for_window_func)
      return static_cast<float>(get_dpi_for_window_func(hwnd)) / DPI_1X;
  }

  // Fallback to the monitor that contains the window center point.
  RECT cr;
  GetWindowRect(hwnd, &cr);
  return display::Screen::GetScreen()
      ->GetDisplayNearestPoint(
          gfx::Point((cr.right - cr.left) / 2, (cr.bottom - cr.top) / 2))
      .device_scale_factor();
}

}  // namespace

CefBrowserPlatformDelegateNativeWin::CefBrowserPlatformDelegateNativeWin(
    const CefWindowInfo& window_info,
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame)
    : CefBrowserPlatformDelegateNative(window_info,
                                       background_color,
                                       use_shared_texture,
                                       use_external_begin_frame),
      host_window_created_(false),
      window_widget_(nullptr) {}

void CefBrowserPlatformDelegateNativeWin::BrowserDestroyed(
    CefBrowserHostImpl* browser) {
  CefBrowserPlatformDelegate::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeWin::CreateHostWindow() {
  RegisterWindowClass();

  has_frame_ = !(window_info_.style & WS_CHILD);

  std::wstring windowName(CefString(&window_info_.window_name));

  // Create the new browser window.
  CreateWindowEx(window_info_.ex_style, GetWndClass(), windowName.c_str(),
                 window_info_.style, window_info_.x, window_info_.y,
                 window_info_.width, window_info_.height,
                 window_info_.parent_window, window_info_.menu,
                 ::GetModuleHandle(NULL), this);

  // It's possible for CreateWindowEx to fail if the parent window was
  // destroyed between the call to CreateBrowser and the above one.
  DCHECK(window_info_.window);
  if (!window_info_.window)
    return false;

  host_window_created_ = true;

  // Add a reference that will later be released in DestroyBrowser().
  browser_->AddRef();

  if (!called_enable_non_client_dpi_scaling_ && has_frame_ &&
      base::win::IsProcessPerMonitorDpiAware()) {
    // This call gets Windows to scale the non-client area when WM_DPICHANGED
    // is fired on Windows versions < 10.0.14393.0.
    // Derived signature; not available in headers.
    static auto enable_child_window_dpi_message_func = []() {
      using EnableChildWindowDpiMessagePtr = LRESULT(WINAPI*)(HWND, BOOL);
      return reinterpret_cast<EnableChildWindowDpiMessagePtr>(GetProcAddress(
          GetModuleHandle(L"user32.dll"), "EnableChildWindowDpiMessage"));
    }();
    if (enable_child_window_dpi_message_func)
      enable_child_window_dpi_message_func(window_info_.window, TRUE);
  }

  DCHECK(!window_widget_);

  // Convert from device coordinates to logical coordinates.
  RECT cr;
  GetClientRect(window_info_.window, &cr);
  gfx::Point point = gfx::Point(cr.right, cr.bottom);
  const float scale = GetWindowScaleFactor(window_info_.window);
  point =
      gfx::ToFlooredPoint(gfx::ScalePoint(gfx::PointF(point), 1.0f / scale));

  // Stay on top if top-most window hosting the web view is topmost.
  HWND top_level_window = GetAncestor(window_info_.window, GA_ROOT);
  DWORD top_level_window_ex_styles =
      GetWindowLongPtr(top_level_window, GWL_EXSTYLE);
  bool always_on_top =
      (top_level_window_ex_styles & WS_EX_TOPMOST) == WS_EX_TOPMOST;

  CefWindowDelegateView* delegate_view = new CefWindowDelegateView(
      GetBackgroundColor(), always_on_top, GetBoundsChangedCallback());
  delegate_view->Init(window_info_.window, browser_->web_contents(),
                      gfx::Rect(0, 0, point.x(), point.y()));

  window_widget_ = delegate_view->GetWidget();

  const HWND widget_hwnd = HWNDForWidget(window_widget_);
  DCHECK(widget_hwnd);
  const DWORD widget_ex_styles = GetWindowLongPtr(widget_hwnd, GWL_EXSTYLE);

  if (window_info_.ex_style & WS_EX_NOACTIVATE) {
    // Add the WS_EX_NOACTIVATE style on the DesktopWindowTreeHostWin HWND
    // so that HWNDMessageHandler::Show() called via Widget::Show() does not
    // activate the window.
    SetWindowLongPtr(widget_hwnd, GWL_EXSTYLE,
                     widget_ex_styles | WS_EX_NOACTIVATE);
  }

  window_widget_->Show();

  if (window_info_.ex_style & WS_EX_NOACTIVATE) {
    // Remove the WS_EX_NOACTIVATE style so that future mouse clicks inside the
    // browser correctly activate and focus the window.
    SetWindowLongPtr(widget_hwnd, GWL_EXSTYLE, widget_ex_styles);
  }

  return true;
}

void CefBrowserPlatformDelegateNativeWin::CloseHostWindow() {
  if (window_info_.window != NULL) {
    HWND frameWnd = GetAncestor(window_info_.window, GA_ROOT);
    PostMessage(frameWnd, WM_CLOSE, 0, 0);
  }
}

CefWindowHandle CefBrowserPlatformDelegateNativeWin::GetHostWindowHandle()
    const {
  if (windowless_handler_)
    return windowless_handler_->GetParentWindowHandle();
  return window_info_.window;
}

views::Widget* CefBrowserPlatformDelegateNativeWin::GetWindowWidget() const {
  return window_widget_;
}

void CefBrowserPlatformDelegateNativeWin::SendFocusEvent(bool setFocus) {
  if (!setFocus)
    return;

  if (browser_->web_contents()) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus.
    browser_->web_contents()->Focus();
  }

  if (window_widget_) {
    // Give native focus to the DesktopWindowTreeHostWin associated with the
    // root window.
    //
    // The DesktopWindowTreeHostWin HandleNativeFocus/HandleNativeBlur methods
    // are called in response to WM_SETFOCUS/WM_KILLFOCUS respectively. The
    // implementation has been patched to call HandleActivationChanged which
    // results in the following behaviors:
    // 1. Update focus/activation state of the aura::Window indirectly via
    //    wm::FocusController. This allows focus-related behaviors (e.g. focus
    //    rings, flashing caret, onFocus/onBlur JS events, etc.) to work as
    //    expected (see issue #1677).
    // 2. Update focus state of the ui::InputMethod. If this does not occur
    //    then InputMethodBase::GetTextInputClient will return NULL and
    //    InputMethodWin::OnChar will fail to sent character events to the
    //    renderer (see issue #1700).
    //
    // This differs from activation in Chrome which is handled via
    // HWNDMessageHandler::PostProcessActivateMessage (Widget::Show indirectly
    // calls HWNDMessageHandler::Activate which calls ::SetForegroundWindow
    // resulting in a WM_ACTIVATE message being sent to the window). The Chrome
    // code path doesn't work for CEF because IsTopLevelWindow in
    // hwnd_message_handler.cc will return false and consequently
    // HWNDMessageHandler::PostProcessActivateMessage will not be called.
    //
    // Activation events are usually reserved for the top-level window so
    // triggering activation based on focus events may be incorrect in some
    // circumstances. Revisit this implementation if additional problems are
    // discovered.
    ::SetFocus(HWNDForWidget(window_widget_));
  }
}

void CefBrowserPlatformDelegateNativeWin::NotifyMoveOrResizeStarted() {
  // Call the parent method to dismiss any existing popups.
  CefBrowserPlatformDelegate::NotifyMoveOrResizeStarted();

  if (!window_widget_)
    return;

  // Notify DesktopWindowTreeHostWin of move events so that screen rectangle
  // information is communicated to the renderer process and popups are
  // displayed in the correct location.
  views::DesktopWindowTreeHostWin* tree_host =
      static_cast<views::DesktopWindowTreeHostWin*>(
          aura::WindowTreeHost::GetForAcceleratedWidget(
              HWNDForWidget(window_widget_)));
  DCHECK(tree_host);
  if (tree_host) {
    // Cast to HWNDMessageHandlerDelegate so we can access HandleMove().
    static_cast<views::HWNDMessageHandlerDelegate*>(tree_host)->HandleMove();
  }
}

void CefBrowserPlatformDelegateNativeWin::SizeTo(int width, int height) {
  HWND window = window_info_.window;

  RECT rect = {0, 0, width, height};
  DWORD style = GetWindowLong(window, GWL_STYLE);
  DWORD ex_style = GetWindowLong(window, GWL_EXSTYLE);
  bool has_menu = !(style & WS_CHILD) && (GetMenu(window) != NULL);

  // The size value is for the client area. Calculate the whole window size
  // based on the current style.
  AdjustWindowRectEx(&rect, style, has_menu, ex_style);

  // Size the window. The left/top values may be negative.
  SetWindowPos(window, NULL, 0, 0, rect.right - rect.left,
               rect.bottom - rect.top,
               SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

gfx::Point CefBrowserPlatformDelegateNativeWin::GetScreenPoint(
    const gfx::Point& view) const {
  if (windowless_handler_)
    return windowless_handler_->GetParentScreenPoint(view);

  if (!window_info_.window)
    return view;

  // Convert from logical coordinates to device coordinates.
  const float scale = GetWindowScaleFactor(window_info_.window);
  const gfx::Point& device_pt =
      gfx::ToFlooredPoint(gfx::ScalePoint(gfx::PointF(view), scale));

  // Convert from client coordinates to screen coordinates.
  POINT screen_pt = {device_pt.x(), device_pt.y()};
  ClientToScreen(window_info_.window, &screen_pt);

  return gfx::Point(screen_pt.x, screen_pt.y);
}

void CefBrowserPlatformDelegateNativeWin::ViewText(const std::string& text) {
  std::string str = text;
  scoped_refptr<base::RefCountedString> str_ref =
      base::RefCountedString::TakeString(&str);
  CEF_POST_USER_VISIBLE_TASK(base::Bind(WriteTempFileAndView, str_ref));
}

bool CefBrowserPlatformDelegateNativeWin::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Any unhandled keyboard/character messages are sent to DefWindowProc so that
  // shortcut keys work correctly.
  if (event.os_event) {
    const MSG& msg = event.os_event->native_event();
    return !DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
  } else {
    MSG msg = {};

    msg.hwnd = GetHostWindowHandle();
    if (!msg.hwnd)
      return false;

    switch (event.GetType()) {
      case blink::WebInputEvent::kRawKeyDown:
        msg.message = event.is_system_key ? WM_SYSKEYDOWN : WM_KEYDOWN;
        break;
      case blink::WebInputEvent::kKeyUp:
        msg.message = event.is_system_key ? WM_SYSKEYUP : WM_KEYUP;
        break;
      case blink::WebInputEvent::kChar:
        msg.message = event.is_system_key ? WM_SYSCHAR : WM_CHAR;
        break;
      default:
        NOTREACHED();
        return false;
    }

    msg.wParam = event.windows_key_code;

    UINT scan_code = ::MapVirtualKeyW(event.windows_key_code, MAPVK_VK_TO_VSC);
    msg.lParam = (scan_code << 16) |  // key scan code
                 1;                   // key repeat count
    if (event.GetModifiers() & content::NativeWebKeyboardEvent::kAltKey)
      msg.lParam |= (1 << 29);

    return !DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
  }
}

// static
void CefBrowserPlatformDelegate::HandleExternalProtocol(const GURL& url) {
  CEF_POST_USER_VISIBLE_TASK(base::Bind(ExecuteExternalProtocol, url));
}

void CefBrowserPlatformDelegateNativeWin::TranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) const {
  result.windows_key_code = key_event.windows_key_code;
  result.native_key_code = key_event.native_key_code;
  result.is_system_key = key_event.is_system_key ? 1 : 0;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      result.SetType(blink::WebInputEvent::kRawKeyDown);
      break;
    case KEYEVENT_KEYUP:
      result.SetType(blink::WebInputEvent::kKeyUp);
      break;
    case KEYEVENT_CHAR:
      result.SetType(blink::WebInputEvent::kChar);
      break;
    default:
      NOTREACHED();
  }

  // Populate DOM values that will be passed to JavaScript handlers via
  // KeyboardEvent.
  result.dom_code = static_cast<int>(
      ui::KeycodeConverter::NativeKeycodeToDomCode(key_event.native_key_code));
  if (result.GetType() == blink::WebInputEvent::kChar) {
    result.dom_key = ui::DomKey::FromCharacter(key_event.windows_key_code);
  } else {
    // TODO(cef): CefKeyEvent does not currently pass extended key status (see
    // WM_KEYDOWN docs) which would be necessary to pass EF_IS_EXTENDED_KEY as
    // the |flags| parameter to DomKeyFromKeyboardCode().
    int flags = 0;
    result.dom_key = ui::PlatformKeyMap::DomKeyFromKeyboardCode(
        ui::KeyboardCodeForWindowsKeyCode(key_event.windows_key_code), &flags);
  }

  if (result.GetType() == blink::WebInputEvent::kChar ||
      result.GetType() == blink::WebInputEvent::kRawKeyDown) {
    result.text[0] = result.windows_key_code;
    result.unmodified_text[0] = result.windows_key_code;
  }

  result.SetModifiers(result.GetModifiers() |
                      TranslateModifiers(key_event.modifiers));
}

void CefBrowserPlatformDelegateNativeWin::TranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) const {
  TranslateMouseEvent(result, mouse_event);

  switch (type) {
    case MBT_LEFT:
      result.SetType(mouseUp ? blink::WebInputEvent::kMouseUp
                             : blink::WebInputEvent::kMouseDown);
      result.button = blink::WebMouseEvent::Button::kLeft;
      break;
    case MBT_MIDDLE:
      result.SetType(mouseUp ? blink::WebInputEvent::kMouseUp
                             : blink::WebInputEvent::kMouseDown);
      result.button = blink::WebMouseEvent::Button::kMiddle;
      break;
    case MBT_RIGHT:
      result.SetType(mouseUp ? blink::WebInputEvent::kMouseUp
                             : blink::WebInputEvent::kMouseDown);
      result.button = blink::WebMouseEvent::Button::kRight;
      break;
    default:
      NOTREACHED();
  }

  result.click_count = clickCount;
}

void CefBrowserPlatformDelegateNativeWin::TranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  TranslateMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.SetType(blink::WebInputEvent::kMouseMove);
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::Button::kLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::Button::kMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::Button::kRight;
    else
      result.button = blink::WebMouseEvent::Button::kNoButton;
  } else {
    result.SetType(blink::WebInputEvent::kMouseLeave);
    result.button = blink::WebMouseEvent::Button::kNoButton;
  }

  result.click_count = 0;
}

void CefBrowserPlatformDelegateNativeWin::TranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX,
    int deltaY) const {
  TranslateMouseEvent(result, mouse_event);

  result.SetType(blink::WebInputEvent::kMouseWheel);
  result.button = blink::WebMouseEvent::Button::kNoButton;

  float wheelDelta;
  bool horizontalScroll = false;

  wheelDelta = static_cast<float>(deltaY ? deltaY : deltaX);

  horizontalScroll = (deltaY == 0);

  static const ULONG defaultScrollCharsPerWheelDelta = 1;
  static const FLOAT scrollbarPixelsPerLine = 100.0f / 3.0f;
  static const ULONG defaultScrollLinesPerWheelDelta = 3;
  wheelDelta /= WHEEL_DELTA;
  float scrollDelta = wheelDelta;
  if (horizontalScroll) {
    ULONG scrollChars = defaultScrollCharsPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0);
    scrollDelta *= static_cast<FLOAT>(scrollChars) * scrollbarPixelsPerLine;
  } else {
    ULONG scrollLines = defaultScrollLinesPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
    if (scrollLines == WHEEL_PAGESCROLL)
      result.scroll_by_page = true;
    if (!result.scroll_by_page)
      scrollDelta *= static_cast<FLOAT>(scrollLines) * scrollbarPixelsPerLine;
  }

  // Set scroll amount based on above calculations.  WebKit expects positive
  // deltaY to mean "scroll up" and positive deltaX to mean "scroll left".
  if (horizontalScroll) {
    result.delta_x = scrollDelta;
    result.wheel_ticks_x = wheelDelta;
  } else {
    result.delta_y = scrollDelta;
    result.wheel_ticks_y = wheelDelta;
  }
}

CefEventHandle CefBrowserPlatformDelegateNativeWin::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  if (!event.os_event)
    return NULL;
  return const_cast<CefEventHandle>(&event.os_event->native_event());
}

std::unique_ptr<CefFileDialogRunner>
CefBrowserPlatformDelegateNativeWin::CreateFileDialogRunner() {
  return base::WrapUnique(new CefFileDialogRunnerWin);
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegateNativeWin::CreateJavaScriptDialogRunner() {
  return base::WrapUnique(new CefJavaScriptDialogRunnerWin);
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateNativeWin::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerWin);
}

void CefBrowserPlatformDelegateNativeWin::TranslateMouseEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) const {
  // position
  result.SetPositionInWidget(mouse_event.x, mouse_event.y);

  const gfx::Point& screen_pt =
      GetScreenPoint(gfx::Point(mouse_event.x, mouse_event.y));
  result.SetPositionInScreen(screen_pt.x(), screen_pt.y());

  // modifiers
  result.SetModifiers(result.GetModifiers() |
                      TranslateModifiers(mouse_event.modifiers));

  // timestamp
  result.SetTimeStamp(base::TimeTicks() +
                      base::TimeDelta::FromMilliseconds(GetMessageTime()));

  result.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
}

gfx::Point CefBrowserPlatformDelegateNativeWin::GetDialogPosition(
    const gfx::Size& size) {
  const gfx::Size& max_size = GetMaximumDialogSize();
  return gfx::Point((max_size.width() - size.width()) / 2,
                    (max_size.height() - size.height()) / 2);
}

gfx::Size CefBrowserPlatformDelegateNativeWin::GetMaximumDialogSize() {
  return GetWindowWidget()->GetWindowBoundsInScreen().size();
}

// static
void CefBrowserPlatformDelegateNativeWin::RegisterWindowClass() {
  static bool registered = false;
  if (registered)
    return;

  // Register the window class
  WNDCLASSEX wcex = {
      /* cbSize = */ sizeof(WNDCLASSEX),
      /* style = */ CS_HREDRAW | CS_VREDRAW,
      /* lpfnWndProc = */ CefBrowserPlatformDelegateNativeWin::WndProc,
      /* cbClsExtra = */ 0,
      /* cbWndExtra = */ 0,
      /* hInstance = */ ::GetModuleHandle(NULL),
      /* hIcon = */ NULL,
      /* hCursor = */ LoadCursor(NULL, IDC_ARROW),
      /* hbrBackground = */ 0,
      /* lpszMenuName = */ NULL,
      /* lpszClassName = */ CefBrowserPlatformDelegateNativeWin::GetWndClass(),
      /* hIconSm = */ NULL,
  };
  RegisterClassEx(&wcex);

  registered = true;
}

// static
LPCTSTR CefBrowserPlatformDelegateNativeWin::GetWndClass() {
  return L"CefBrowserWindow";
}

// static
LRESULT CALLBACK CefBrowserPlatformDelegateNativeWin::WndProc(HWND hwnd,
                                                              UINT message,
                                                              WPARAM wParam,
                                                              LPARAM lParam) {
  CefBrowserPlatformDelegateNativeWin* platform_delegate = nullptr;
  CefBrowserHostImpl* browser = nullptr;

  if (message != WM_NCCREATE) {
    platform_delegate = static_cast<CefBrowserPlatformDelegateNativeWin*>(
        gfx::GetWindowUserData(hwnd));
    if (platform_delegate)
      browser = platform_delegate->browser_;
  }

  switch (message) {
    case WM_CLOSE:
      if (browser && !browser->TryCloseBrowser()) {
        // Cancel the close.
        return 0;
      }

      // Allow the close.
      break;

    case WM_NCCREATE: {
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
      platform_delegate =
          reinterpret_cast<CefBrowserPlatformDelegateNativeWin*>(
              cs->lpCreateParams);
      DCHECK(platform_delegate);
      // Associate |platform_delegate| with the window handle.
      gfx::SetWindowUserData(hwnd, platform_delegate);
      platform_delegate->window_info_.window = hwnd;

      if (platform_delegate->has_frame_ &&
          base::win::IsProcessPerMonitorDpiAware()) {
        // This call gets Windows to scale the non-client area when
        // WM_DPICHANGED is fired on Windows versions >= 10.0.14393.0.
        static auto enable_non_client_dpi_scaling_func = []() {
          return reinterpret_cast<decltype(::EnableNonClientDpiScaling)*>(
              GetProcAddress(GetModuleHandle(L"user32.dll"),
                             "EnableNonClientDpiScaling"));
        }();
        platform_delegate->called_enable_non_client_dpi_scaling_ =
            !!(enable_non_client_dpi_scaling_func &&
               enable_non_client_dpi_scaling_func(hwnd));
      }
    } break;

    case WM_NCDESTROY:
      if (platform_delegate) {
        // Clear the user data pointer.
        gfx::SetWindowUserData(hwnd, NULL);

        // Force the browser to be destroyed. This will result in a call to
        // BrowserDestroyed() that will release the reference added in
        // CreateHostWindow().
        browser->WindowDestroyed();
      }
      break;

    case WM_SIZE:
      if (platform_delegate && platform_delegate->window_widget_) {
        // Pass window resize events to the HWND for the DesktopNativeWidgetAura
        // root window. Passing size 0x0 (wParam == SIZE_MINIMIZED, for example)
        // will cause the widget to be hidden which reduces resource usage.
        RECT rc;
        GetClientRect(hwnd, &rc);
        SetWindowPos(HWNDForWidget(platform_delegate->window_widget_), NULL,
                     rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER);
      }
      return 0;

    case WM_MOVING:
    case WM_MOVE:
      if (browser)
        browser->NotifyMoveOrResizeStarted();
      return 0;

    case WM_SETFOCUS:
      // Selecting "Close window" from the task bar menu may send a focus
      // notification even though the window is currently disabled (e.g. while
      // a modal JS dialog is displayed).
      if (browser && ::IsWindowEnabled(hwnd))
        browser->SetFocus(true);
      return 0;

    case WM_ERASEBKGND:
      return 0;

    case WM_DPICHANGED:
      if (platform_delegate && platform_delegate->has_frame_) {
        // Suggested size and position of the current window scaled for the
        // new DPI.
        const RECT* rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(platform_delegate->GetHostWindowHandle(), NULL, rect->left,
                     rect->top, rect->right - rect->left,
                     rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
      }
      break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}
