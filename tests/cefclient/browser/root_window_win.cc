// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window_win.h"

#include <shellscalingapi.h>

#include <memory>
#include <optional>

#include "include/base/cef_build.h"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/views/cef_display.h"
#include "tests/cefclient/browser/browser_window_osr_win.h"
#include "tests/cefclient/browser/browser_window_std_win.h"
#include "tests/cefclient/browser/client_prefs.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/cefclient/browser/temp_window.h"
#include "tests/cefclient/browser/window_test_runner_win.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/main_message_loop.h"
#include "tests/shared/browser/util_win.h"
#include "tests/shared/common/client_switches.h"

#define MAX_URL_LENGTH 255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT 24

namespace client {

namespace {

// Message handler for the About box.
INT_PTR CALLBACK AboutWndProc(HWND hDlg,
                              UINT message,
                              WPARAM wParam,
                              LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return TRUE;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return TRUE;
      }
      break;
  }
  return FALSE;
}

// Returns true if the process is per monitor DPI aware.
bool IsProcessPerMonitorDpiAware() {
  enum class PerMonitorDpiAware {
    UNKNOWN = 0,
    PER_MONITOR_DPI_UNAWARE,
    PER_MONITOR_DPI_AWARE,
  };
  static PerMonitorDpiAware per_monitor_dpi_aware = PerMonitorDpiAware::UNKNOWN;
  if (per_monitor_dpi_aware == PerMonitorDpiAware::UNKNOWN) {
    per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_UNAWARE;
    HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    if (shcore_dll) {
      typedef HRESULT(WINAPI * GetProcessDpiAwarenessPtr)(
          HANDLE, PROCESS_DPI_AWARENESS*);
      GetProcessDpiAwarenessPtr func_ptr =
          reinterpret_cast<GetProcessDpiAwarenessPtr>(
              ::GetProcAddress(shcore_dll, "GetProcessDpiAwareness"));
      if (func_ptr) {
        PROCESS_DPI_AWARENESS awareness;
        if (SUCCEEDED(func_ptr(nullptr, &awareness)) &&
            awareness == PROCESS_PER_MONITOR_DPI_AWARE) {
          per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
        }
      }
    }
  }
  return per_monitor_dpi_aware == PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
}

// DPI value for 1x scale factor.
#define DPI_1X 96.0f

// WARNING: Only use this value for scaling native controls. DIP coordinates
// originating from the browser should be converted using GetScreenPixelBounds.
float GetWindowScaleFactor(HWND hwnd) {
  if (hwnd && IsProcessPerMonitorDpiAware()) {
    typedef UINT(WINAPI * GetDpiForWindowPtr)(HWND);
    static GetDpiForWindowPtr func_ptr = reinterpret_cast<GetDpiForWindowPtr>(
        GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForWindow"));
    if (func_ptr) {
      return static_cast<float>(func_ptr(hwnd)) / DPI_1X;
    }
  }

  return client::GetDeviceScaleFactor();
}

int GetButtonWidth(HWND hwnd) {
  return LogicalToDevice(BUTTON_WIDTH, GetWindowScaleFactor(hwnd));
}

int GetURLBarHeight(HWND hwnd) {
  return LogicalToDevice(URLBAR_HEIGHT, GetWindowScaleFactor(hwnd));
}

float GetScaleFactor(const CefRect& bounds,
                     const std::optional<float>& device_scale_factor,
                     bool pixel_bounds) {
  if (device_scale_factor.has_value()) {
    return *device_scale_factor;
  }
  auto display = CefDisplay::GetDisplayMatchingBounds(
      bounds, /*input_pixel_coords=*/pixel_bounds);
  return display->GetDeviceScaleFactor();
}

// Keep the bounds inside the closest display work area.
CefRect ClampBoundsToDisplay(const CefRect& pixel_bounds) {
  auto display = CefDisplay::GetDisplayMatchingBounds(
      pixel_bounds, /*input_pixel_coords=*/true);
  CefRect work_area =
      CefDisplay::ConvertScreenRectToPixels(display->GetWorkArea());

  CefRect bounds = pixel_bounds;
  ConstrainWindowBounds(work_area, bounds);

  return bounds;
}

// Convert DIP screen coordinates originating from the browser to device screen
// (pixel) coordinates. |device_scale_factor| will be specified with off-screen
// rendering.
CefRect GetScreenPixelBounds(const CefRect& dip_bounds,
                             const std::optional<float>& device_scale_factor) {
  if (device_scale_factor.has_value()) {
    return LogicalToDevice(dip_bounds, *device_scale_factor);
  }
  return CefDisplay::ConvertScreenRectToPixels(dip_bounds);
}

// |content_bounds| is the browser content area bounds in DIP screen
// coordinates. Convert to device screen (pixel) coordinates and then expand to
// frame bounds. Keep the resulting bounds inside the closest display work area.
// |device_scale_factor| will be specified with off-screen rendering.
CefRect GetFrameBoundsInDisplay(
    HWND hwnd,
    const CefRect& content_bounds,
    bool with_controls,
    const std::optional<float>& device_scale_factor) {
  CefRect pixel_bounds =
      GetScreenPixelBounds(content_bounds, device_scale_factor);
  if (with_controls) {
    // Expand the bounds to include native controls.
    const int urlbar_height = GetURLBarHeight(hwnd);
    pixel_bounds.y -= urlbar_height;
    pixel_bounds.height += urlbar_height;
  }

  RECT rect = {pixel_bounds.x, pixel_bounds.y,
               pixel_bounds.x + pixel_bounds.width,
               pixel_bounds.y + pixel_bounds.height};
  DWORD style = GetWindowLong(hwnd, GWL_STYLE);
  DWORD ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
  bool has_menu = !(style & WS_CHILD) && (GetMenu(hwnd) != nullptr);

  // Calculate the frame size based on the current style.
  AdjustWindowRectEx(&rect, style, has_menu, ex_style);

  return ClampBoundsToDisplay(
      {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top});
}

// Execute calls on the required threads.
void GetPixelBoundsAndContinue(HWND hwnd,
                               const CefRect& dip_bounds,
                               bool content_bounds,
                               bool with_controls,
                               const std::optional<float>& device_scale_factor,
                               base::OnceCallback<void(const CefRect&)> next) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&GetPixelBoundsAndContinue, hwnd, dip_bounds,
                               content_bounds, with_controls,
                               device_scale_factor, std::move(next)));
    return;
  }

  CefRect pixel_bounds;
  if (content_bounds) {
    pixel_bounds = GetFrameBoundsInDisplay(hwnd, dip_bounds, with_controls,
                                           device_scale_factor);
  } else {
    pixel_bounds = ClampBoundsToDisplay(
        GetScreenPixelBounds(dip_bounds, device_scale_factor));
  }

  if (CURRENTLY_ON_MAIN_THREAD()) {
    std::move(next).Run(pixel_bounds);
  } else {
    MAIN_POST_CLOSURE(base::BindOnce(std::move(next), pixel_bounds));
  }
}

}  // namespace

