// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <sys/sysinfo.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "libcef/browser/context.h"
#include "libcef/browser/window_delegate_view.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/web/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the number of seconds since system boot.
long GetSystemUptime() {
  struct sysinfo info;
  if (sysinfo(&info) == 0)
    return info.uptime;
  return 0;
}

const char* kAtomsToCache[] = {
  "WM_DELETE_WINDOW",
  "WM_PROTOCOLS",
  "_NET_WM_PING",
  "_NET_WM_PID",
  NULL
};

::Window FindEventTarget(const base::NativeEvent& xev) {
  ::Window target = xev->xany.window;
  if (xev->type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xev->xcookie.data)->event;
  return target;
}

::Window FindChild(::Display* display, ::Window window) {
  ::Window root;
  ::Window parent;
  ::Window* children;
  unsigned int nchildren;
  if (XQueryTree(display, window, &root, &parent, &children, &nchildren)) {
    DCHECK_EQ(1U, nchildren);
    return children[0];
  }
  return None;
}

}  // namespace

#if defined(USE_X11)
CEF_EXPORT XDisplay* cef_get_xdisplay() {
  if (!CEF_CURRENTLY_ON(CEF_UIT))
    return NULL;
  return gfx::GetXDisplay();
}
#endif

// Object wrapper for an X11 Window.
// Based on WindowTreeHostX11 and DesktopWindowTreeHostX11.
class CefWindowX11 : public ui::PlatformEventDispatcher {
 public:
  CefWindowX11(CefRefPtr<CefBrowserHostImpl> browser,
               ::Window parent_xwindow,
               const gfx::Rect& bounds)
    : browser_(browser),
      xdisplay_(gfx::GetXDisplay()),
      parent_xwindow_(parent_xwindow),
      xwindow_(0),
      window_mapped_(false),
      bounds_(bounds),
      atom_cache_(xdisplay_, kAtomsToCache) {
    if (parent_xwindow_ == None)
      parent_xwindow_ = DefaultRootWindow(xdisplay_);

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.background_pixmap = None;
    swa.override_redirect = false;
    xwindow_ = XCreateWindow(
        xdisplay_, parent_xwindow_,
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        0,               // border width
        CopyFromParent,  // depth
        InputOutput,
        CopyFromParent,  // visual
        CWBackPixmap | CWOverrideRedirect,
        &swa);

    if (ui::PlatformEventSource::GetInstance())
      ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

    long event_mask = FocusChangeMask | StructureNotifyMask | PropertyChangeMask;
    XSelectInput(xdisplay_, xwindow_, event_mask);
    XFlush(xdisplay_);

    // TODO(erg): We currently only request window deletion events. We also
    // should listen for activation events and anything else that GTK+ listens
    // for, and do something useful.
    ::Atom protocols[2];
    protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
    protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
    XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

    // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
    // the desktop environment.
    XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

    // Likewise, the X server needs to know this window's pid so it knows which
    // program to kill if the window hangs.
    // XChangeProperty() expects "pid" to be long.
    COMPILE_ASSERT(sizeof(long) >= sizeof(pid_t), pid_t_bigger_than_long);
    long pid = getpid();
    XChangeProperty(xdisplay_,
                    xwindow_,
                    atom_cache_.GetAtom("_NET_WM_PID"),
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    reinterpret_cast<unsigned char*>(&pid), 1);

    // Allow subclasses to create and cache additional atoms.
    atom_cache_.allow_uncached_atoms();
  }

  virtual ~CefWindowX11() {
    DCHECK(!xwindow_);
    if (ui::PlatformEventSource::GetInstance())
      ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }

  void Close() {
    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = xwindow_;
    ev.xclient.message_type = atom_cache_.GetAtom("WM_PROTOCOLS");
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(xdisplay_, xwindow_, False, NoEventMask, &ev);
  }

  void Show() {
    if (xwindow_ == None)
      return;

    if (!window_mapped_) {
      // Before we map the window, set size hints. Otherwise, some window managers
      // will ignore toplevel XMoveWindow commands.
      XSizeHints size_hints;
      size_hints.flags = PPosition | PWinGravity;
      size_hints.x = bounds_.x();
      size_hints.y = bounds_.y();
      // Set StaticGravity so that the window position is not affected by the
      // frame width when running with window manager.
      size_hints.win_gravity = StaticGravity;
      XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

      XMapWindow(xdisplay_, xwindow_);

      // We now block until our window is mapped. Some X11 APIs will crash and
      // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
      // asynchronous.
      if (ui::X11EventSource::GetInstance())
        ui::X11EventSource::GetInstance()->BlockUntilWindowMapped(xwindow_);
      window_mapped_ = true;
    }
  }

  void Hide() {
    if (xwindow_ == None)
      return;

    if (window_mapped_) {
      XWithdrawWindow(xdisplay_, xwindow_, 0);
      window_mapped_ = false;
    }
  }

