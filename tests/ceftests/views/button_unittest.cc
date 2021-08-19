// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/views/cef_button.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_menu_button.h"
#include "include/views/cef_menu_button_delegate.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/image_util.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/views/test_window_delegate.h"
#include "tests/gtest/include/gtest/gtest.h"

#define BUTTON_TEST(name) UI_THREAD_TEST(ViewsButtonTest, name)
#define BUTTON_TEST_ASYNC(name) UI_THREAD_TEST_ASYNC(ViewsButtonTest, name)

namespace {

CefRefPtr<CefImage> CreateIconImage() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  image_util::LoadIconImage(image, 1.0);
  image_util::LoadIconImage(image, 2.0);
  return image;
}

const char kButtonText[] = "My Button";

void VerifyButtonStyle(CefRefPtr<CefButton> button) {
  // Test state.
  EXPECT_EQ(CEF_BUTTON_STATE_NORMAL, button->GetState());
  button->SetState(CEF_BUTTON_STATE_HOVERED);
  EXPECT_EQ(CEF_BUTTON_STATE_HOVERED, button->GetState());
  button->SetState(CEF_BUTTON_STATE_PRESSED);
  EXPECT_EQ(CEF_BUTTON_STATE_PRESSED, button->GetState());
  button->SetState(CEF_BUTTON_STATE_DISABLED);
  EXPECT_EQ(CEF_BUTTON_STATE_DISABLED, button->GetState());
  button->SetState(CEF_BUTTON_STATE_NORMAL);

  button->SetTooltipText("Some tooltip text");
  button->SetAccessibleName("MyButton");
}

void VerifyLabelButtonImage(CefRefPtr<CefLabelButton> button,
                            cef_button_state_t state,
                            CefRefPtr<CefImage> image) {
  EXPECT_FALSE(button->GetImage(state).get()) << "state = " << state;
  button->SetImage(state, image);
  EXPECT_TRUE(image->IsSame(button->GetImage(state))) << "state = " << state;
  button->SetImage(state, nullptr);
  EXPECT_FALSE(button->GetImage(state).get()) << "state = " << state;
}

void VerifyLabelButtonStyle(CefRefPtr<CefLabelButton> button) {
  VerifyButtonStyle(button);

  // Test set/get text.
  EXPECT_STREQ(kButtonText, button->GetText().ToString().c_str());
  const char kText[] = "My text";
  button->SetText(kText);
  EXPECT_STREQ(kText, button->GetText().ToString().c_str());

  // Test images.
  CefRefPtr<CefImage> image = CreateIconImage();
  VerifyLabelButtonImage(button, CEF_BUTTON_STATE_NORMAL, image);
  VerifyLabelButtonImage(button, CEF_BUTTON_STATE_HOVERED, image);
  VerifyLabelButtonImage(button, CEF_BUTTON_STATE_PRESSED, image);
  VerifyLabelButtonImage(button, CEF_BUTTON_STATE_DISABLED, image);

  // Test colors.
  const cef_color_t color = CefColorSetARGB(255, 255, 0, 255);
  button->SetTextColor(CEF_BUTTON_STATE_NORMAL, color);
  button->SetTextColor(CEF_BUTTON_STATE_HOVERED, color);
  button->SetTextColor(CEF_BUTTON_STATE_PRESSED, color);
  button->SetTextColor(CEF_BUTTON_STATE_DISABLED, color);
  button->SetEnabledTextColors(color);

  // Test alignment.
  button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_LEFT);
  button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_CENTER);
  button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_RIGHT);

  // Test fonts.
  button->SetFontList("Arial, 14px");

  // Test sizes.
  button->SetMinimumSize(CefSize(100, 100));
  button->SetMaximumSize(CefSize(100, 100));
}

void VerifyMenuButtonStyle(CefRefPtr<CefMenuButton> button) {
  VerifyLabelButtonStyle(button);
}

class EmptyMenuButtonDelegate : public CefMenuButtonDelegate {
 public:
  EmptyMenuButtonDelegate() {}

  void OnMenuButtonPressed(
      CefRefPtr<CefMenuButton> menu_button,
      const CefPoint& screen_point,
      CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) override {
    EXPECT_TRUE(false);  // Not reached.
  }

  void OnButtonPressed(CefRefPtr<CefButton> button) override {
    EXPECT_TRUE(false);  // Not reached.
  }

 private:
  IMPLEMENT_REFCOUNTING(EmptyMenuButtonDelegate);
  DISALLOW_COPY_AND_ASSIGN(EmptyMenuButtonDelegate);
};