RootWindowWin::RootWindowWin(bool use_alloy_style)
    : RootWindow(use_alloy_style) {
  // Create a HRGN representing the draggable window area.
  draggable_region_ = ::CreateRectRgn(0, 0, 0, 0);
}

RootWindowWin::~RootWindowWin() {
  REQUIRE_MAIN_THREAD();

  ::DeleteObject(draggable_region_);
  ::DeleteObject(font_);

  // The window and browser should already have been destroyed.
  DCHECK(window_destroyed_);
  DCHECK(browser_destroyed_);
}

void RootWindowWin::Init(RootWindow::Delegate* delegate,
                         std::unique_ptr<RootWindowConfig> config,
                         const CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!initialized_);

  delegate_ = delegate;
  with_controls_ = config->with_controls;
  always_on_top_ = config->always_on_top;
  with_osr_ = config->with_osr;

  CreateBrowserWindow(config->url);

  if (CefCurrentlyOn(TID_UI)) {
    ContinueInitOnUIThread(std::move(config), settings);
  } else {
    CefPostTask(TID_UI, base::BindOnce(&RootWindowWin::ContinueInitOnUIThread,
                                       this, std::move(config), settings));
  }
}

void RootWindowWin::ContinueInitOnUIThread(
    std::unique_ptr<RootWindowConfig> config,
    const CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  if (!config->bounds.IsEmpty()) {
    // Initial state was specified via the config object.
    initial_bounds_ = config->bounds;
    initial_show_state_ = config->show_state;
  } else {
    // Initial state may be specified via the command-line or global
    // preferences.
    std::optional<CefRect> bounds;
    if (prefs::LoadWindowRestorePreferences(initial_show_state_, bounds) &&
        bounds) {
      initial_bounds_ = CefDisplay::ConvertScreenRectToPixels(*bounds);
    }
  }

  if (with_osr_) {
    initial_scale_factor_ =
        GetScaleFactor(initial_bounds_, std::nullopt, /*pixel_bounds=*/true);
  }

  if (CURRENTLY_ON_MAIN_THREAD()) {
    ContinueInitOnMainThread(std::move(config), settings);
  } else {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowWin::ContinueInitOnMainThread,
                                     this, std::move(config), settings));
  }
}

void RootWindowWin::ContinueInitOnMainThread(
    std::unique_ptr<RootWindowConfig> config,
    const CefBrowserSettings& settings) {
  REQUIRE_MAIN_THREAD();

  initialized_ = true;

  CreateRootWindow(settings, config->initially_hidden);
}

void RootWindowWin::InitAsPopup(RootWindow::Delegate* delegate,
                                bool with_controls,
                                bool with_osr,
                                const CefPopupFeatures& popupFeatures,
                                CefWindowInfo& windowInfo,
                                CefRefPtr<CefClient>& client,
                                CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  DCHECK(delegate);
  DCHECK(!initialized_);

  delegate_ = delegate;
  with_controls_ = with_controls;
  with_osr_ = with_osr;
  is_popup_ = true;

  // NOTE: This will be the size for the whole window including frame.
  if (popupFeatures.xSet) {
    initial_bounds_.x = popupFeatures.x;
  }
  if (popupFeatures.ySet) {
    initial_bounds_.y = popupFeatures.y;
  }
  if (popupFeatures.widthSet) {
    initial_bounds_.width = popupFeatures.width;
  }
  if (popupFeatures.heightSet) {
    initial_bounds_.height = popupFeatures.height;
  }
  initial_bounds_ = ClampBoundsToDisplay(
      CefDisplay::ConvertScreenRectToPixels(initial_bounds_));

  if (with_osr_) {
    initial_scale_factor_ =
        GetScaleFactor(initial_bounds_, std::nullopt, /*pixel_bounds=*/true);
  }

  CreateBrowserWindow(std::string());

  initialized_ = true;

  // The new popup is initially parented to a temporary window. The native root
  // window will be created after the browser is created and the popup window
  // will be re-parented to it at that time.
  browser_window_->GetPopupConfig(TempWindow::GetWindowHandle(), windowInfo,
                                  client, settings);
}

void RootWindowWin::Show(ShowMode mode) {
  REQUIRE_MAIN_THREAD();

  if (!hwnd_) {
    return;
  }

  int nCmdShow = SW_SHOWNORMAL;
  switch (mode) {
    case ShowMinimized:
      nCmdShow = SW_SHOWMINIMIZED;
      break;
    case ShowMaximized:
      nCmdShow = SW_SHOWMAXIMIZED;
      break;
    case ShowNoActivate:
      nCmdShow = SW_SHOWNOACTIVATE;
      break;
    default:
      break;
  }

  ShowWindow(hwnd_, nCmdShow);
  if (mode != ShowMinimized) {
    UpdateWindow(hwnd_);
  }
}