  void SetBounds(const gfx::Rect& bounds) {
    if (xwindow_ == None)
      return;

    bool origin_changed = bounds_.origin() != bounds.origin();
    bool size_changed = bounds_.size() != bounds.size();
    XWindowChanges changes = {0};
    unsigned value_mask = 0;

    if (size_changed) {
      changes.width = bounds.width();
      changes.height = bounds.height();
      value_mask = CWHeight | CWWidth;
    }

    if (origin_changed) {
      changes.x = bounds.x();
      changes.y = bounds.y();
      value_mask |= CWX | CWY;
    }

    if (value_mask)
      XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);
  }

  // ui::PlatformEventDispatcher methods:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    ::Window target = FindEventTarget(event);
     return target == xwindow_;
  }

  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    XEvent* xev = event;
    switch (xev->type) {
      case ConfigureNotify: {
        DCHECK_EQ(xwindow_, xev->xconfigure.event);
        DCHECK_EQ(xwindow_, xev->xconfigure.window);
        // It's possible that the X window may be resized by some other means
        // than from within Aura (e.g. the X window manager can change the
        // size). Make sure the root window size is maintained properly.
        gfx::Rect bounds(xev->xconfigure.x, xev->xconfigure.y,
            xev->xconfigure.width, xev->xconfigure.height);
        bounds_ = bounds;

        ::Window child = FindChild(xdisplay_, xwindow_);
        if (child) {
          // Resize the child DesktopWindowTreeHostX11 to match this window.
          XWindowChanges changes = {0};
          changes.width = bounds.width();
          changes.height = bounds.height();
          XConfigureWindow(xdisplay_, child, CWHeight | CWWidth, &changes);
        }
        break;
      }
      case ClientMessage: {
        Atom message_type = static_cast<Atom>(xev->xclient.data.l[0]);
        if (message_type == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
          // We have received a close message from the window manager.
          if (browser_->destruction_state() <=
              CefBrowserHostImpl::DESTRUCTION_STATE_PENDING) {
            if (browser_->destruction_state() ==
                CefBrowserHostImpl::DESTRUCTION_STATE_NONE) {
              // Request that the browser close.
              browser_->CloseBrowser(false);
            }

            // Cancel the close.
          } else {
            // Allow the close.
            XDestroyWindow(xdisplay_, xwindow_);
          }
        } else if (message_type == atom_cache_.GetAtom("_NET_WM_PING")) {
          XEvent reply_event = *xev;
          reply_event.xclient.window = parent_xwindow_;

          XSendEvent(xdisplay_,
                     reply_event.xclient.window,
                     False,
                     SubstructureRedirectMask | SubstructureNotifyMask,
                     &reply_event);
          XFlush(xdisplay_);
        }
        break;
      }
      case DestroyNotify:
        xwindow_ = None;

        // Force the browser to be destroyed and release the reference added
        // in PlatformCreateWindow().
        browser_->WindowDestroyed();

        delete this;
        break;
      case FocusIn:
        // This message is recieved first followed by a "_NET_ACTIVE_WINDOW"
        // message sent to the root window. When X11DesktopHandler handles the
        // "_NET_ACTIVE_WINDOW" message it will erroneously mark the WebView
        // (hosted in a DesktopWindowTreeHostX11) as unfocused. Use a delayed
        // task here to restore the WebView's focus state.
        CEF_POST_DELAYED_TASK(CEF_UIT,
            base::Bind(&CefBrowserHostImpl::OnSetFocus, browser_,
                       FOCUS_SOURCE_SYSTEM),
            100);
        break;
    }

    return ui::POST_DISPATCH_STOP_PROPAGATION;
  }

  ::Window xwindow() const { return xwindow_; }
  gfx::Rect bounds() const { return bounds_; }

 private:
  CefRefPtr<CefBrowserHostImpl> browser_;

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window parent_xwindow_;
  ::Window xwindow_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  ui::X11AtomCache atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(CefWindowX11);
};


bool CefBrowserHostImpl::PlatformCreateWindow() {
  DCHECK(!window_x11_);
  DCHECK(!window_widget_);

  if (window_info_.width == 0)
    window_info_.width = 800;
  if (window_info_.height == 0)
    window_info_.height = 600;

  gfx::Rect rect(window_info_.x, window_info_.y,
                 window_info_.width, window_info_.height);

  // Create a new window object. It will delete itself when the associated X11
  // window is destroyed.
  window_x11_ = new CefWindowX11(this, window_info_.parent_window, rect);
  window_info_.window = window_x11_->xwindow();

  // Add a reference that will be released in the destroy handler.
  AddRef();

  SkColor background_color = SK_ColorWHITE;
  const CefSettings& settings = CefContext::Get()->settings();
  if (CefColorGetA(settings.background_color) > 0) {
    background_color = SkColorSetRGB(
        CefColorGetR(settings.background_color),
        CefColorGetG(settings.background_color),
        CefColorGetB(settings.background_color));
  }

  CefWindowDelegateView* delegate_view =
      new CefWindowDelegateView(background_color);
  delegate_view->Init(window_info_.window,
                      web_contents(),
                      gfx::Rect(gfx::Point(), rect.size()));

  window_widget_ = delegate_view->GetWidget();
  window_widget_->Show();

  window_x11_->Show();

  // As an additional requirement on Linux, we must set the colors for the
  // render widgets in webkit.
  content::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);
  prefs->thumb_active_color = SkColorSetRGB(244, 244, 244);
  prefs->thumb_inactive_color = SkColorSetRGB(234, 234, 234);
  prefs->track_color = SkColorSetRGB(211, 211, 211);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);

  return true;
}

