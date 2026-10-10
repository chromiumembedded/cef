// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/linux_frame_view.h"

#include <algorithm>
#include <vector>

#include "base/containers/adapters.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/linux/linux_ui.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_button_order_provider.h"

namespace {

constexpr int kResizeAreaCornerSize = 16;
constexpr int kTitleCaptionSpacing = 5;
constexpr int kTitleIconOffsetX = 4;
constexpr int kIconLeftSpacing = 2;
constexpr int kCaptionButtonHeightWithPadding = 19;

void LayoutButton(views::ImageButton* button, const gfx::Rect& bounds) {
  button->SetVisible(true);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
  button->SetBoundsRect(bounds);
}

ui::NavButtonProvider::ButtonState ToNavButtonState(
    views::Button::ButtonState state) {
  switch (state) {
    case views::Button::STATE_NORMAL:
      return ui::NavButtonProvider::ButtonState::kNormal;
    case views::Button::STATE_HOVERED:
      return ui::NavButtonProvider::ButtonState::kHovered;
    case views::Button::STATE_PRESSED:
      return ui::NavButtonProvider::ButtonState::kPressed;
    case views::Button::STATE_DISABLED:
      return ui::NavButtonProvider::ButtonState::kDisabled;
    case views::Button::STATE_COUNT:
    default:
      NOTREACHED();
  }
}

}  // namespace

bool LinuxFrameView::NavButtonCacheParams::operator==(
    const NavButtonCacheParams& other) const {
  return top_area_height == other.top_area_height &&
         maximized == other.maximized && active == other.active;
}

LinuxFrameView::LinuxFrameView(views::Widget* widget,
                               base::WeakPtr<CefWindowView> view,
                               ui::LinuxUiTheme* linux_ui_theme)
    : widget_(widget),
      view_(std::move(view)),
      linux_ui_theme_(linux_ui_theme),
      nav_button_provider_(linux_ui_theme->CreateNavButtonProvider()) {
  if (!nav_button_provider_) {
    // Fall back to Chromium's built-in caption button images.
    close_button_ = InitWindowCaptionButton(
        base::BindRepeating(&views::Widget::CloseWithReason,
                            base::Unretained(widget_),
                            views::Widget::ClosedReason::kCloseButtonClicked),
        IDS_APP_ACCNAME_CLOSE, IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P);
    minimize_button_ = InitWindowCaptionButton(
        base::BindRepeating(&views::Widget::Minimize,
                            base::Unretained(widget_)),
        IDS_APP_ACCNAME_MINIMIZE, IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P);
    maximize_button_ = InitWindowCaptionButton(
        base::BindRepeating(&views::Widget::Maximize,
                            base::Unretained(widget_)),
        IDS_APP_ACCNAME_MAXIMIZE, IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P);
    restore_button_ = InitWindowCaptionButton(
        base::BindRepeating(&views::Widget::Restore,
                            base::Unretained(widget_)),
        IDS_APP_ACCNAME_RESTORE, IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P);
  } else {
    // GTK-themed nav buttons. Create ImageButton children that we'll
    // populate with images from the NavButtonProvider.
    close_button_ = AddChildView(std::make_unique<views::ImageButton>(
        base::BindRepeating(&views::Widget::CloseWithReason,
                            base::Unretained(widget_),
                            views::Widget::ClosedReason::kCloseButtonClicked)));
    close_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    close_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));

    minimize_button_ = AddChildView(std::make_unique<views::ImageButton>(
        base::BindRepeating(&views::Widget::Minimize,
                            base::Unretained(widget_))));
    minimize_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    minimize_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));

    maximize_button_ = AddChildView(std::make_unique<views::ImageButton>(
        base::BindRepeating(&views::Widget::Maximize,
                            base::Unretained(widget_))));
    maximize_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    maximize_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));

    restore_button_ = AddChildView(std::make_unique<views::ImageButton>(
        base::BindRepeating(&views::Widget::Restore,
                            base::Unretained(widget_))));
    restore_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    restore_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_RESTORE));
  }

  paint_as_active_subscription_ =
      widget_->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &LinuxFrameView::SchedulePaint, base::Unretained(this)));
}