void RootWindowWin::Hide() {
  REQUIRE_MAIN_THREAD();

  if (hwnd_) {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

void RootWindowWin::SetBounds(int x,
                              int y,
                              size_t width,
                              size_t height,
                              bool content_bounds) {
  REQUIRE_MAIN_THREAD();

  if (!hwnd_) {
    return;
  }

  CefRect dip_bounds = {x, y, static_cast<int>(width),
                        static_cast<int>(height)};
  GetWindowBoundsAndContinue(
      dip_bounds, content_bounds,
      base::BindOnce(
          [](HWND hwnd, const CefRect& pixel_bounds) {
            SetWindowPos(hwnd, nullptr, pixel_bounds.x, pixel_bounds.y,
                         pixel_bounds.width, pixel_bounds.height, SWP_NOZORDER);
          },
          hwnd_));
}

bool RootWindowWin::DefaultToContentBounds() const {
  if (!WithWindowlessRendering()) {
    // The root HWND will be queried by default.
    return false;
  }
  if (osr_settings_.real_screen_bounds) {
    // Root HWND bounds are provided via GetRootWindowRect.
    return false;
  }
  // The root HWND will not be queried by default.
  return true;
}

void RootWindowWin::GetWindowBoundsAndContinue(
    const CefRect& dip_bounds,
    bool content_bounds,
    base::OnceCallback<void(const CefRect&)> next) {
  REQUIRE_MAIN_THREAD();
  DCHECK(hwnd_);

  GetPixelBoundsAndContinue(hwnd_, dip_bounds, content_bounds, with_controls_,
                            GetDeviceScaleFactor(), std::move(next));
}

void RootWindowWin::Close(bool force) {
  REQUIRE_MAIN_THREAD();

  if (hwnd_) {
    if (force) {
      DestroyWindow(hwnd_);
    } else {
      PostMessage(hwnd_, WM_CLOSE, 0, 0);
    }
  }
}

void RootWindowWin::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    browser_window_->SetDeviceScaleFactor(device_scale_factor);
  }
}

std::optional<float> RootWindowWin::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    return browser_window_->GetDeviceScaleFactor();
  }

  return std::nullopt;
}

CefRefPtr<CefBrowser> RootWindowWin::GetBrowser() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_) {
    return browser_window_->GetBrowser();
  }
  return nullptr;
}

ClientWindowHandle RootWindowWin::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return hwnd_;
}

bool RootWindowWin::WithWindowlessRendering() const {
  REQUIRE_MAIN_THREAD();
  DCHECK(initialized_);
  return with_osr_;
}

void RootWindowWin::CreateBrowserWindow(const std::string& startup_url) {
  if (with_osr_) {
    MainContext::Get()->PopulateOsrSettings(&osr_settings_);
    browser_window_ = std::make_unique<BrowserWindowOsrWin>(
        this, with_controls_, startup_url, osr_settings_);
  } else {
    browser_window_ = std::make_unique<BrowserWindowStdWin>(
        this, with_controls_, startup_url);
  }
}

void RootWindowWin::CreateRootWindow(const CefBrowserSettings& settings,
                                     bool initially_hidden) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!hwnd_);

  HINSTANCE hInstance = GetCodeModuleHandle();

  // Load strings from the resource file.
  const std::wstring& window_title = GetResourceString(IDS_APP_TITLE);
  const std::wstring& window_class = GetResourceString(IDR_MAINFRAME);

  const cef_color_t background_color = MainContext::Get()->GetBackgroundColor();
  const HBRUSH background_brush = CreateSolidBrush(
      RGB(CefColorGetR(background_color), CefColorGetG(background_color),
          CefColorGetB(background_color)));

  // Register the window class.
  RegisterRootClass(hInstance, window_class, background_brush);

  // Register the message used with the find dialog.
  find_message_id_ = RegisterWindowMessage(FINDMSGSTRING);
  CHECK(find_message_id_);

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  const bool no_activate = command_line->HasSwitch(switches::kNoActivate);

  DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  DWORD dwExStyle = always_on_top_ ? WS_EX_TOPMOST : 0;
  if (no_activate) {
    // Don't activate the browser window on creation.
    dwExStyle |= WS_EX_NOACTIVATE;
  }

  if (initial_show_state_ == CEF_SHOW_STATE_MAXIMIZED) {
    dwStyle |= WS_MAXIMIZE;
  } else if (initial_show_state_ == CEF_SHOW_STATE_MINIMIZED) {
    dwStyle |= WS_MINIMIZE;
  }

  int x, y, width, height;
  if (initial_bounds_.IsEmpty()) {
    // Use the default window position/size.
    x = y = width = height = CW_USEDEFAULT;
  } else {
    x = initial_bounds_.x;
    y = initial_bounds_.y;
    width = initial_bounds_.width;
    height = initial_bounds_.height;

    if (is_popup_) {
      // Adjust the window size to account for window frame and controls. Keep
      // the origin unchanged.
      RECT window_rect = {x, y, x + width, y + height};
      ::AdjustWindowRectEx(&window_rect, dwStyle, with_controls_, dwExStyle);
      width = window_rect.right - window_rect.left;
      height = window_rect.bottom - window_rect.top;
    }
  }

  browser_settings_ = settings;

  // Create the main window initially hidden.
  CreateWindowEx(dwExStyle, window_class.c_str(), window_title.c_str(), dwStyle,
                 x, y, width, height, nullptr, nullptr, hInstance, this);
  CHECK(hwnd_);

  if (!called_enable_non_client_dpi_scaling_ && IsProcessPerMonitorDpiAware()) {
    // This call gets Windows to scale the non-client area when WM_DPICHANGED
    // is fired on Windows versions < 10.0.14393.0.
    // Derived signature; not available in headers.
    typedef LRESULT(WINAPI * EnableChildWindowDpiMessagePtr)(HWND, BOOL);
    static EnableChildWindowDpiMessagePtr func_ptr =
        reinterpret_cast<EnableChildWindowDpiMessagePtr>(GetProcAddress(
            GetModuleHandle(L"user32.dll"), "EnableChildWindowDpiMessage"));
    if (func_ptr) {
      func_ptr(hwnd_, TRUE);
    }
  }

  if (!initially_hidden) {
    ShowMode mode = ShowNormal;
    if (no_activate) {
      mode = ShowNoActivate;
    } else if (initial_show_state_ == CEF_SHOW_STATE_MAXIMIZED) {
      mode = ShowMaximized;
    } else if (initial_show_state_ == CEF_SHOW_STATE_MINIMIZED) {
      mode = ShowMinimized;
    }

    // Show this window.
    Show(mode);
  }
}

