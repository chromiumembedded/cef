// Copyright (c) 2021 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_overlay_controls.h"

#include <algorithm>
#include <string>

#include "include/views/cef_box_layout.h"
#include "include/views/cef_window.h"
#include "tests/cefclient/browser/views_style.h"

namespace client {

namespace {

constexpr int kInsets = 4;
constexpr int kLocationBarPadding = 100;

// White with 80% opacity.
constexpr auto kBackgroundColor = CefColorSetARGB(255 * .80, 255, 255, 255);

std::string GetLabel(ViewsOverlayControls::Command command, bool maximized) {
  switch (command) {
    case ViewsOverlayControls::Command::kMinimize:
      return "-";
    case ViewsOverlayControls::Command::kMaximize:
      return maximized ? "O" : "o";
    case ViewsOverlayControls::Command::kClose:
      return "X";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

ViewsOverlayControls::ViewsOverlayControls() = default;

void ViewsOverlayControls::Initialize(CefRefPtr<CefWindow> window,
                                      CefRefPtr<CefMenuButton> menu_button,
                                      CefRefPtr<CefView> location_bar,
                                      bool is_chrome_toolbar) {
  DCHECK(!window_);
  DCHECK(menu_button);
  DCHECK(location_bar);

  window_ = window;
  window_maximized_ = window_->IsMaximized();

  // Window control buttons. These controls are currently text which means that
  // we can't use a transparent background because subpixel text rendering will
  // break. See comments on the related DCHECK in Label::PaintText.
  panel_ = CefPanel::CreatePanel(nullptr);
  views_style::ApplyTo(panel_);

  // Use a horizontal box layout.
  CefBoxLayoutSettings panel_layout_settings;
  panel_layout_settings.horizontal = true;
  panel_->SetToBoxLayout(panel_layout_settings);

  panel_->AddChildView(CreateButton(ViewsOverlayControls::Command::kMinimize));
  panel_->AddChildView(CreateButton(ViewsOverlayControls::Command::kMaximize));
  panel_->AddChildView(CreateButton(ViewsOverlayControls::Command::kClose));

  panel_controller_ =
      window->AddOverlayView(panel_, CEF_DOCKING_MODE_TOP_RIGHT);
  panel_controller_->SetVisible(true);

  // Menu button.
  menu_button->SetBackgroundColor(kBackgroundColor);
  menu_controller_ =
      window_->AddOverlayView(menu_button, CEF_DOCKING_MODE_TOP_LEFT);
  menu_controller_->SetInsets(CefInsets(kInsets, kInsets, 0, 0));
  menu_controller_->SetVisible(true);

  // Location bar. Will be made visible in UpdateControls().
  location_bar_ = location_bar;
  is_chrome_toolbar_ = is_chrome_toolbar;
  // Use a 100% transparent background for the Chrome toolbar.
  location_bar_->SetBackgroundColor(is_chrome_toolbar_ ? 0 : kBackgroundColor);
  location_controller_ =
      window_->AddOverlayView(location_bar_, CEF_DOCKING_MODE_CUSTOM);
}

void ViewsOverlayControls::Destroy() {
  window_ = nullptr;
  panel_ = nullptr;
  panel_controller_->Destroy();
  panel_controller_ = nullptr;
  menu_controller_->Destroy();
  menu_controller_ = nullptr;
  location_bar_ = nullptr;
  location_controller_->Destroy();
  location_controller_ = nullptr;
}

void ViewsOverlayControls::UpdateControls() {
  // Update location bar size, position and visibility.
  auto bounds = window_->GetBounds();
  bounds.x = kLocationBarPadding;
  bounds.width -= kLocationBarPadding * 2;
  bounds.y = kInsets;
  if (is_chrome_toolbar_) {
    // Fit the standard Chrome toolbar.
    const auto preferred_size = location_bar_->GetPreferredSize();
    bounds.height =
        std::max(menu_controller_->GetSize().height, preferred_size.height);
  } else {
    bounds.height = menu_controller_->GetSize().height;
  }
  if (bounds.width < kLocationBarPadding * 2) {
    // Not enough space.
    location_controller_->SetVisible(false);
  } else {
    location_bar_->SetSize(CefSize(bounds.width, bounds.height));
    location_controller_->SetBounds(bounds);
    location_controller_->SetVisible(true);
  }

  MaybeUpdateMaximizeButton();
}

void ViewsOverlayControls::UpdateDraggableRegions(
    std::vector<CefDraggableRegion>& window_regions) {
  if (panel_controller_ && panel_controller_->IsVisible()) {
    window_regions.push_back(CefDraggableRegion(panel_controller_->GetBounds(),
                                                /*draggable=*/false));
  }

  if (menu_controller_ && menu_controller_->IsVisible()) {
    window_regions.push_back(
        CefDraggableRegion(menu_controller_->GetBounds(), /*draggable=*/false));
  }

  if (location_controller_ && location_controller_->IsVisible()) {
    window_regions.push_back(CefDraggableRegion(
        location_controller_->GetBounds(), /*draggable=*/false));
  }
}

void ViewsOverlayControls::OnButtonPressed(CefRefPtr<CefButton> button) {
  auto command = static_cast<Command>(button->GetID());
  switch (command) {
    case ViewsOverlayControls::Command::kMinimize:
      window_->Minimize();
      break;
    case ViewsOverlayControls::Command::kMaximize:
      if (window_->IsMaximized())
        window_->Restore();
      else
        window_->Maximize();
      break;
    case ViewsOverlayControls::Command::kClose:
      window_->Close();
      return;
  }

  // Explicitly reset button state because the button may have moved and it
  // won't receive the corresponding mouse move events.
  button->SetState(CEF_BUTTON_STATE_NORMAL);
  button->SetInkDropEnabled(false);
  button->SetInkDropEnabled(true);

  if (command == Command::kMaximize)
    MaybeUpdateMaximizeButton();
}

CefRefPtr<CefLabelButton> ViewsOverlayControls::CreateButton(Command command) {
  CefRefPtr<CefLabelButton> button = CefLabelButton::CreateLabelButton(
      this, GetLabel(command, window_maximized_));
  button->SetID(static_cast<int>(command));
  views_style::ApplyTo(button);
  button->SetInkDropEnabled(true);
  button->SetFocusable(false);  // Don't give focus to the button.
  return button;
}

void ViewsOverlayControls::MaybeUpdateMaximizeButton() {
  if (window_->IsMaximized() == window_maximized_)
    return;
  window_maximized_ = !window_maximized_;

  auto max_button = panel_->GetChildViewAt(1);
  auto command = static_cast<Command>(max_button->GetID());
  DCHECK(command == Command::kMaximize);
  max_button->AsButton()->AsLabelButton()->SetText(
      GetLabel(command, window_maximized_));

  // Adjust overlay size and placement due to layout changing.
  panel_controller_->SizeToPreferredSize();
}

}  // namespace client