LinuxFrameView::~LinuxFrameView() = default;

gfx::Rect LinuxFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect LinuxFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Insets insets = GetFrameBorderInsets();
  int top_height = GetTopAreaHeight();
  return gfx::Rect(client_bounds.x() - insets.left(),
                    client_bounds.y() - top_height,
                    client_bounds.width() + insets.left() + insets.right(),
                    client_bounds.height() + top_height + insets.bottom());
}

int LinuxFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point)) {
    return HTNOWHERE;
  }

  int frame_component = widget_->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE) {
    return frame_component;
  }

  if (close_button_ && close_button_->GetVisible() &&
      close_button_->GetMirroredBounds().Contains(point)) {
    return HTCLOSE;
  }
  if (restore_button_ && restore_button_->GetVisible() &&
      restore_button_->GetMirroredBounds().Contains(point)) {
    return HTMAXBUTTON;
  }
  if (maximize_button_ && maximize_button_->GetVisible() &&
      maximize_button_->GetMirroredBounds().Contains(point)) {
    return HTMAXBUTTON;
  }
  if (minimize_button_ && minimize_button_->GetVisible() &&
      minimize_button_->GetMirroredBounds().Contains(point)) {
    return HTMINBUTTON;
  }

  // Test for draggable region.
  if (view_) {
    SkRegion* draggable_region = view_->draggable_region();
    if (draggable_region && draggable_region->contains(point.x(), point.y())) {
      return HTCAPTION;
    }
  }

  gfx::Insets resize_border = GetFrameBorderInsets();
  int window_component = GetHTComponentForFrame(
      point, resize_border, kResizeAreaCornerSize, kResizeAreaCornerSize,
      widget_->widget_delegate()->CanResize());
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void LinuxFrameView::GetWindowMask(const gfx::Size& size,
                                   SkPath* window_mask) {}

void LinuxFrameView::ResetWindowControls() {
  if (restore_button_) {
    restore_button_->SetState(views::Button::STATE_NORMAL);
  }
  if (minimize_button_) {
    minimize_button_->SetState(views::Button::STATE_NORMAL);
  }
  if (maximize_button_) {
    maximize_button_->SetState(views::Button::STATE_NORMAL);
  }
}

void LinuxFrameView::UpdateWindowIcon() {}

void LinuxFrameView::UpdateWindowTitle() {
  if (widget_->widget_delegate()->ShouldShowWindowTitle() &&
      maximum_title_bar_x_ > -1) {
    LayoutTitleBar();
    SchedulePaintInRect(title_bounds_);
  }
}

void LinuxFrameView::SizeConstraintsChanged() {
  ResetWindowControls();
  LayoutWindowControls();
}

void LinuxFrameView::OnPaint(gfx::Canvas* canvas) {
  if (widget_->IsFullscreen()) {
    return;
  }

  PaintFrameBorder(canvas);

  // Paint title text.
  views::WidgetDelegate* delegate = widget_->widget_delegate();
  if (delegate && delegate->ShouldShowWindowTitle()) {
    gfx::Rect rect = title_bounds_;
    rect.set_x(GetMirroredXForRect(title_bounds_));
    canvas->DrawStringRect(
        delegate->GetWindowTitle(),
        views::TypographyProvider::Get().GetWindowTitleFontList(),
        GetColorProvider()->GetColor(ui::kColorCustomFrameCaptionForeground),
        rect);
  }
}

void LinuxFrameView::Layout(views::View::PassKey) {
  if (nav_button_provider_) {
    MaybeUpdateCachedNavButtonImages();
  }

  if (!widget_->IsFullscreen()) {
    LayoutWindowControls();
    LayoutTitleBar();
  }

  LayoutClientView();
  LayoutSuperclass<views::FrameView>(this);
}

gfx::Size LinuxFrameView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return widget_->non_client_view()
      ->GetWindowBoundsForClientBounds(
          gfx::Rect(widget_->client_view()->GetPreferredSize(available_size)))
      .size();
}