// static
void RootWindowWin::RegisterRootClass(HINSTANCE hInstance,
                                      const std::wstring& window_class,
                                      HBRUSH background_brush) {
  // Only register the class one time.
  static bool class_registered = false;
  if (class_registered) {
    return;
  }
  class_registered = true;

  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = RootWndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = background_brush;
  wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MAINFRAME);
  wcex.lpszClassName = window_class.c_str();
  wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  RegisterClassEx(&wcex);
}

// static
LRESULT CALLBACK RootWindowWin::EditWndProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam) {
  REQUIRE_MAIN_THREAD();

  RootWindowWin* self = GetUserDataPtr<RootWindowWin*>(hWnd);
  DCHECK(self);
  DCHECK(hWnd == self->edit_hwnd_);

  switch (message) {
    case WM_CHAR:
      if (wParam == VK_RETURN) {
        // When the user hits the enter key load the URL.
        CefRefPtr<CefBrowser> browser = self->GetBrowser();
        if (browser) {
          wchar_t strPtr[MAX_URL_LENGTH + 1] = {0};
          *((LPWORD)strPtr) = MAX_URL_LENGTH;
          LRESULT strLen = SendMessage(hWnd, EM_GETLINE, 0, (LPARAM)strPtr);
          if (strLen > 0) {
            strPtr[strLen] = 0;
            browser->GetMainFrame()->LoadURL(strPtr);
          }
        }
        return 0;
      }
      break;
    case WM_NCDESTROY:
      // Clear the reference to |self|.
      SetUserDataPtr(hWnd, nullptr);
      self->edit_hwnd_ = nullptr;
      break;
  }

  return CallWindowProc(self->edit_wndproc_old_, hWnd, message, wParam, lParam);
}

// static
LRESULT CALLBACK RootWindowWin::FindWndProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam) {
  REQUIRE_MAIN_THREAD();

  RootWindowWin* self = GetUserDataPtr<RootWindowWin*>(hWnd);
  DCHECK(self);
  DCHECK(hWnd == self->find_hwnd_);

  switch (message) {
    case WM_ACTIVATE:
      // Set this dialog as current when activated.
      MainMessageLoop::Get()->SetCurrentModelessDialog(wParam == 0 ? nullptr
                                                                   : hWnd);
      return FALSE;
    case WM_NCDESTROY:
      // Clear the reference to |self|.
      SetUserDataPtr(hWnd, nullptr);
      self->find_hwnd_ = nullptr;
      break;
  }

  return CallWindowProc(self->find_wndproc_old_, hWnd, message, wParam, lParam);
}

