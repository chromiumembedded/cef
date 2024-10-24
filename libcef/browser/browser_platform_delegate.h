// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "cef/include/cef_client.h"
#include "cef/include/cef_drag_data.h"
#include "cef/include/internal/cef_types.h"
#include "cef/include/views/cef_browser_view.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebInputEvent;
class WebTouchEvent;

namespace mojom {
class WindowFeatures;
}
}  // namespace blink

namespace content {
struct DropData;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderWidgetHostImpl;
class WebContents;
class WebContentsView;
}  // namespace content

namespace gfx {
class ImageSkia;
class Point;
class Rect;
class Size;
class Vector2d;
}  // namespace gfx

namespace input {
struct NativeWebKeyboardEvent;
}

namespace ui {
class AXTreeID;
struct AXLocationAndScrollUpdates;
struct AXUpdatesAndEvents;
}  // namespace ui

namespace views {
class Widget;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

struct CefBrowserCreateParams;
class CefBrowserHostBase;
class CefJavaScriptDialogRunner;
class CefMenuRunner;

// Provides platform-specific implementations of browser functionality. All
// methods are called on the browser process UI thread unless otherwise
// indicated.
class CefBrowserPlatformDelegate {
 public:
  CefBrowserPlatformDelegate(const CefBrowserPlatformDelegate&) = delete;
  CefBrowserPlatformDelegate& operator=(const CefBrowserPlatformDelegate&) =
      delete;

  // Create a new CefBrowserPlatformDelegate instance. May be called on multiple
  // threads.
  static std::unique_ptr<CefBrowserPlatformDelegate> Create(
      const CefBrowserCreateParams& create_params);

  // Called from BrowserHost::Create.
  // Wait for the call to WebContentsCreated(owned=true) before taking ownership
  // of the resulting WebContents object.
  virtual content::WebContents* CreateWebContents(
      CefBrowserCreateParams& create_params,
      bool& own_web_contents);

  // Called to create the view objects for a new WebContents. Will only be
  // called a single time per instance. May be called on multiple threads. Only
  // used with windowless rendering.
  virtual void CreateViewForWebContents(
      raw_ptr<content::WebContentsView>* view,
      raw_ptr<content::RenderViewHostDelegateView>* delegate_view);

  // Called after the WebContents for a browser has been created. |owned| will
  // be true if |web_contents| was created via CreateWebContents() and we should
  // take ownership. This will also be called for popup WebContents created
  // indirectly by Chromium. Will only be called a single time per instance.
  virtual void WebContentsCreated(content::WebContents* web_contents,
                                  bool owned);

  // Called when Chromium is ready to hand over ownership of a popup
  // WebContents. WebContentsCreated(owned=false) will be called first for
  // |new_contents|. Will only be called a single time per instance. See also
  // the WebContentsDelegate documentation.
  virtual void AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked);

  // Called when the WebContents is destroyed. This will be called before
  // BrowserDestroyed(). Will only be called a single time per instance.
  virtual void WebContentsDestroyed(content::WebContents* web_contents);

  // Called after the RenderViewHost is created.
  virtual void RenderViewCreated(content::RenderViewHost* render_view_host);

  // See WebContentsObserver documentation.
  virtual void RenderViewReady();

  // Called after the owning BrowserHost is created. Will only be
  // called a single time per instance. Do not send any client notifications
  // from this method.
  virtual void BrowserCreated(CefBrowserHostBase* browser);

  // Send any notifications related to browser creation. Called after
  // BrowserCreated().
  virtual void NotifyBrowserCreated();

  // Send any notifications related to browser destruction. Called before
  // BrowserDestroyed().
  virtual void NotifyBrowserDestroyed();

  // Called before the owning BrowserHost is destroyed. Will only be
  // called a single time per instance. All references to the
  // BrowserHost and WebContents should be cleared when this method is
  // called. Do not send any client notifications from this method.
  virtual void BrowserDestroyed(CefBrowserHostBase* browser);

  // Create the window that hosts the browser. Will only be called a single time
  // per instance. Only used with windowed rendering.
  virtual bool CreateHostWindow();

  // Sends a message to close the window that hosts the browser. On native
  // platforms this will be done via the OS. DestroyBrowser will be called after
  // the native window has closed. Only used with windowed rendering.
  virtual void CloseHostWindow();