gfx::Size LinuxFrameView::GetMinimumSize() const {
  return widget_->non_client_view()
      ->GetWindowBoundsForClientBounds(
          gfx::Rect(widget_->client_view()->GetMinimumSize()))
      .size();
}

gfx::Size LinuxFrameView::GetMaximumSize() const {
  gfx::Size max_size = widget_->client_view()->GetMaximumSize();
  gfx::Size converted_size =
      widget_->non_client_view()
          ->GetWindowBoundsForClientBounds(gfx::Rect(max_size))
          .size();
  return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                   max_size.height() == 0 ? 0 : converted_size.height());
}

void LinuxFrameView::OnThemeChanged() {
  views::FrameView::OnThemeChanged();
  nav_button_cache_.reset();
  SchedulePaint();
}

ui::WindowFrameProvider* LinuxFrameView::GetFrameProvider() const {
  bool tiled = false;
  bool maximized = widget_->IsMaximized();
  // solid_frame=true means no shadow (tiled/maximized windows).
  bool solid_frame = maximized || tiled;
  return linux_ui_theme_->GetWindowFrameProvider(solid_frame, tiled, maximized);
}

gfx::Insets LinuxFrameView::GetFrameBorderInsets() const {
  if (widget_->IsMaximized() || widget_->IsFullscreen()) {
    return gfx::Insets();
  }
  auto* provider = GetFrameProvider();
  if (provider) {
    return provider->GetFrameThicknessDip();
  }
  return gfx::Insets(4);
}

int LinuxFrameView::GetTopAreaHeight() const {
  if (widget_->IsFullscreen()) {
    return 0;
  }
  gfx::Insets insets = GetFrameBorderInsets();
  return std::max(insets.top(),
                  insets.top() + kCaptionButtonHeightWithPadding);
}

void LinuxFrameView::LayoutWindowControls() {
  minimum_title_bar_x_ = 0;
  maximum_title_bar_x_ = width();

  if (bounds().IsEmpty()) {
    return;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int caption_y = insets.top();
  int next_button_x = insets.left();

  bool is_maximized = widget_->IsMaximized();
  bool is_restored = !is_maximized && !widget_->IsMinimized();
  views::ImageButton* invisible_button =
      is_restored ? restore_button_.get() : maximize_button_.get();
  if (invisible_button) {
    invisible_button->SetVisible(false);
  }

  views::WindowButtonOrderProvider* button_order =
      views::WindowButtonOrderProvider::GetInstance();
  const std::vector<views::FrameButton>& leading_buttons =
      button_order->leading_buttons();
  const std::vector<views::FrameButton>& trailing_buttons =
      button_order->trailing_buttons();

  for (auto frame_button : leading_buttons) {
    views::ImageButton* button = GetImageButton(frame_button);
    if (!button) {
      continue;
    }
    gfx::Rect target_bounds(gfx::Point(next_button_x, caption_y),
                            button->GetPreferredSize({}));
    LayoutButton(button, target_bounds);
    next_button_x += button->width();
    minimum_title_bar_x_ = std::min(width(), next_button_x);
  }

  next_button_x = width() - insets.right();
  for (auto frame_button : base::Reversed(trailing_buttons)) {
    views::ImageButton* button = GetImageButton(frame_button);
    if (!button) {
      continue;
    }
    gfx::Rect target_bounds(gfx::Point(next_button_x, caption_y),
                            button->GetPreferredSize({}));
    target_bounds.Offset(-target_bounds.width(), 0);
    LayoutButton(button, target_bounds);
    next_button_x = button->x();
    maximum_title_bar_x_ = std::max(minimum_title_bar_x_, next_button_x);
  }
}

void LinuxFrameView::LayoutTitleBar() {
  if (!widget_->widget_delegate()->ShouldShowWindowTitle()) {
    return;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int title_x = insets.left() + kIconLeftSpacing + minimum_title_bar_x_ +
                kTitleIconOffsetX;
  int title_height =
      views::TypographyProvider::Get().GetWindowTitleFontList().GetHeight();
  int top_height = GetTopAreaHeight();
  int title_y = insets.top() + (top_height - insets.top() - title_height) / 2;

  title_bounds_.SetRect(
      title_x, title_y,
      std::max(0, maximum_title_bar_x_ - kTitleCaptionSpacing - title_x),
      title_height);
}

void LinuxFrameView::LayoutClientView() {
  if (widget_->IsFullscreen()) {
    client_view_bounds_ = bounds();
    return;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  int top_height = GetTopAreaHeight();
  client_view_bounds_.SetRect(
      insets.left(), top_height,
      std::max(0, width() - insets.left() - insets.right()),
      std::max(0, height() - top_height - insets.bottom()));
}

void LinuxFrameView::PaintFrameBorder(gfx::Canvas* canvas) {
  auto* provider = GetFrameProvider();
  if (!provider) {
    return;
  }

  gfx::Insets input_insets;
  if (widget_->IsMaximized()) {
    input_insets = gfx::Insets();
  }

  provider->PaintWindowFrame(canvas, GetLocalBounds(), GetTopAreaHeight(),
                             ShouldPaintAsActive(), input_insets);
}

views::ImageButton* LinuxFrameView::InitWindowCaptionButton(
    views::Button::PressedCallback callback,
    int accessibility_string_id,
    int normal_image_id,
    int hot_image_id,
    int pushed_image_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::ImageButton* button =
      AddChildView(std::make_unique<views::ImageButton>(std::move(callback)));
  button->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(accessibility_string_id));
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImage(rb.GetImageNamed(normal_image_id)));
  button->SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromImage(rb.GetImageNamed(hot_image_id)));
  button->SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromImage(rb.GetImageNamed(pushed_image_id)));
  return button;
}