// static
LRESULT CALLBACK RootWindowWin::RootWndProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam) {
  REQUIRE_MAIN_THREAD();

  RootWindowWin* self = nullptr;
  if (message != WM_NCCREATE) {
    self = GetUserDataPtr<RootWindowWin*>(hWnd);
    if (!self) {
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    DCHECK_EQ(hWnd, self->hwnd_);
  }

  if (self && message == self->find_message_id_) {
    // Message targeting the find dialog.
    LPFINDREPLACE lpfr = reinterpret_cast<LPFINDREPLACE>(lParam);
    CHECK(lpfr == &self->find_state_);
    self->OnFindEvent();
    return 0;
  }

  // Callback for the main window
  switch (message) {
    case WM_COMMAND:
      if (self->OnCommand(LOWORD(wParam))) {
        return 0;
      }
      break;

    case WM_GETOBJECT: {
      // Only the lower 32 bits of lParam are valid when checking the object id
      // because it sometimes gets sign-extended incorrectly (but not always).
      DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(lParam));

      // Accessibility readers will send an OBJID_CLIENT message.
      if (static_cast<DWORD>(OBJID_CLIENT) == obj_id) {
        if (self->GetBrowser() && self->GetBrowser()->GetHost()) {
          self->GetBrowser()->GetHost()->SetAccessibilityState(STATE_ENABLED);
        }
      }
    } break;

    case WM_PAINT:
      self->OnPaint();
      return 0;

    case WM_ACTIVATE:
      self->OnActivate(LOWORD(wParam) != WA_INACTIVE);
      // Allow DefWindowProc to set keyboard focus.
      break;

    case WM_SETFOCUS:
      self->OnFocus();
      return 0;

    case WM_ENABLE:
      if (wParam == TRUE) {
        // Give focus to the browser after EnableWindow enables this window
        // (e.g. after a modal dialog is dismissed).
        self->OnFocus();
        return 0;
      }
      break;

    case WM_SIZE:
      self->OnSize(wParam == SIZE_MINIMIZED);
      break;

    case WM_MOVING:
    case WM_MOVE:
      self->OnMove();
      return 0;

    case WM_DPICHANGED:
      self->OnDpiChanged(wParam, lParam);
      break;

    case WM_ERASEBKGND:
      if (self->OnEraseBkgnd()) {
        break;
      }
      // Don't erase the background.
      return 0;

    case WM_ENTERMENULOOP:
      if (!wParam) {
        // Entering the menu loop for the application menu.
        CefSetOSModalLoop(true);
      }
      break;

    case WM_EXITMENULOOP:
      if (!wParam) {
        // Exiting the menu loop for the application menu.
        CefSetOSModalLoop(false);
      }
      break;

    case WM_CLOSE:
      if (self->OnClose()) {
        return 0;  // Cancel the close.
      }
      break;

    case WM_NCHITTEST: {
      LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
      if (hit == HTCLIENT) {
        POINTS points = MAKEPOINTS(lParam);
        POINT point = {points.x, points.y};
        ::ScreenToClient(hWnd, &point);
        if (::PtInRegion(self->draggable_region_, point.x, point.y)) {
          // If cursor is inside a draggable region return HTCAPTION to allow
          // dragging.
          return HTCAPTION;
        }
      }
      return hit;
    }

    case WM_NCCREATE: {
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
      self = reinterpret_cast<RootWindowWin*>(cs->lpCreateParams);
      DCHECK(self);
      // Associate |self| with the main window.
      SetUserDataPtr(hWnd, self);
      self->hwnd_ = hWnd;

      self->OnNCCreate(cs);
    } break;

    case WM_CREATE:
      self->OnCreate(reinterpret_cast<CREATESTRUCT*>(lParam));
      break;

    case WM_NCDESTROY:
      // Clear the reference to |self|.
      SetUserDataPtr(hWnd, nullptr);
      self->hwnd_ = nullptr;
      self->OnDestroyed();
      break;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

void RootWindowWin::OnPaint() {
  PAINTSTRUCT ps;
  BeginPaint(hwnd_, &ps);
  EndPaint(hwnd_, &ps);
}

void RootWindowWin::OnFocus() {
  // Selecting "Close window" from the task bar menu may send a focus
  // notification even though the window is currently disabled (e.g. while a
  // modal JS dialog is displayed).
  if (browser_window_ && ::IsWindowEnabled(hwnd_)) {
    browser_window_->SetFocus(true);
  }
}

void RootWindowWin::OnActivate(bool active) {
  if (active) {
    delegate_->OnRootWindowActivated(this);
  }
}

void RootWindowWin::OnSize(bool minimized) {
  if (minimized) {
    // Notify the browser window that it was hidden and do nothing further.
    if (browser_window_) {
      browser_window_->Hide();
    }
    return;
  }

  if (browser_window_) {
    browser_window_->Show();
  }

  RECT rect;
  GetClientRect(hwnd_, &rect);

  if (with_controls_ && edit_hwnd_) {
    const int button_width = GetButtonWidth(hwnd_);
    const int urlbar_height = GetURLBarHeight(hwnd_);
    const int font_height = LogicalToDevice(14, GetWindowScaleFactor(hwnd_));

    if (font_height != font_height_) {
      font_height_ = font_height;
      if (font_) {
        DeleteObject(font_);
      }

      // Create a scaled font.
      font_ =
          ::CreateFont(-font_height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");

      SendMessage(back_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_),
                  TRUE);
      SendMessage(forward_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_),
                  TRUE);
      SendMessage(reload_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_),
                  TRUE);
      SendMessage(stop_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_),
                  TRUE);
      SendMessage(edit_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_),
                  TRUE);
    }

    // Resize the window and address bar to match the new frame size.
    rect.top += urlbar_height;

    int x_offset = rect.left;

    // |browser_hwnd| may be nullptr if the browser has not yet been created.
    HWND browser_hwnd = nullptr;
    if (browser_window_) {
      browser_hwnd = browser_window_->GetWindowHandle();
    }

    // Resize all controls.
    HDWP hdwp = BeginDeferWindowPos(browser_hwnd ? 6 : 5);
    hdwp = DeferWindowPos(hdwp, back_hwnd_, nullptr, x_offset, 0, button_width,
                          urlbar_height, SWP_NOZORDER);
    x_offset += button_width;
    hdwp = DeferWindowPos(hdwp, forward_hwnd_, nullptr, x_offset, 0,
                          button_width, urlbar_height, SWP_NOZORDER);
    x_offset += button_width;
    hdwp = DeferWindowPos(hdwp, reload_hwnd_, nullptr, x_offset, 0,
                          button_width, urlbar_height, SWP_NOZORDER);
    x_offset += button_width;
    hdwp = DeferWindowPos(hdwp, stop_hwnd_, nullptr, x_offset, 0, button_width,
                          urlbar_height, SWP_NOZORDER);
    x_offset += button_width;
    hdwp = DeferWindowPos(hdwp, edit_hwnd_, nullptr, x_offset, 0,
                          rect.right - x_offset, urlbar_height, SWP_NOZORDER);

    if (browser_hwnd) {
      hdwp = DeferWindowPos(hdwp, browser_hwnd, nullptr, rect.left, rect.top,
                            rect.right - rect.left, rect.bottom - rect.top,
                            SWP_NOZORDER);
    }

    [[maybe_unused]] BOOL result = EndDeferWindowPos(hdwp);
    DCHECK(result);
  } else if (browser_window_) {
    // Size the browser window to the whole client area.
    browser_window_->SetBounds(0, 0, rect.right, rect.bottom);
  }

  MaybeNotifyScreenInfoChanged();
}

void RootWindowWin::OnMove() {
  // Notify the browser of move events so that popup windows are displayed
  // in the correct location and dismissed when the window moves.
  if (auto browser = GetBrowser()) {
    browser->GetHost()->NotifyMoveOrResizeStarted();
  }

  MaybeNotifyScreenInfoChanged();
}

void RootWindowWin::MaybeNotifyScreenInfoChanged() {
  if (!DefaultToContentBounds()) {
    // Send the new root window bounds to the renderer.
    if (auto browser = GetBrowser()) {
      browser->GetHost()->NotifyScreenInfoChanged();
    }
  }
}

void RootWindowWin::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  if (LOWORD(wParam) != HIWORD(wParam)) {
    NOTIMPLEMENTED() << "Received non-square scaling factors";
    return;
  }

  if (!hwnd_) {
    return;
  }

  if (browser_window_ && with_osr_) {
    // Scale factor for the new display.
    const float display_scale_factor =
        static_cast<float>(LOWORD(wParam)) / DPI_1X;
    browser_window_->SetDeviceScaleFactor(display_scale_factor);
  }

  // Suggested size and position of the current window scaled for the new DPI.
  const RECT* rect = reinterpret_cast<RECT*>(lParam);
  SetWindowPos(hwnd_, nullptr, rect->left, rect->top, rect->right - rect->left,
               rect->bottom - rect->top, SWP_NOZORDER);
}