  // Return the OS handle for the window that hosts the browser. For windowed
  // rendering this will return the most immediate parent window handle. For
  // windowless rendering this will return the parent window handle specified by
  // the client, which may be NULL. May be called on multiple threads.
  virtual CefWindowHandle GetHostWindowHandle() const;

  // Returns the Widget owner for the browser window. Only used with windowed
  // rendering.
  virtual views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with Views-
  // based browsers.
  virtual CefRefPtr<CefBrowserView> GetBrowserView() const;

  // Sets the BrowserView associated with this browser. Only used with
  // Views-based browsers.
  virtual void SetBrowserView(CefRefPtr<CefBrowserView> browser_view);

  // Returns the WebContentsModalDialogHost associated with this browser.
  virtual web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      const;

  // Called after the WebContents have been created for a new popup browser
  // parented to this browser but before the BrowserHost is created for the
  // popup. |is_devtools| will be true if the popup will host DevTools. This
  // method will be called before WebContentsCreated() is called on
  // |new_platform_delegate|. Does not make the new browser visible in this
  // callback.
  void PopupWebContentsCreated(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* new_web_contents,
      CefBrowserPlatformDelegate* new_platform_delegate,
      bool is_devtools);

  // Called after the BrowserHost is created for a new popup browser parented to
  // this browser. |is_devtools| will be true if the popup will host DevTools.
  // This method will be called immediately after
  // CefLifeSpanHandler::OnAfterCreated() for the popup browser. It is safe to
  // make the new browser visible in this callback (for example, add the browser
  // to a window and show it).
  void PopupBrowserCreated(CefBrowserPlatformDelegate* new_platform_delegate,
                           CefBrowserHostBase* new_browser,
                           bool is_devtools);

  // Called from PopupWebContentsCreated/PopupBrowserCreated to retrieve the
  // default BrowserViewDelegate in cases where this is a new Views-based popup
  // and the opener is either not Views-based or doesn't implement the
  // BrowserViewDelegate. Only implemented for specific configurations where
  // special handling of new popups is required for proper functioning.
  virtual CefRefPtr<CefBrowserViewDelegate>
  GetDefaultBrowserViewDelegateForPopupOpener();

  // Returns the background color for the browser. The alpha component will be
  // either SK_AlphaTRANSPARENT or SK_AlphaOPAQUE (e.g. fully transparent or
  // fully opaque). SK_AlphaOPAQUE will always be returned for windowed
  // browsers. SK_ColorTRANSPARENT may be returned for windowless browsers to
  // enable transparency.
  virtual SkColor GetBackgroundColor() const;

  // Notify the window that it was resized.
  virtual void WasResized();

  // Send input events.
  virtual void SendKeyEvent(const CefKeyEvent& event);
  virtual void SendMouseClickEvent(const CefMouseEvent& event,
                                   CefBrowserHost::MouseButtonType type,
                                   bool mouseUp,
                                   int clickCount);
  virtual void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave);
  virtual void SendMouseWheelEvent(const CefMouseEvent& event,
                                   int deltaX,
                                   int deltaY);
  virtual void SendTouchEvent(const CefTouchEvent& event);

  // Send focus event. The browser's WebContents may be NULL when this method is
  // called.
  virtual void SetFocus(bool setFocus);

  // Send capture lost event.
  virtual void SendCaptureLostEvent();

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  // The window hosting the browser is about to be moved or resized. Only used
  // on Windows and Linux.
  virtual void NotifyMoveOrResizeStarted();

  // Resize the host window to the given dimensions. Only used with windowed
  // rendering on Windows and Linux.
  virtual void SizeTo(int width, int height);
