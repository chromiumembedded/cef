// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_LINUX_FRAME_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_LINUX_FRAME_VIEW_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/views/window_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/frame_buttons.h"
#include "ui/views/window/frame_view.h"

namespace ui {
class LinuxUiTheme;
}

// A frame view for CEF windows on Linux that uses the GTK toolkit's
// WindowFrameProvider for native-looking client-side decorations.
// This replaces the default blue header bar (DefaultFrameView) when
// the compositor does not support server-side decorations.
class LinuxFrameView : public views::FrameView {
  METADATA_HEADER(LinuxFrameView, views::FrameView)

 public:
  LinuxFrameView(views::Widget* widget,
                 base::WeakPtr<CefWindowView> view,
                 ui::LinuxUiTheme* linux_ui_theme);

  LinuxFrameView(const LinuxFrameView&) = delete;
  LinuxFrameView& operator=(const LinuxFrameView&) = delete;

  ~LinuxFrameView() override;

  // views::FrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(views::View::PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnThemeChanged() override;

  gfx::Insets GetFrameBorderInsets() const;

 private:
  ui::WindowFrameProvider* GetFrameProvider() const;
  int GetTopAreaHeight() const;

  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  void PaintFrameBorder(gfx::Canvas* canvas);

  views::ImageButton* InitWindowCaptionButton(
      views::Button::PressedCallback callback,
      int accessibility_string_id,
      int normal_image_id,
      int hot_image_id,
      int pushed_image_id);

  views::ImageButton* GetImageButton(views::FrameButton frame_button);

  void MaybeUpdateCachedNavButtonImages();
  views::Button* GetButtonFromDisplayType(
      ui::NavButtonProvider::FrameButtonDisplayType type);

  raw_ptr<views::Widget> widget_;
  base::WeakPtr<CefWindowView> view_;
  raw_ptr<ui::LinuxUiTheme> linux_ui_theme_;
  std::unique_ptr<ui::NavButtonProvider> nav_button_provider_;

  gfx::Rect client_view_bounds_;
  gfx::Rect title_bounds_;

  raw_ptr<views::ImageButton> minimize_button_ = nullptr;
  raw_ptr<views::ImageButton> maximize_button_ = nullptr;
  raw_ptr<views::ImageButton> restore_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  int minimum_title_bar_x_ = 0;
  int maximum_title_bar_x_ = -1;

  struct NavButtonCacheParams {
    bool operator==(const NavButtonCacheParams& other) const;
    int top_area_height;
    bool maximized;
    bool active;
  };
  std::optional<NavButtonCacheParams> nav_button_cache_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_LINUX_FRAME_VIEW_H_