bool RootWindowWin::OnEraseBkgnd() {
  // Erase the background when the browser does not exist.
  return (GetBrowser() == nullptr);
}

bool RootWindowWin::OnCommand(UINT id) {
  if (id >= ID_TESTS_FIRST && id <= ID_TESTS_LAST) {
    delegate_->OnTest(this, id);
    return true;
  }

  switch (id) {
    case IDM_ABOUT:
      OnAbout();
      return true;
    case IDM_EXIT:
      delegate_->OnExit(this);
      return true;
    case ID_FIND:
      OnFind();
      return true;
    case IDC_NAV_BACK:  // Back button
      if (CefRefPtr<CefBrowser> browser = GetBrowser()) {
        browser->GoBack();
      }
      return true;
    case IDC_NAV_FORWARD:  // Forward button
      if (CefRefPtr<CefBrowser> browser = GetBrowser()) {
        browser->GoForward();
      }
      return true;
    case IDC_NAV_RELOAD:  // Reload button
      if (CefRefPtr<CefBrowser> browser = GetBrowser()) {
        browser->Reload();
      }
      return true;
    case IDC_NAV_STOP:  // Stop button
      if (CefRefPtr<CefBrowser> browser = GetBrowser()) {
        browser->StopLoad();
      }
      return true;
  }

  return false;
}

void RootWindowWin::OnFind() {
  if (find_hwnd_) {
    // Give focus to the existing find dialog.
    ::SetFocus(find_hwnd_);
    return;
  }

  // Configure dialog state.
  ZeroMemory(&find_state_, sizeof(find_state_));
  find_state_.lStructSize = sizeof(find_state_);
  find_state_.hwndOwner = hwnd_;
  find_state_.lpstrFindWhat = find_buff_;
  find_state_.wFindWhatLen = sizeof(find_buff_);
  find_state_.Flags = FR_HIDEWHOLEWORD | FR_DOWN;

  // Create the dialog.
  find_hwnd_ = FindText(&find_state_);

  // Override the dialog's window procedure.
  find_wndproc_old_ = SetWndProcPtr(find_hwnd_, FindWndProc);

  // Associate |self| with the dialog.
  SetUserDataPtr(find_hwnd_, this);
}

void RootWindowWin::OnFindEvent() {
  CefRefPtr<CefBrowser> browser = GetBrowser();

  if (find_state_.Flags & FR_DIALOGTERM) {
    // The find dialog box has been dismissed so invalidate the handle and
    // reset the search results.
    if (browser) {
      browser->GetHost()->StopFinding(true);
      find_what_last_.clear();
      find_next_ = false;
    }
  } else if ((find_state_.Flags & FR_FINDNEXT) && browser) {
    // Search for the requested string.
    bool match_case = ((find_state_.Flags & FR_MATCHCASE) ? true : false);
    const std::wstring& find_what = find_buff_;
    if (match_case != find_match_case_last_ || find_what != find_what_last_) {
      // The search string has changed, so reset the search results.
      if (!find_what.empty()) {
        browser->GetHost()->StopFinding(true);
        find_next_ = false;
      }
      find_match_case_last_ = match_case;
      find_what_last_ = find_buff_;
    }

    browser->GetHost()->Find(find_what,
                             (find_state_.Flags & FR_DOWN) ? true : false,
                             match_case, find_next_);
    if (!find_next_) {
      find_next_ = true;
    }
  }
}

void RootWindowWin::OnAbout() {
  // Show the about box.
  DialogBox(GetCodeModuleHandle(), MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd_,
            AboutWndProc);
}

void RootWindowWin::OnNCCreate(LPCREATESTRUCT lpCreateStruct) {
  if (IsProcessPerMonitorDpiAware()) {
    // This call gets Windows to scale the non-client area when WM_DPICHANGED
    // is fired on Windows versions >= 10.0.14393.0.
    typedef BOOL(WINAPI * EnableNonClientDpiScalingPtr)(HWND);
    static EnableNonClientDpiScalingPtr func_ptr =
        reinterpret_cast<EnableNonClientDpiScalingPtr>(GetProcAddress(
            GetModuleHandle(L"user32.dll"), "EnableNonClientDpiScaling"));
    called_enable_non_client_dpi_scaling_ = !!(func_ptr && func_ptr(hwnd_));
  }
}

