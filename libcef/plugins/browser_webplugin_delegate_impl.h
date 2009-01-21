// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_WEBPLUGIN_DELEGATE_IMPL_H
#define _BROWSER_WEBPLUGIN_DELEGATE_IMPL_H

#include <string>
#include <list>

#include "browser_plugin_lib.h"

#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/iat_patch.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/webplugin_delegate.h"
#include "webkit/glue/webcursor.h"

namespace NPAPI {
  class BrowserPluginInstance;
};

// An implementation of WebPluginDelegate that proxies all calls to
// the plugin process.
class BrowserWebPluginDelegateImpl : public WebPluginDelegate {
 public:
  static BrowserWebPluginDelegateImpl* Create(const struct CefPluginInfo& plugin_info,
                                              const std::string& mime_type,
                                              gfx::NativeView containing_view);
  static bool IsPluginDelegateWindow(HWND window);
  static bool GetPluginNameFromWindow(HWND window, std::wstring *plugin_name);

  // Returns true if the window handle passed in is that of the dummy
  // activation window for windowless plugins.
  static bool IsDummyActivationWindow(HWND window);

  // WebPluginDelegate implementation
  virtual void PluginDestroyed();
  virtual bool Initialize(const GURL& url,
                          char** argn,
                          char** argv,
                          int argc,
                          WebPlugin* plugin,
                          bool load_manually);
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(HDC hdc, const gfx::Rect& rect);
  virtual void Print(HDC hdc);
  virtual void SetFocus();  // only called when windowless
// only called when windowless
  virtual bool HandleEvent(NPEvent* event,
                           WebCursor* cursor);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(NPReason reason);
  virtual int GetProcessId();

