// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_

#include <string>
#include <vector>

#include "include/cef_client.h"
#include "include/cef_drag_data.h"
#include "include/views/cef_browser_view.h"
#include "include/internal/cef_types.h"
#include "libcef/browser/browser_host_impl.h"

#include "base/callback.h"
#include "content/public/browser/web_contents.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
class WebInputEvent;
}

namespace content {
struct NativeWebKeyboardEvent;
class RenderViewHost;
class RenderViewHostDelegateView;
class WebContentsView;
}

#if defined(USE_AURA)
namespace views {
class Widget;
}
#endif

class CefBrowserInfo;
class CefFileDialogRunner;
class CefJavaScriptDialogRunner;
class CefMenuRunner;

// Provides platform-specific implementations of browser functionality. All
// methods are called on the browser process UI thread unless otherwise
// indicated.
class CefBrowserPlatformDelegate {
 public:
  // Create a new CefBrowserPlatformDelegate instance. May be called on multiple
  // threads.
  static std::unique_ptr<CefBrowserPlatformDelegate> Create(
      CefBrowserHostImpl::CreateParams& create_params);

  // Called to create the view objects for a new WebContents. Will only be
  // called a single time per instance. May be called on multiple threads. Only
  // used with windowless rendering.
  virtual void CreateViewForWebContents(
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view);

  // Called after the WebContents for the browser is created. Will only be
  // called a single time per instance.
  virtual void WebContentsCreated(content::WebContents* web_contents);

  // Called after the RenderViewHost is created.
  virtual void RenderViewCreated(
    content::RenderViewHost* render_view_host);

  // Called after the owning CefBrowserHostImpl is created. Will only be called
  // a single time per instance. Do not send any client notifications from this
  // method.
  virtual void BrowserCreated(CefBrowserHostImpl* browser);

  // Send any notifications related to browser creation. Called after
  // BrowserCreated().
  virtual void NotifyBrowserCreated();

  // Send any notifications related to browser destruction. Called before
  // BrowserDestroyed().
  virtual void NotifyBrowserDestroyed();
  
  // Called before the owning CefBrowserHostImpl is destroyed. Will only be
  // called a single time per instance. All references to the CefBrowserHostImpl
  // and WebContents should be cleared when this method is called. Do not send
  // any client notifications from this method.
  virtual void BrowserDestroyed(CefBrowserHostImpl* browser);

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
  virtual CefWindowHandle GetHostWindowHandle() const = 0;

#if defined(USE_AURA)
  // Returns the Widget owner for the browser window. Only used with windowed
  // rendering.
  virtual views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with views-
  // based browsers.
  virtual CefRefPtr<CefBrowserView> GetBrowserView() const;
#endif  // defined(USE_AURA)

  // Called after the WebContents have been created for a new popup browser
  // parented to this browser but before the CefBrowserHostImpl is created for
  // the popup. |is_devtools| will be true if the popup will host DevTools. This
  // method will be called before WebContentsCreated() is called on
  // |new_platform_delegate|. Do not make the new browser visible in this
  // callback.
  virtual void PopupWebContentsCreated(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* new_web_contents,
      CefBrowserPlatformDelegate* new_platform_delegate,
      bool is_devtools);

  // Called after the CefBrowserHostImpl is created for a new popup browser
  // parented to this browser. |is_devtools| will be true if the popup will host
  // DevTools. This method will be called immediately after
  // CefLifeSpanHandler::OnAfterCreated() for the popup browser. It is safe to
  // make the new browser visible in this callback (for example, add the browser
  // to a window and show it).
  virtual void PopupBrowserCreated(
      CefBrowserHostImpl* new_browser,
      bool is_devtools);

  // Returns the background color for the browser. The alpha component will be
  // either SK_AlphaTRANSPARENT or SK_AlphaOPAQUE (e.g. fully transparent or
  // fully opaque). SK_AlphaOPAQUE will always be returned for windowed
  // browsers. SK_ColorTRANSPARENT may be returned for windowless browsers to
  // enable transparency.
  virtual SkColor GetBackgroundColor() const = 0;