void RunLabelButtonStyle(CefRefPtr<CefWindow> window) {
  CefRefPtr<CefLabelButton> button = CefLabelButton::CreateLabelButton(
      new EmptyMenuButtonDelegate(), kButtonText);

  // Must be added to a parent window before retrieving the style to avoid
  // a CHECK() in View::GetNativeTheme(). See https://crbug.com/1056756.
  window->AddChildView(button);
  window->Layout();

  VerifyLabelButtonStyle(button);
}

void LabelButtonStyleImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunLabelButtonStyle);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void RunMenuButtonStyle(CefRefPtr<CefWindow> window) {
  CefRefPtr<CefMenuButton> button = CefMenuButton::CreateMenuButton(
      new EmptyMenuButtonDelegate(), kButtonText);

  // Must be added to a parent window before retrieving the style to avoid
  // a CHECK() in View::GetNativeTheme(). See https://crbug.com/1056756.
  window->AddChildView(button);
  window->Layout();

  VerifyMenuButtonStyle(button);
}

void MenuButtonStyleImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunMenuButtonStyle);
  TestWindowDelegate::RunTest(event, std::move(config));
}

}  // namespace

// Test Button getters/setters.
BUTTON_TEST_ASYNC(LabelButtonStyle)
BUTTON_TEST_ASYNC(MenuButtonStyle)

namespace {

// Mouse click delay in MS.
const int kClickDelayMS = 100;

const int kButtonID = 1;

class TestButtonDelegate : public CefButtonDelegate {
 public:
  TestButtonDelegate() {}

  void OnButtonPressed(CefRefPtr<CefButton> button) override {
    EXPECT_TRUE(button.get());
    EXPECT_EQ(button->GetID(), kButtonID);

    // Complete the test by closing the window.
    button->GetWindow()->Close();
  }

 private:
  IMPLEMENT_REFCOUNTING(TestButtonDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestButtonDelegate);
};

void ClickButton(CefRefPtr<CefWindow> window, int button_id) {
  CefRefPtr<CefView> button = window->GetViewForID(button_id);
  EXPECT_TRUE(button->AsButton());

  // Determine the middle of the button in screen coordinates.
  const CefRect& bounds = button->GetBoundsInScreen();
  const CefPoint& click_point =
      CefPoint(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);

  // Click the button.
  window->SendMouseMove(click_point.x, click_point.y);
  window->SendMouseEvents(MBT_LEFT, true, true);
}

void AddImage(CefRefPtr<CefLabelButton> button) {
  CefRefPtr<CefImage> image = CreateIconImage();
  button->SetImage(CEF_BUTTON_STATE_NORMAL, image);
}

void RunLabelButtonClick(bool with_text,
                         bool with_image,
                         CefRefPtr<CefWindow> window) {
  CefRefPtr<CefLabelButton> button = CefLabelButton::CreateLabelButton(
      new TestButtonDelegate(), with_text ? kButtonText : "");
  button->SetID(kButtonID);

  EXPECT_TRUE(button->AsButton());
  EXPECT_TRUE(button->AsButton()->AsLabelButton());
  EXPECT_EQ(kButtonID, button->GetID());
  EXPECT_TRUE(button->IsVisible());
  EXPECT_FALSE(button->IsDrawn());

  if (with_text)
    EXPECT_STREQ(kButtonText, button->GetText().ToString().c_str());
  else
    EXPECT_TRUE(button->GetText().empty());

  if (with_image)
    AddImage(button);

  window->AddChildView(button);
  window->Layout();

  EXPECT_TRUE(window->IsSame(button->GetWindow()));
  EXPECT_TRUE(window->IsSame(button->GetParentView()));
  EXPECT_TRUE(button->IsSame(window->GetViewForID(kButtonID)));
  EXPECT_TRUE(button->IsVisible());
  EXPECT_TRUE(button->IsDrawn());

  window->Show();

  // Wait a bit before trying to click the button.
  CefPostDelayedTask(TID_UI, base::BindOnce(ClickButton, window, kButtonID),
                     kClickDelayMS);
}

void LabelButtonClick(CefRefPtr<CefWaitableEvent> event,
                      bool with_button_frame,
                      bool with_button_text,
                      bool with_button_image) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created =
      base::BindOnce(RunLabelButtonClick, with_button_text, with_button_image);
  config->frameless = false;
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void LabelButtonClickFramedWithTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, true, true, true);
}

void LabelButtonClickFramedWithTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, true, true, false);
}

void LabelButtonClickFramedNoTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, true, false, true);
}

void LabelButtonClickFramedNoTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, true, false, false);
}

void LabelButtonClickFramelessWithTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, false, true, true);
}

void LabelButtonClickFramelessWithTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, false, true, false);
}

void LabelButtonClickFramelessNoTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, false, false, true);
}

void LabelButtonClickFramelessNoTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  LabelButtonClick(event, false, false, false);
}

}  // namespace

// Test LabelButton functionality. This is primarily to exercise exposed CEF
// APIs and is not intended to comprehensively test button-related behavior
// (which we presume that Chromium is testing).
BUTTON_TEST_ASYNC(LabelButtonClickFramedWithTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramedWithTextNoImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramedNoTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramedNoTextNoImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramelessWithTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramelessWithTextNoImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramelessNoTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(LabelButtonClickFramelessNoTextNoImageFramelessWindow)

namespace {

const int kMenuItemID = 2;
const char kMenuItemLabel[] = "My Menu Item";

void ClickMenuItem(CefRefPtr<CefMenuButton> menu_button) {
  // Determine the lower-right corner of the menu button, then offset a bit to
  // hit the first menu item.
  const CefRect& bounds = menu_button->GetBoundsInScreen();
  const CefPoint& click_point =
      CefPoint(bounds.x + bounds.width + 10, bounds.y + bounds.height + 10);

  // Click the menu item.
  CefRefPtr<CefWindow> window = menu_button->GetWindow();
  window->SendMouseMove(click_point.x, click_point.y);
  window->SendMouseEvents(MBT_LEFT, true, true);
}

class TestMenuButtonDelegate : public CefMenuButtonDelegate,
                               public CefMenuModelDelegate {
 public:
  TestMenuButtonDelegate() {}

  void OnMenuButtonPressed(
      CefRefPtr<CefMenuButton> menu_button,
      const CefPoint& screen_point,
      CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) override {
    window_ = menu_button->GetWindow();

    CefRefPtr<CefMenuModel> model = CefMenuModel::CreateMenuModel(this);
    model->AddItem(kMenuItemID, kMenuItemLabel);

    // Verify color accessors.
    for (int i = 0; i < CEF_MENU_COLOR_COUNT; ++i) {
      cef_menu_color_type_t color_type = static_cast<cef_menu_color_type_t>(i);
      cef_color_t color_out;
      cef_color_t color = CefColorSetARGB(255, 255, 255, i);

      // No color set yet.
      color_out = 1;
      EXPECT_TRUE(model->GetColor(kMenuItemID, color_type, color_out));
      EXPECT_EQ(0U, color_out);
      color_out = 1;
      EXPECT_TRUE(model->GetColorAt(0, color_type, color_out));
      EXPECT_EQ(0U, color_out);
      color_out = 1;
      EXPECT_TRUE(model->GetColorAt(-1, color_type, color_out));
      EXPECT_EQ(0U, color_out);

      // Set the default color.
      EXPECT_TRUE(model->SetColorAt(-1, color_type, color));
      color_out = 1;
      EXPECT_TRUE(model->GetColorAt(-1, color_type, color_out));
      EXPECT_EQ(color, color_out);

      // Clear the default color.
      EXPECT_TRUE(model->SetColorAt(-1, color_type, 0));
      color_out = 1;
      EXPECT_TRUE(model->GetColorAt(-1, color_type, color_out));
      EXPECT_EQ(0U, color_out);

      // Set the index color.
      EXPECT_TRUE(model->SetColorAt(0, color_type, color));
      color_out = 1;
      EXPECT_TRUE(model->GetColorAt(0, color_type, color_out));
      EXPECT_EQ(color, color_out);

      // Clear the index color.
      EXPECT_TRUE(model->SetColorAt(0, color_type, 0));
      color_out = 1;
      EXPECT_TRUE(model->GetColorAt(0, color_type, color_out));
      EXPECT_EQ(0U, color_out);

      // Set the ID color.
      EXPECT_TRUE(model->SetColor(kMenuItemID, color_type, color));
      color_out = 1;
      EXPECT_TRUE(model->GetColor(kMenuItemID, color_type, color_out));
      EXPECT_EQ(color, color_out);

      // Clear the ID color.
      EXPECT_TRUE(model->SetColor(kMenuItemID, color_type, 0));
      color_out = 1;
      EXPECT_TRUE(model->GetColor(kMenuItemID, color_type, color_out));
      EXPECT_EQ(0U, color_out);

      // Index/ID doesn't exist.
      EXPECT_FALSE(model->SetColorAt(4, color_type, color));
      EXPECT_FALSE(model->SetColor(4, color_type, color));
      color_out = 1;
      EXPECT_FALSE(model->GetColorAt(4, color_type, color_out));
      EXPECT_FALSE(model->GetColor(4, color_type, color_out));
      EXPECT_EQ(1U, color_out);
    }

    // Verify font accessors.
    const std::string& font = "Tahoma, 12px";
    EXPECT_TRUE(model->SetFontListAt(0, font));
    EXPECT_TRUE(model->SetFontListAt(0, CefString()));
    EXPECT_TRUE(model->SetFontList(kMenuItemID, font));
    EXPECT_TRUE(model->SetFontList(kMenuItemID, CefString()));

    // Index/ID doesn't exist.
    EXPECT_FALSE(model->SetFontListAt(4, font));
    EXPECT_FALSE(model->SetFontList(4, font));

    // Wait a bit before trying to click the menu item.
    CefPostDelayedTask(TID_UI, base::BindOnce(ClickMenuItem, menu_button),
                       kClickDelayMS);

    menu_button->ShowMenu(model, screen_point, CEF_MENU_ANCHOR_TOPLEFT);
  }

  void OnButtonPressed(CefRefPtr<CefButton> button) override {}

  void ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                      int command_id,
                      cef_event_flags_t event_flags) override {
    EXPECT_TRUE(menu_model.get());
    EXPECT_EQ(command_id, kMenuItemID);

    // Complete the test by closing the window.
    window_->GetWindow()->Close();
    window_ = nullptr;
  }

 private:
  CefRefPtr<CefWindow> window_;

  IMPLEMENT_REFCOUNTING(TestMenuButtonDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestMenuButtonDelegate);
};

void RunMenuButtonClick(bool with_text,
                        bool with_image,
                        CefRefPtr<CefWindow> window) {
  CefRefPtr<CefMenuButton> button = CefMenuButton::CreateMenuButton(
      new TestMenuButtonDelegate(), with_text ? kButtonText : "");
  button->SetID(kButtonID);

  EXPECT_TRUE(button->AsButton());
  EXPECT_TRUE(button->AsButton()->AsLabelButton());
  EXPECT_TRUE(button->AsButton()->AsLabelButton()->AsMenuButton());
  EXPECT_EQ(kButtonID, button->GetID());
  EXPECT_TRUE(button->IsVisible());
  EXPECT_FALSE(button->IsDrawn());

  if (with_text)
    EXPECT_STREQ(kButtonText, button->GetText().ToString().c_str());
  else
    EXPECT_TRUE(button->GetText().empty());

  if (with_image)
    AddImage(button);

  window->AddChildView(button);
  window->Layout();

  EXPECT_TRUE(window->IsSame(button->GetWindow()));
  EXPECT_TRUE(window->IsSame(button->GetParentView()));
  EXPECT_TRUE(button->IsSame(window->GetViewForID(kButtonID)));
  EXPECT_TRUE(button->IsVisible());
  EXPECT_TRUE(button->IsDrawn());

  window->Show();

  // Wait a bit before trying to click the button.
  CefPostDelayedTask(TID_UI, base::BindOnce(ClickButton, window, kButtonID),
                     kClickDelayMS);
}

void MenuButtonClick(CefRefPtr<CefWaitableEvent> event,
                     bool with_button_frame,
                     bool with_button_text,
                     bool with_button_image) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created =
      base::BindOnce(RunMenuButtonClick, with_button_text, with_button_image);
  config->frameless = false;
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void MenuButtonClickFramedWithTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, true, true, true);
}

void MenuButtonClickFramedWithTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, true, true, false);
}

void MenuButtonClickFramedNoTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, true, false, true);
}

void MenuButtonClickFramedNoTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, true, false, false);
}

void MenuButtonClickFramelessWithTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, false, true, true);
}

void MenuButtonClickFramelessWithTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, false, true, false);
}

void MenuButtonClickFramelessNoTextWithImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, false, false, true);
}

void MenuButtonClickFramelessNoTextNoImageFramelessWindowImpl(
    CefRefPtr<CefWaitableEvent> event) {
  MenuButtonClick(event, false, false, false);
}

}  // namespace

// Test MenuButton functionality. This is primarily to exercise exposed CEF
// APIs and is not intended to comprehensively test button-related behavior
// (which we presume that Chromium is testing).
BUTTON_TEST_ASYNC(MenuButtonClickFramedWithTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramedWithTextNoImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramedNoTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramedNoTextNoImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramelessWithTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramelessWithTextNoImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramelessNoTextWithImageFramelessWindow)
BUTTON_TEST_ASYNC(MenuButtonClickFramelessNoTextNoImageFramelessWindow)

namespace {

class TestMenuButtonCustomPopupDelegate : public CefMenuButtonDelegate,
                                          public CefWindowDelegate {
 public:
  explicit TestMenuButtonCustomPopupDelegate(bool can_activate)
      : can_activate_(can_activate) {}

  void OnMenuButtonPressed(
      CefRefPtr<CefMenuButton> menu_button,
      const CefPoint& screen_point,
      CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) override {
    parent_window_ = menu_button->GetWindow();
    button_pressed_lock_ = button_pressed_lock;

    popup_window_ = CefWindow::CreateTopLevelWindow(this);
    popup_window_->SetBounds(CefRect(screen_point.x, screen_point.y, 100, 100));

    CefRefPtr<CefLabelButton> button =
        CefLabelButton::CreateLabelButton(this, "Button");
    button->SetFocusable(can_activate_);
    popup_window_->AddChildView(button);

    popup_window_->Show();

    // Wait a bit before trying to click the popup button.
    CefPostDelayedTask(TID_UI, base::BindOnce(ClickMenuItem, menu_button),
                       kClickDelayMS);
  }

  void OnButtonPressed(CefRefPtr<CefButton> button) override {
    EXPECT_TRUE(button->GetWindow()->IsSame(popup_window_));
    popup_window_->Close();
    popup_window_ = nullptr;
    button_pressed_lock_ = nullptr;
  }

  CefRefPtr<CefWindow> GetParentWindow(CefRefPtr<CefWindow> window,
                                       bool* is_menu,
                                       bool* can_activate_menu) override {
    EXPECT_TRUE(parent_window_);
    *is_menu = true;
    *can_activate_menu = can_activate_;
    return parent_window_;
  }

  bool IsFrameless(CefRefPtr<CefWindow> window) override { return true; }

  void OnFocus(CefRefPtr<CefView> view) override {
    if (popup_window_ && view->GetWindow()->IsSame(popup_window_)) {
      EXPECT_TRUE(can_activate_);
      got_focus_.yes();
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    if (can_activate_)
      EXPECT_TRUE(got_focus_);
    else
      EXPECT_FALSE(got_focus_);

    // Complete the test by closing the parent window.
    parent_window_->Close();
    parent_window_ = nullptr;
  }

 private:
  const bool can_activate_;

  CefRefPtr<CefWindow> parent_window_;
  CefRefPtr<CefWindow> popup_window_;
  CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock_;

  TrackCallback got_focus_;

  IMPLEMENT_REFCOUNTING(TestMenuButtonCustomPopupDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestMenuButtonCustomPopupDelegate);
};

void RunMenuButtonCustomPopupClick(bool can_activate,
                                   CefRefPtr<CefWindow> window) {
  CefRefPtr<CefMenuButton> button = CefMenuButton::CreateMenuButton(
      new TestMenuButtonCustomPopupDelegate(can_activate), "Custom");
  button->SetID(kButtonID);

  window->AddChildView(button);
  window->Layout();

  window->Show();

  // Wait a bit before trying to click the button.
  CefPostDelayedTask(TID_UI, base::BindOnce(ClickButton, window, kButtonID),
                     kClickDelayMS);
}

void MenuButtonCustomPopupClick(CefRefPtr<CefWaitableEvent> event,
                                bool can_activate) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created =
      base::BindOnce(RunMenuButtonCustomPopupClick, can_activate);
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void MenuButtonCustomPopupActivateImpl(CefRefPtr<CefWaitableEvent> event) {
  MenuButtonCustomPopupClick(event, true);
}

void MenuButtonCustomPopupNoActivateImpl(CefRefPtr<CefWaitableEvent> event) {
  MenuButtonCustomPopupClick(event, false);
}

}  // namespace

BUTTON_TEST_ASYNC(MenuButtonCustomPopupActivate)
BUTTON_TEST_ASYNC(MenuButtonCustomPopupNoActivate)