  virtual void FlushGeometryUpdates() {
  }
  virtual void SendJavaScriptStream(const std::string& url,
                                    const std::wstring& result,
                                    bool success, bool notify_needed,
                                    int notify_data);
  virtual void DidReceiveManualResponse(const std::string& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual FilePath GetPluginPath();
  virtual void InstallMissingPlugin();
  virtual WebPluginResourceClient* CreateResourceClient(int resource_id,
                                                        const std::string &url,
                                                        bool notify_needed,
                                                        void *notify_data,
                                                        void* stream);

  virtual void URLRequestRouted(const std::string&url, bool notify_needed,
                                void* notify_data);
  bool windowless() const { return windowless_ ; }
  gfx::Rect rect() const { return window_rect_; }
  gfx::Rect clip_rect() const { return clip_rect_; }

  enum PluginQuirks {
    PLUGIN_QUIRK_SETWINDOW_TWICE = 1,
    PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE = 2,
    PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY = 4,
    PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY = 8,
    PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES = 16,
    PLUGIN_QUIRK_DIE_AFTER_UNLOAD = 32,
    PLUGIN_QUIRK_PATCH_TRACKPOPUP_MENU = 64,
    PLUGIN_QUIRK_PATCH_SETCURSOR = 128,
	PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS = 256,
  };

  int quirks() { return quirks_; }

 private:
  BrowserWebPluginDelegateImpl(gfx::NativeView containing_view,
                               NPAPI::BrowserPluginInstance *instance);
  ~BrowserWebPluginDelegateImpl();

  //--------------------------
  // used for windowed plugins
  void WindowedUpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  // Create the native window. 
  // Returns true if the window is created (or already exists).
  // Returns false if unable to create the window.
  bool WindowedCreatePlugin();

  // Destroy the native window.
  void WindowedDestroyWindow();

  // Reposition the native window to be in sync with the given geometry.
  // Returns true if the native window has moved or been clipped differently.
  bool WindowedReposition(const gfx::Rect& window_rect,
                          const gfx::Rect& clip_rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowedSetWindow();

  // Registers the window class for our window
  ATOM RegisterNativeWindowClass();

  // Our WndProc functions.
  static LRESULT CALLBACK DummyWindowProc(
      HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK NativeWndProc(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK FlashWindowlessWndProc(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Used for throttling Flash messages.
  static void ClearThrottleQueueForWindow(HWND window);
  static void OnThrottleMessage();
  static void ThrottleMessage(WNDPROC proc, HWND hwnd, UINT message,
      WPARAM wParam, LPARAM lParam);

  //----------------------------
  // used for windowless plugins
  void WindowlessUpdateGeometry(const gfx::Rect& window_rect,
                                const gfx::Rect& clip_rect);
  void WindowlessPaint(HDC hdc, const gfx::Rect& rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowlessSetWindow(bool force_set_window);


  //-----------------------------------------
  // used for windowed and windowless plugins

  NPAPI::BrowserPluginInstance* instance() { return instance_.get(); }

  // Closes down and destroys our plugin instance.
  void DestroyInstance();

  // used for windowed plugins
  HWND windowed_handle_;
  bool windowed_did_set_window_;
  gfx::Rect windowed_last_pos_;

  // this is an optimization to avoid calling SetWindow to the plugin 
  // when it is not necessary.  Initially, we need to call SetWindow,
  // and after that we only need to call it when the geometry changes.
  // use this flag to indicate whether we really need it or not.
  bool windowless_needs_set_window_;

  // used by windowed and windowless plugins
  bool windowless_;

  WebPlugin* plugin_;
  scoped_refptr<NPAPI::BrowserPluginInstance> instance_;

  // Original wndproc before we subclassed.
  WNDPROC plugin_wnd_proc_;

  // Used to throttle WM_USER+1 messages in Flash.
  uint32 last_message_;
  bool is_calling_wndproc;

  HWND parent_;
  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  std::vector<gfx::Rect> cutout_rects_;
  int quirks_;

  // Windowless plugins don't have keyboard focus causing issues with the
  // plugin not receiving keyboard events if the plugin enters a modal 
  // loop like TrackPopupMenuEx or MessageBox, etc.
  // This is a basic issue with windows activation and focus arising due to
  // the fact that these windows are created by different threads. Activation
  // and focus are thread specific states, and if the browser has focus,
  // the plugin may not have focus. 
  // To fix a majority of these activation issues we create a dummy visible 
  // child window to which we set focus whenever the windowless plugin 
  // receives a WM_LBUTTONDOWN/WM_RBUTTONDOWN message via NPP_HandleEvent.
  HWND dummy_window_for_activation_;
  bool CreateDummyWindowForActivation();

  static std::list<MSG> throttle_queue_;

  // Returns true if the event passed in needs to be tracked for a potential
  // modal loop.
  static bool ShouldTrackEventForModalLoops(NPEvent* event);

  // The message filter hook procedure, which tracks modal loops entered by
  // a plugin in the course of a NPP_HandleEvent call.
  static LRESULT CALLBACK HandleEventMessageFilterHook(int code, WPARAM wParam,
                                                       LPARAM lParam);

  // Called by the message filter hook when the plugin enters a modal loop.
  void OnModalLoopEntered();

  // Returns true if the message passed in corresponds to a user gesture.
  static bool IsUserGestureMessage(unsigned int message);

  // Indicates the end of a user gesture period.
  void OnUserGestureEnd();

  // Handle to the message filter hook
  HHOOK handle_event_message_filter_hook_;

  // The current instance of the plugin which entered the modal loop.
  static BrowserWebPluginDelegateImpl* current_plugin_instance_;

  // Event which is set when the plugin enters a modal loop in the course
  // of a NPP_HandleEvent call.
  HANDLE handle_event_pump_messages_event_;

  // Holds the depth of the HandleEvent callstack.
  int handle_event_depth_;

  // This flag indicates whether we started tracking a user gesture message.
  bool user_gesture_message_posted_;

  // Runnable Method Factory used to invoke the OnUserGestureEnd method
  // asynchronously.
  ScopedRunnableMethodFactory<BrowserWebPluginDelegateImpl> user_gesture_msg_factory_;

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

  // The plugin module handle.
  HMODULE plugin_module_handle_;

  // Helper object for patching the TrackPopupMenu API
  static iat_patch::IATPatchFunction iat_patch_track_popup_menu_;

  // TrackPopupMenu interceptor. Parameters are the same as the Win32 function
  // TrackPopupMenu.
  static BOOL WINAPI TrackPopupMenuPatch(HMENU menu, unsigned int flags, int x,
                                         int y, int reserved, HWND window,
                                         const RECT* rect);

  // SetCursor interceptor for windowless plugins.
  static HCURSOR WINAPI SetCursorPatch(HCURSOR cursor);

  // Helper object for patching the SetCursor API
  static iat_patch::IATPatchFunction iat_patch_set_cursor_;

  // Holds the current cursor set by the windowless plugin.
  WebCursor current_windowless_cursor_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserWebPluginDelegateImpl);
};

#endif  // #ifndef _BROWSER_WEBPLUGIN_DELEGATE_IMPL_H