  // Notify the window that it was resized.
  virtual void WasResized() = 0;

  // Send input events.
  virtual void SendKeyEvent(const content::NativeWebKeyboardEvent& event) = 0;
  virtual void SendMouseEvent(const blink::WebMouseEvent& event) = 0;
  virtual void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event) = 0;

  // Send focus event. The browser's WebContents may be NULL when this method is
  // called.
  virtual void SendFocusEvent(bool setFocus) = 0;

  // Send capture lost event.
  virtual void SendCaptureLostEvent();

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  // The window hosting the browser is about to be moved or resized. Only used
  // on Windows and Linux.
  virtual void NotifyMoveOrResizeStarted();

  // Resize the host window to the given dimensions. Only used with windowed
  // rendering on Windows and Linux.
  virtual void SizeTo(int width, int height);
#endif

  // Convert from view coordinates to screen coordinates. Potential display
  // scaling will be applied to the result.
  virtual gfx::Point GetScreenPoint(const gfx::Point& view) const = 0;

  // Open the specified text in the default text editor.
  virtual void ViewText(const std::string& text) = 0;

  // Forward the keyboard event to the application or frame window to allow
  // processing of shortcut keys.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Invoke platform specific handling for the external protocol.
  virtual void HandleExternalProtocol(const GURL& url) = 0;

  // Translate CEF events to Chromium/Blink events.
  virtual void TranslateKeyEvent(content::NativeWebKeyboardEvent& result,
                                 const CefKeyEvent& key_event) const = 0;
  virtual void TranslateClickEvent(blink::WebMouseEvent& result,
                                   const CefMouseEvent& mouse_event,
                                   CefBrowserHost::MouseButtonType type,
                                   bool mouseUp, int clickCount) const = 0;
  virtual void TranslateMoveEvent(blink::WebMouseEvent& result,
                                  const CefMouseEvent& mouse_event,
                                  bool mouseLeave) const = 0;
  virtual void TranslateWheelEvent(blink::WebMouseWheelEvent& result,
                                   const CefMouseEvent& mouse_event,
                                   int deltaX, int deltaY) const = 0;

  // Returns the OS event handle, if any, associated with |event|.
  virtual CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const = 0;

  // Create the platform-specific file dialog runner.
  virtual std::unique_ptr<CefFileDialogRunner> CreateFileDialogRunner();

  // Create the platform-specific JavaScript dialog runner.
  virtual std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner();

  // Create the platform-specific menu runner.
  virtual std::unique_ptr<CefMenuRunner> CreateMenuRunner() = 0;

  // Returns true if this delegate implements windowless rendering. May be
  // called on multiple threads.
  virtual bool IsWindowless() const = 0;

  // Returns true if this delegate implements views-hosted browser handling. May
  // be called on multiple threads.
  virtual bool IsViewsHosted() const = 0;

  // Notify the browser that it was hidden. Only used with windowless rendering.
  virtual void WasHidden(bool hidden);

  // Notify the browser that screen information has changed. Only used with
  // windowless rendering.
  virtual void NotifyScreenInfoChanged();

  // Invalidate the view. Only used with windowless rendering.
  virtual void Invalidate(cef_paint_element_type_t type);

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
      blink::WebDragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& image_offset,
      const content::DragEventSourceInfo& event_info,
      content::RenderWidgetHostImpl* source_rwh);
  virtual void UpdateDragCursor(blink::WebDragOperation operation);
  virtual void DragSourceEndedAt(int x, int y,
                                 cef_drag_operations_mask_t op);
  virtual void DragSourceSystemDragEnded();

 protected:
  // Allow deletion via scoped_ptr only.
  friend std::default_delete<CefBrowserPlatformDelegate>;

  CefBrowserPlatformDelegate();
  virtual ~CefBrowserPlatformDelegate();

  static int TranslateModifiers(uint32 cef_modifiers);

  CefBrowserHostImpl *browser_;  // Not owned by this object.

 private:
  DISALLOW_COPY_AND_ASSIGN(CefBrowserPlatformDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_PLATFORM_DELEGATE_H_