void CefBrowserHostImpl::PlatformCloseWindow() {
  if (window_x11_)
    window_x11_->Close();
}

void CefBrowserHostImpl::PlatformSizeTo(int width, int height) {
  if (window_x11_) {
    window_x11_->SetBounds(
        gfx::Rect(window_x11_->bounds().origin(), gfx::Size(width, height)));
  }
}

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return window_info_.window;
}

bool CefBrowserHostImpl::PlatformViewText(const std::string& text) {
  CEF_REQUIRE_UIT();

  char buff[] = "/tmp/CEFSourceXXXXXX";
  int fd = mkstemp(buff);

  if (fd == -1)
    return false;

  FILE* srcOutput = fdopen(fd, "w+");
  if (!srcOutput)
    return false;

  if (fputs(text.c_str(), srcOutput) < 0) {
    fclose(srcOutput);
    return false;
  }

  fclose(srcOutput);

  std::string newName(buff);
  newName.append(".txt");
  if (rename(buff, newName.c_str()) != 0)
    return false;

  std::string openCommand("xdg-open ");
  openCommand += newName;

  if (system(openCommand.c_str()) != 0)
    return false;

  return true;
}

void CefBrowserHostImpl::PlatformHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // TODO(cef): Is something required here to handle shortcut keys?
}

void CefBrowserHostImpl::PlatformRunFileChooser(
    const content::FileChooserParams& params,
    RunFileChooserCallback callback) {
  NOTIMPLEMENTED();
  std::vector<base::FilePath> files;
  callback.Run(files);
}

void CefBrowserHostImpl::PlatformHandleExternalProtocol(const GURL& url) {
}

void CefBrowserHostImpl::PlatformTranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) {
  NOTIMPLEMENTED();
}

void CefBrowserHostImpl::PlatformTranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    MouseButtonType type,
    bool mouseUp, int clickCount) {
  PlatformTranslateMouseEvent(result, mouse_event);

  switch (type) {
  case MBT_LEFT:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonLeft;
    break;
  case MBT_MIDDLE:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonMiddle;
    break;
  case MBT_RIGHT:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonRight;
    break;
  default:
    NOTREACHED();
  }

  result.clickCount = clickCount;
}

void CefBrowserHostImpl::PlatformTranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) {
  PlatformTranslateMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.type = blink::WebInputEvent::MouseMove;
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonRight;
    else
      result.button = blink::WebMouseEvent::ButtonNone;
  } else {
    result.type = blink::WebInputEvent::MouseLeave;
    result.button = blink::WebMouseEvent::ButtonNone;
  }

  result.clickCount = 0;
}

void CefBrowserHostImpl::PlatformTranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX, int deltaY) {
  result = blink::WebMouseWheelEvent();
  PlatformTranslateMouseEvent(result, mouse_event);

  result.type = blink::WebInputEvent::MouseWheel;

  static const double scrollbarPixelsPerGtkTick = 40.0;
  result.deltaX = deltaX;
  result.deltaY = deltaY;
  result.wheelTicksX = result.deltaX / scrollbarPixelsPerGtkTick;
  result.wheelTicksY = result.deltaY / scrollbarPixelsPerGtkTick;
  result.hasPreciseScrollingDeltas = true;

  // Unless the phase and momentumPhase are passed in as parameters to this
  // function, there is no way to know them
  result.phase = blink::WebMouseWheelEvent::PhaseNone;
  result.momentumPhase = blink::WebMouseWheelEvent::PhaseNone;

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonLeft;
  else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonMiddle;
  else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonRight;
  else
    result.button = blink::WebMouseEvent::ButtonNone;
}

void CefBrowserHostImpl::PlatformTranslateMouseEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) {
  // position
  result.x = mouse_event.x;
  result.y = mouse_event.y;
  result.windowX = result.x;
  result.windowY = result.y;
  result.globalX = result.x;
  result.globalY = result.y;

  // TODO(linux): Convert global{X,Y} to screen coordinates.

  // modifiers
  result.modifiers |= TranslateModifiers(mouse_event.modifiers);

  // timestamp
  result.timeStampSeconds = GetSystemUptime();
}