void RootWindowWin::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  const HINSTANCE hInstance = lpCreateStruct->hInstance;

  RECT rect;
  GetClientRect(hwnd_, &rect);

  if (with_controls_) {
    // Create the child controls.
    int x_offset = 0;

    const int button_width = GetButtonWidth(hwnd_);
    const int urlbar_height = GetURLBarHeight(hwnd_);

    back_hwnd_ = CreateWindow(
        L"BUTTON", L"Back", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        x_offset, 0, button_width, urlbar_height, hwnd_,
        reinterpret_cast<HMENU>(IDC_NAV_BACK), hInstance, nullptr);
    CHECK(back_hwnd_);
    x_offset += button_width;

    forward_hwnd_ = CreateWindow(
        L"BUTTON", L"Forward",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, x_offset, 0,
        button_width, urlbar_height, hwnd_,
        reinterpret_cast<HMENU>(IDC_NAV_FORWARD), hInstance, nullptr);
    CHECK(forward_hwnd_);
    x_offset += button_width;

    reload_hwnd_ = CreateWindow(
        L"BUTTON", L"Reload",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED, x_offset, 0,
        button_width, urlbar_height, hwnd_,
        reinterpret_cast<HMENU>(IDC_NAV_RELOAD), hInstance, nullptr);
    CHECK(reload_hwnd_);
    x_offset += button_width;

    stop_hwnd_ = CreateWindow(
        L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        x_offset, 0, button_width, urlbar_height, hwnd_,
        reinterpret_cast<HMENU>(IDC_NAV_STOP), hInstance, nullptr);
    CHECK(stop_hwnd_);
    x_offset += button_width;

    edit_hwnd_ =
        CreateWindow(L"EDIT", nullptr,
                     WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                         ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_DISABLED,
                     x_offset, 0, rect.right - button_width * 4, urlbar_height,
                     hwnd_, nullptr, hInstance, nullptr);
    CHECK(edit_hwnd_);

    // Override the edit control's window procedure.
    edit_wndproc_old_ = SetWndProcPtr(edit_hwnd_, EditWndProc);

    // Associate |this| with the edit window.
    SetUserDataPtr(edit_hwnd_, this);

    rect.top += urlbar_height;

    if (!with_osr_) {
      // Remove the menu items that are only used with OSR.
      HMENU hMenu = ::GetMenu(hwnd_);
      if (hMenu) {
        HMENU hTestMenu = ::GetSubMenu(hMenu, 2);
        if (hTestMenu) {
          ::RemoveMenu(hTestMenu, ID_TESTS_OSR_FPS, MF_BYCOMMAND);
          ::RemoveMenu(hTestMenu, ID_TESTS_OSR_DSF, MF_BYCOMMAND);
        }
      }
    }
  } else {
    // No controls so also remove the default menu.
    ::SetMenu(hwnd_, nullptr);
  }

  if (with_osr_) {
    std::optional<float> parent_scale_factor;
    if (is_popup_) {
      if (auto parent_window =
              MainContext::Get()->GetRootWindowManager()->GetWindowForBrowser(
                  opener_browser_id())) {
        parent_scale_factor = parent_window->GetDeviceScaleFactor();
      }
    }

    browser_window_->SetDeviceScaleFactor(
        parent_scale_factor.value_or(initial_scale_factor_));
  }

  CefRect bounds(rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top);
  if (!is_popup_) {
    // Create the browser window.
    browser_window_->CreateBrowser(hwnd_, bounds, browser_settings_, nullptr,
                                   delegate_->GetRequestContext());
  } else {
    // With popups we already have a browser window. Parent the browser window
    // to the root window and show it in the correct location.
    browser_window_->ShowPopup(hwnd_, bounds.x, bounds.y, bounds.width,
                               bounds.height);
  }

  window_created_ = true;
}

bool RootWindowWin::OnClose() {
  // Retrieve current window placement information.
  WINDOWPLACEMENT placement;
  ::GetWindowPlacement(hwnd_, &placement);

  if (CefCurrentlyOn(TID_UI)) {
    SaveWindowRestoreOnUIThread(placement);
  } else {
    CefPostTask(
        TID_UI,
        base::BindOnce(&RootWindowWin::SaveWindowRestoreOnUIThread, placement));
  }

  if (browser_window_ && !browser_window_->IsClosing()) {
    CefRefPtr<CefBrowser> browser = GetBrowser();
    if (browser) {
      // Notify the browser window that we would like to close it. With Alloy
      // style this will result in a call to ClientHandler::DoClose() if the
      // JavaScript 'onbeforeunload' event handler allows it. With Chrome style
      // this will close the window indirectly via browser destruction.
      browser->GetHost()->CloseBrowser(false);

      // Cancel the close.
      return true;
    }
  }

  // Allow the close.
  return false;
}

void RootWindowWin::OnDestroyed() {
  window_destroyed_ = true;
  NotifyDestroyedIfDone();
}

void RootWindowWin::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  if (is_popup_) {
    // For popup browsers create the root window once the browser has been
    // created.
    CreateRootWindow(CefBrowserSettings(), false);
  } else {
    // Make sure the browser is sized correctly.
    OnSize(false);
  }
}

void RootWindowWin::OnBrowserWindowDestroyed() {
  REQUIRE_MAIN_THREAD();

  browser_window_.reset();

  if (!window_destroyed_) {
    // The browser was destroyed first. This could be due to the use of
    // off-screen rendering or native (external) parent, or execution of
    // JavaScript window.close(). Close the RootWindow asyncronously to allow
    // the current call stack to unwind.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowWin::Close, this, true));
  }

  browser_destroyed_ = true;
  NotifyDestroyedIfDone();
}

void RootWindowWin::OnSetAddress(const std::string& url) {
  REQUIRE_MAIN_THREAD();

  if (edit_hwnd_) {
    SetWindowText(edit_hwnd_, CefString(url).ToWString().c_str());
  }
}

void RootWindowWin::OnSetTitle(const std::string& title) {
  REQUIRE_MAIN_THREAD();

  if (hwnd_) {
    SetWindowText(hwnd_, CefString(title).ToWString().c_str());
  }
}

void RootWindowWin::OnSetFullscreen(bool fullscreen) {
  REQUIRE_MAIN_THREAD();

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser) {
    std::unique_ptr<window_test::WindowTestRunnerWin> test_runner(
        new window_test::WindowTestRunnerWin());
    if (fullscreen) {
      test_runner->Maximize(browser);
    } else {
      test_runner->Restore(browser);
    }
  }
}

