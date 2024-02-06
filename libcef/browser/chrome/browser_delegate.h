// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
#pragma once

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class Profile;

namespace cef {

// Delegate for the chrome Browser object. Lifespan is controlled by the Browser
// object. See the ChromeBrowserDelegate documentation for additional details.
// Only accessed on the UI thread.
class BrowserDelegate : public content::WebContentsDelegate {
 public:
  // Opaque ref-counted base class for CEF-specific parameters passed via
  // Browser::CreateParams::cef_params and possibly shared by multiple Browser
  // instances.
  class CreateParams : public base::RefCounted<CreateParams> {
   public:
    virtual ~CreateParams() = default;
  };

  // Called from the Browser constructor to create a new delegate.
  static std::unique_ptr<BrowserDelegate> Create(
      Browser* browser,
      scoped_refptr<CreateParams> cef_params,
      const Browser* opener);

  ~BrowserDelegate() override = default;

  // Optionally override Browser creation in
  // DevToolsWindow::CreateDevToolsBrowser. The returned Browser, if any, will
  // take ownership of |devtools_contents|.
  virtual Browser* CreateDevToolsBrowser(
      Profile* profile,
      Browser* opener,
      std::unique_ptr<content::WebContents>& devtools_contents) = 0;

  // Optionally override chrome::AddWebContents behavior. This is most often
  // called via Browser::AddNewContents for new popup browsers and provides an
  // opportunity for CEF to create a new Browser instead of proceeding with
  // default Browser or tab creation.
  virtual std::unique_ptr<content::WebContents> AddWebContents(
      std::unique_ptr<content::WebContents> new_contents) = 0;

  // Called immediately after |new_contents| is created via chrome::Navigate.
  // This is most often called for navigations targeting a new tab without a
  // pre-existing WebContents.
  virtual void OnWebContentsCreated(content::WebContents* new_contents) = 0;

  // Add or remove ownership of the WebContents.
  virtual void SetAsDelegate(content::WebContents* web_contents,
                             bool set_delegate) = 0;

  // Return true to show the status bubble. This should consistently return the
  // same value for the lifespan of a Browser.
  virtual bool ShowStatusBubble(bool show_by_default) {
    return show_by_default;
  }

  // Return true to handle (or disable) a command. ID values come from
  // chrome/app/chrome_command_ids.h.
  virtual bool HandleCommand(int command_id,
                             WindowOpenDisposition disposition) {
    return false;
  }

  // Return true if the app menu item should be visible. ID values come from
  // chrome/app/chrome_command_ids.h.
  virtual bool IsAppMenuItemVisible(int command_id) { return true; }

  // Return true if the app menu item should be enabled. ID values come from
  // chrome/app/chrome_command_ids.h.
  virtual bool IsAppMenuItemEnabled(int command_id) { return true; }

  // Return true if the page action icon should be visible.
  virtual bool IsPageActionIconVisible(PageActionIconType icon_type) {
    return true;
  }

  enum class ToolbarButtonType {
    kCast = 0,
    kDownload,
    kSendTabToSelf,
    kSidePanel,
    kMaxValue = kSidePanel,
  };

  // Return true if the toolbar button should be visible.
  virtual bool IsToolbarButtonVisible(ToolbarButtonType button_type) {
    return true;
  }

  // Optionally modify the bounding box for the Find bar.
  virtual void UpdateFindBarBoundingBox(gfx::Rect* bounds) {}

  // Optionally modify the top inset for dialogs.
  virtual void UpdateDialogTopInset(int* dialog_top_y) {}

  // Same as RequestMediaAccessPermission but returning |callback| if the
  // request is unhandled.
  [[nodiscard]] virtual content::MediaResponseCallback
  RequestMediaAccessPermissionEx(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback) {
    return callback;
  }

  // Optionally override support for the specified window feature of type
  // Browser::WindowFeature.
  virtual std::optional<bool> SupportsWindowFeature(int feature) const {
    return std::nullopt;
  }

  // Returns true if draggable regions are supported.
  virtual bool SupportsDraggableRegion() const { return false; }

  // Returns the draggable region, if any, relative to the web contents.
  // Called from PictureInPictureBrowserFrameView::NonClientHitTest and
  // BrowserView::ShouldDescendIntoChildForEventHandling.
  virtual const std::optional<SkRegion> GetDraggableRegion() const {
    return std::nullopt;
  }

  // Set the draggable region relative to web contents.
  // Called from DraggableRegionsHostImpl::UpdateDraggableRegions.
  virtual void UpdateDraggableRegion(const SkRegion& region) {}

  // Called at the end of a fullscreen transition.
  virtual void WindowFullscreenStateChanged() {}

  // Returns true if this browser has a Views-hosted opener. Only
  // applicable for Browsers of type picture_in_picture and devtools.
  virtual bool HasViewsHostedOpener() const { return false; }
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