#endif

  // Convert from view DIP coordinates to screen coordinates. If
  // |want_dip_coords| is true return DIP instead of device (pixel) coordinates
  // on Windows/Linux.
  virtual gfx::Point GetScreenPoint(const gfx::Point& view,
                                    bool want_dip_coords) const;

  // Open the specified text in the default text editor.
  virtual void ViewText(const std::string& text);

  // Forward the keyboard event to the application or frame window to allow
  // processing of shortcut keys.
  virtual bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event);

  // Invoke platform specific handling for the external protocol.
  static void HandleExternalProtocol(const GURL& url);

  // Returns the OS event handle, if any, associated with |event|.
  virtual CefEventHandle GetEventHandle(
      const input::NativeWebKeyboardEvent& event) const;

  // Create the platform-specific JavaScript dialog runner.
  virtual std::unique_ptr<CefJavaScriptDialogRunner>
  CreateJavaScriptDialogRunner();

  // Create the platform-specific menu runner.
  virtual std::unique_ptr<CefMenuRunner> CreateMenuRunner();

  // Returns true if this delegate implements windowless rendering. May be
  // called on multiple threads.
  virtual bool IsWindowless() const;

  // Returns true if this delegate implements views-hosted browser handling. May
  // be called on multiple threads.
  virtual bool IsViewsHosted() const;

  // Returns the runtime style implemented by this delegate. May be called on
  // multiple threads.
  virtual bool IsAlloyStyle() const = 0;
  bool IsChromeStyle() const { return !IsAlloyStyle(); }

  // Returns true if this delegate implements a browser with external
  // (client-provided) parent window. May be called on multiple threads.
  virtual bool HasExternalParent() const;

  // Notify the browser that it was hidden. Only used with windowless rendering.
  virtual void WasHidden(bool hidden);

  // Returns true if the browser is currently hidden. Only used with windowless
  // rendering.
  virtual bool IsHidden() const;

  // Notify the browser that screen information has changed. Only used with
  // windowless rendering.
  virtual void NotifyScreenInfoChanged();

  // Invalidate the view. Only used with windowless rendering.
  virtual void Invalidate(cef_paint_element_type_t type);

  // Send the external begin frame message. Only used with windowless rendering.
  virtual void SendExternalBeginFrame();

  // Set the windowless frame rate. Only used with windowless rendering.
  virtual void SetWindowlessFrameRate(int frame_rate);

  // IME-related callbacks. See documentation in CefBrowser and
  // CefRenderHandler. Only used with windowless rendering.
  virtual void ImeSetComposition(
      const CefString& text,
      const std::vector<CefCompositionUnderline>& underlines,
      const CefRange& replacement_range,
      const CefRange& selection_range);
  virtual void ImeCommitText(const CefString& text,
                             const CefRange& replacement_range,
                             int relative_cursor_pos);
  virtual void ImeFinishComposingText(bool keep_selection);
  virtual void ImeCancelComposition();

  // Drag/drop-related callbacks. See documentation in CefRenderHandler. Only
  // used with windowless rendering.
  virtual void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                                   const CefMouseEvent& event,
                                   cef_drag_operations_mask_t allowed_ops);
  virtual void DragTargetDragOver(const CefMouseEvent& event,
                                  cef_drag_operations_mask_t allowed_ops);
  virtual void DragTargetDragLeave();
  virtual void DragTargetDrop(const CefMouseEvent& event);
  virtual void StartDragging(
      const content::DropData& drop_data,
      blink::DragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& image_offset,
      const blink::mojom::DragEventSourceInfo& event_info,
      content::RenderWidgetHostImpl* source_rwh);
  virtual void UpdateDragOperation(ui::mojom::DragOperation operation,
                                   bool document_is_handling_drag);
  virtual void DragSourceEndedAt(int x, int y, cef_drag_operations_mask_t op);
  virtual void DragSourceSystemDragEnded();
  virtual void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details);
  virtual void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details);
  virtual gfx::Point GetDialogPosition(const gfx::Size& size);
  virtual gfx::Size GetMaximumDialogSize();

  // See CefBrowserHost documentation.
  virtual void SetAutoResizeEnabled(bool enabled,
                                    const CefSize& min_size,
                                    const CefSize& max_size);
  virtual void SetAccessibilityState(cef_state_t accessibility_state);
  virtual bool IsPrintPreviewSupported() const;
  virtual void Find(const CefString& searchText,
                    bool forward,
                    bool matchCase,
                    bool findNext);
  virtual void StopFinding(bool clearSelection);

 protected:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<CefBrowserPlatformDelegate>;

  CefBrowserPlatformDelegate();
  virtual ~CefBrowserPlatformDelegate();

  static int TranslateWebEventModifiers(uint32_t cef_modifiers);

  // Not owned by this object.
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<CefBrowserHostBase> browser_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_