void RootWindowWin::OnAutoResize(const CefSize& new_size) {
  REQUIRE_MAIN_THREAD();

  if (!hwnd_) {
    return;
  }

  CefRect dip_bounds = {0, 0, new_size.width, new_size.height};

  GetWindowBoundsAndContinue(
      dip_bounds, /*content_bounds=*/true,
      base::BindOnce(
          [](HWND hwnd, const CefRect& pixel_bounds) {
            // Size the window and show if it's not currently visible.
            SetWindowPos(
                hwnd, nullptr, 0, 0, pixel_bounds.width, pixel_bounds.height,
                SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
          },
          hwnd_));
}

void RootWindowWin::OnSetLoadingState(bool isLoading,
                                      bool canGoBack,
                                      bool canGoForward) {
  REQUIRE_MAIN_THREAD();

  if (with_controls_) {
    EnableWindow(back_hwnd_, canGoBack);
    EnableWindow(forward_hwnd_, canGoForward);
    EnableWindow(reload_hwnd_, !isLoading);
    EnableWindow(stop_hwnd_, isLoading);
    EnableWindow(edit_hwnd_, TRUE);
  }

  if (!isLoading && GetWindowLongPtr(hwnd_, GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
    // Done with the initial navigation. Remove the WS_EX_NOACTIVATE style so
    // that future mouse clicks inside the browser correctly activate and focus
    // the window. For the top-level window removing this style causes Windows
    // to display the task bar button.
    SetWindowLongPtr(hwnd_, GWL_EXSTYLE,
                     GetWindowLongPtr(hwnd_, GWL_EXSTYLE) & ~WS_EX_NOACTIVATE);

    if (browser_window_) {
      HWND browser_hwnd = browser_window_->GetWindowHandle();
      SetWindowLongPtr(
          browser_hwnd, GWL_EXSTYLE,
          GetWindowLongPtr(browser_hwnd, GWL_EXSTYLE) & ~WS_EX_NOACTIVATE);
    }
  }
}

namespace {

LPCWSTR kParentWndProc = L"CefParentWndProc";
LPCWSTR kDraggableRegion = L"CefDraggableRegion";

LRESULT CALLBACK SubclassedWindowProc(HWND hWnd,
                                      UINT message,
                                      WPARAM wParam,
                                      LPARAM lParam) {
  WNDPROC hParentWndProc =
      reinterpret_cast<WNDPROC>(::GetPropW(hWnd, kParentWndProc));
  HRGN hRegion = reinterpret_cast<HRGN>(::GetPropW(hWnd, kDraggableRegion));

  if (message == WM_NCHITTEST) {
    LRESULT hit = CallWindowProc(hParentWndProc, hWnd, message, wParam, lParam);
    if (hit == HTCLIENT) {
      POINTS points = MAKEPOINTS(lParam);
      POINT point = {points.x, points.y};
      ::ScreenToClient(hWnd, &point);
      if (::PtInRegion(hRegion, point.x, point.y)) {
        // Let the parent window handle WM_NCHITTEST by returning HTTRANSPARENT
        // in child windows.
        return HTTRANSPARENT;
      }
    }
    return hit;
  }

  return CallWindowProc(hParentWndProc, hWnd, message, wParam, lParam);
}

void SubclassWindow(HWND hWnd, HRGN hRegion) {
  HANDLE hParentWndProc = ::GetPropW(hWnd, kParentWndProc);
  if (hParentWndProc) {
    return;
  }

  SetLastError(0);
  LONG_PTR hOldWndProc = SetWindowLongPtr(
      hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubclassedWindowProc));
  if (hOldWndProc == 0 && GetLastError() != ERROR_SUCCESS) {
    return;
  }

  ::SetPropW(hWnd, kParentWndProc, reinterpret_cast<HANDLE>(hOldWndProc));
  ::SetPropW(hWnd, kDraggableRegion, reinterpret_cast<HANDLE>(hRegion));
}

void UnSubclassWindow(HWND hWnd) {
  LONG_PTR hParentWndProc =
      reinterpret_cast<LONG_PTR>(::GetPropW(hWnd, kParentWndProc));
  if (hParentWndProc) {
    [[maybe_unused]] LONG_PTR hPreviousWndProc =
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, hParentWndProc);
    DCHECK_EQ(hPreviousWndProc,
              reinterpret_cast<LONG_PTR>(SubclassedWindowProc));
  }

  ::RemovePropW(hWnd, kParentWndProc);
  ::RemovePropW(hWnd, kDraggableRegion);
}

BOOL CALLBACK SubclassWindowsProc(HWND hwnd, LPARAM lParam) {
  SubclassWindow(hwnd, reinterpret_cast<HRGN>(lParam));
  return TRUE;
}

BOOL CALLBACK UnSubclassWindowsProc(HWND hwnd, LPARAM lParam) {
  UnSubclassWindow(hwnd);
  return TRUE;
}

}  // namespace

void RootWindowWin::OnSetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  REQUIRE_MAIN_THREAD();

  // Reset draggable region.
  ::SetRectRgn(draggable_region_, 0, 0, 0, 0);

  // Determine new draggable region.
  std::vector<CefDraggableRegion>::const_iterator it = regions.begin();
  for (; it != regions.end(); ++it) {
    HRGN region = ::CreateRectRgn(it->bounds.x, it->bounds.y,
                                  it->bounds.x + it->bounds.width,
                                  it->bounds.y + it->bounds.height);
    ::CombineRgn(draggable_region_, draggable_region_, region,
                 it->draggable ? RGN_OR : RGN_DIFF);
    ::DeleteObject(region);
  }

  // Subclass child window procedures in order to do hit-testing.
  // This will be a no-op, if it is already subclassed.
  if (hwnd_) {
    WNDENUMPROC proc =
        !regions.empty() ? SubclassWindowsProc : UnSubclassWindowsProc;
    ::EnumChildWindows(hwnd_, proc,
                       reinterpret_cast<LPARAM>(draggable_region_));
  }
}

void RootWindowWin::NotifyDestroyedIfDone() {
  // Notify once both the window and the browser have been destroyed.
  if (window_destroyed_ && browser_destroyed_) {
    delegate_->OnRootWindowDestroyed(this);
  }
}

// static
void RootWindowWin::SaveWindowRestoreOnUIThread(
    const WINDOWPLACEMENT& placement) {
  CEF_REQUIRE_UI_THREAD();

  cef_show_state_t show_state = CEF_SHOW_STATE_NORMAL;
  if (placement.showCmd == SW_SHOWMINIMIZED) {
    show_state = CEF_SHOW_STATE_MINIMIZED;
  } else if (placement.showCmd == SW_SHOWMAXIMIZED) {
    show_state = CEF_SHOW_STATE_MAXIMIZED;
  }

  // Coordinates when the window is in the restored position.
  const auto rect = placement.rcNormalPosition;
  CefRect pixel_bounds(rect.left, rect.top, rect.right - rect.left,
                       rect.bottom - rect.top);
  const auto dip_bounds = CefDisplay::ConvertScreenRectFromPixels(pixel_bounds);

  prefs::SaveWindowRestorePreferences(show_state, dip_bounds);
}

}  // namespace client