views::ImageButton* LinuxFrameView::GetImageButton(
    views::FrameButton frame_button) {
  views::ImageButton* button = nullptr;
  switch (frame_button) {
    case views::FrameButton::kMinimize: {
      button = minimize_button_;
      bool should_show = widget_->widget_delegate()->CanMinimize();
      if (button) {
        button->SetVisible(should_show);
      }
      return should_show ? button : nullptr;
    }
    case views::FrameButton::kMaximize: {
      bool is_restored = !widget_->IsMaximized() && !widget_->IsMinimized();
      button = is_restored ? maximize_button_.get() : restore_button_.get();
      bool should_show = widget_->widget_delegate()->CanMaximize();
      if (button) {
        button->SetVisible(should_show);
      }
      return should_show ? button : nullptr;
    }
    case views::FrameButton::kClose: {
      return close_button_;
    }
  }
  return nullptr;
}

void LinuxFrameView::MaybeUpdateCachedNavButtonImages() {
  if (!nav_button_provider_) {
    return;
  }

  gfx::Insets insets = GetFrameBorderInsets();
  NavButtonCacheParams params{GetTopAreaHeight() - insets.top(),
                              widget_->IsMaximized(), ShouldPaintAsActive()};
  if (nav_button_cache_ == params) {
    return;
  }
  nav_button_cache_ = params;

  nav_button_provider_->RedrawImages(params.top_area_height, params.maximized,
                                     params.active);

  for (auto type : {
           ui::NavButtonProvider::FrameButtonDisplayType::kMinimize,
           params.maximized
               ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
               : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize,
           ui::NavButtonProvider::FrameButtonDisplayType::kClose,
       }) {
    views::Button* button = GetButtonFromDisplayType(type);
    if (!button) {
      continue;
    }
    for (size_t state = 0; state < views::Button::STATE_COUNT; state++) {
      auto button_state = static_cast<views::Button::ButtonState>(state);
      static_cast<views::ImageButton*>(button)->SetImageModel(
          button_state,
          ui::ImageModel::FromImageSkia(nav_button_provider_->GetImage(
              type, ToNavButtonState(button_state))));
    }
  }
}

views::Button* LinuxFrameView::GetButtonFromDisplayType(
    ui::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return minimize_button_;
    case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      return maximize_button_;
    case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return restore_button_;
    case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
      return close_button_;
    default:
      NOTREACHED();
  }
}

BEGIN_METADATA(LinuxFrameView)
END_METADATA
