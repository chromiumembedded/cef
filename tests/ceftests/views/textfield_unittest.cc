// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_pack_strings.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/views/test_window_delegate.h"
#include "tests/gtest/include/gtest/gtest.h"

// See ui/events/keycodes/keyboard_codes.h
#define VKEY_UNKNOWN 0
#if defined(OS_WIN)
#define VKEY_A 'A'
#define VKEY_SPACE VK_SPACE
#define VKEY_RETURN VK_RETURN
#elif defined(OS_POSIX)
#define VKEY_A 0x41
#define VKEY_SPACE 0x20
#define VKEY_RETURN 0x0D
#else
#error "Unsupported platform"
#endif

#define TEXTFIELD_TEST(name) UI_THREAD_TEST(ViewsTextfieldTest, name)
#define TEXTFIELD_TEST_ASYNC(name) \
  UI_THREAD_TEST_ASYNC(ViewsTextfieldTest, name)

namespace {

void RunTextfieldContents(CefRefPtr<CefWindow> window) {
  CefRefPtr<CefTextfield> textfield = CefTextfield::CreateTextfield(nullptr);
  EXPECT_TRUE(textfield.get());
  EXPECT_TRUE(textfield->AsTextfield().get());

  // Must be added to a parent window before retrieving the style to avoid
  // a CHECK() in View::GetNativeTheme(). See https://crbug.com/1056756.
  window->AddChildView(textfield);
  window->Layout();

  // Test defaults.
  EXPECT_TRUE(textfield->GetText().empty());
  EXPECT_FALSE(textfield->HasSelection());
  EXPECT_EQ(CefRange(0, 0), textfield->GetSelectedRange());
  EXPECT_EQ(0U, textfield->GetCursorPosition());

  // Test set/get text.
  const char kText[] = "My test message!";
  textfield->SetText(kText);
  EXPECT_STREQ(kText, textfield->GetText().ToString().c_str());

  size_t cursor_pos = sizeof(kText) - 1;
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  // Test append text.
  const char kAppendText[] = " And more.";
  textfield->AppendText(kAppendText);
  EXPECT_STREQ((std::string(kText) + kAppendText).c_str(),
               textfield->GetText().ToString().c_str());
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  // Test select range.
  EXPECT_FALSE(textfield->HasSelection());
  EXPECT_EQ(
      CefRange(static_cast<int>(cursor_pos), static_cast<int>(cursor_pos)),
      textfield->GetSelectedRange());
  textfield->SelectRange(CefRange(0, static_cast<int>(cursor_pos)));
  EXPECT_TRUE(textfield->HasSelection());
  EXPECT_EQ(CefRange(0, static_cast<int>(cursor_pos)),
            textfield->GetSelectedRange());
  EXPECT_STREQ(kText, textfield->GetSelectedText().ToString().c_str());
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  // Test insert or replace.
  const char kReplaceText[] = "Other text.";
  textfield->InsertOrReplaceText(kReplaceText);
  EXPECT_STREQ((std::string(kReplaceText) + kAppendText).c_str(),
               textfield->GetText().ToString().c_str());

  cursor_pos = sizeof(kReplaceText) - 1;
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  // Test select all.
  EXPECT_FALSE(textfield->HasSelection());
  textfield->SelectAll(false);
  EXPECT_TRUE(textfield->HasSelection());

  cursor_pos = sizeof(kReplaceText) + sizeof(kAppendText) - 2;
  EXPECT_EQ(CefRange(0, static_cast<int>(cursor_pos)),
            textfield->GetSelectedRange());
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  // Test clear selection.
  textfield->ClearSelection();
  EXPECT_FALSE(textfield->HasSelection());
  EXPECT_EQ(
      CefRange(static_cast<int>(cursor_pos), static_cast<int>(cursor_pos)),
      textfield->GetSelectedRange());
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  // Test selection with command.
  EXPECT_TRUE(textfield->IsCommandEnabled(CEF_TFC_SELECT_ALL));
  textfield->ExecuteCommand(CEF_TFC_SELECT_ALL);
  EXPECT_TRUE(textfield->HasSelection());
  EXPECT_EQ(CefRange(0, static_cast<int>(cursor_pos)),
            textfield->GetSelectedRange());
  EXPECT_EQ(cursor_pos, textfield->GetCursorPosition());

  textfield->ClearEditHistory();
}

void TextfieldContentsImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunTextfieldContents);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void RunTextfieldStyle(CefRefPtr<CefWindow> window) {
  CefRefPtr<CefTextfield> textfield = CefTextfield::CreateTextfield(nullptr);
  EXPECT_TRUE(textfield.get());

  // Must be added to a parent window before retrieving the style to avoid
  // a CHECK() in View::GetNativeTheme(). See https://crbug.com/1056756.
  window->AddChildView(textfield);
  window->Layout();

  // Test defaults.
  EXPECT_FALSE(textfield->IsPasswordInput());
  EXPECT_FALSE(textfield->IsReadOnly());

  // Test password input.
  textfield->SetPasswordInput(true);
  EXPECT_TRUE(textfield->IsPasswordInput());
  textfield->SetPasswordInput(false);
  EXPECT_FALSE(textfield->IsPasswordInput());

  // Test read only.
  textfield->SetReadOnly(true);
  EXPECT_TRUE(textfield->IsReadOnly());
  textfield->SetReadOnly(false);
  EXPECT_FALSE(textfield->IsReadOnly());

  // Test colors.
  const cef_color_t color = CefColorSetARGB(255, 255, 0, 255);

  EXPECT_NE(color, textfield->GetTextColor());
  textfield->SetTextColor(color);
  EXPECT_EQ(color, textfield->GetTextColor());

  EXPECT_NE(color, textfield->GetSelectionTextColor());
  textfield->SetSelectionTextColor(color);
  EXPECT_EQ(color, textfield->GetSelectionTextColor());

  EXPECT_NE(color, textfield->GetSelectionBackgroundColor());
  textfield->SetSelectionBackgroundColor(color);
  EXPECT_EQ(color, textfield->GetSelectionBackgroundColor());

  textfield->SetPlaceholderTextColor(color);

  // Test fonts.
  textfield->SetFontList("Arial, 14px");

  // Test format ranges.
  const char kText[] = "test text";
  textfield->SetText(kText);
  textfield->ApplyTextColor(color, CefRange(0, 5));
  textfield->ApplyTextStyle(CEF_TEXT_STYLE_BOLD, true, CefRange(0, 5));

  // Test placeholder text.
  textfield->SetPlaceholderText(kText);
  EXPECT_STREQ(kText, textfield->GetPlaceholderText().ToString().c_str());

  textfield->SetAccessibleName("MyTextfield");
}

void TextfieldStyleImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunTextfieldStyle);
  TestWindowDelegate::RunTest(event, std::move(config));
}

}  // namespace

// Test Textfield getters/setters.
TEXTFIELD_TEST_ASYNC(TextfieldContents)
TEXTFIELD_TEST_ASYNC(TextfieldStyle)

namespace {

const int kTextfieldID = 1;

// Contents need to be supported by the TranslateKey function.
const char kTestInputMessage[] = "Test Message";

void TranslateKey(int c, int* keycode, uint32* modifiers) {
  *keycode = VKEY_UNKNOWN;
  *modifiers = 0;

  if (c >= 'a' && c <= 'z') {
    *keycode = VKEY_A + (c - 'a');
  } else if (c >= 'A' && c <= 'Z') {
    *keycode = VKEY_A + (c - 'A');
    *modifiers = EVENTFLAG_SHIFT_DOWN;
  } else if (c == ' ') {
    *keycode = VKEY_SPACE;
  }
}

class TestTextfieldDelegate : public CefTextfieldDelegate {
 public:
  TestTextfieldDelegate() {}

  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                  const CefKeyEvent& event) override {
    EXPECT_TRUE(textfield.get());
    EXPECT_EQ(textfield->GetID(), kTextfieldID);

    if (event.type == KEYEVENT_RAWKEYDOWN &&
        event.windows_key_code == VKEY_RETURN) {
      // Got the whole string. Finish the test asynchronously.
      CefPostTask(TID_UI, base::BindOnce(&TestTextfieldDelegate::FinishTest,
                                         this, textfield));
      return true;
    }

    if (event.type == KEYEVENT_CHAR) {
      int keycode;
      uint32 modifiers;
      TranslateKey(kTestInputMessage[index_++], &keycode, &modifiers);

      EXPECT_EQ(keycode, event.windows_key_code);
      EXPECT_EQ(modifiers, event.modifiers);
    }

    return false;
  }

  void OnAfterUserAction(CefRefPtr<CefTextfield> textfield) override {
    after_user_action_ct_++;
  }

 private:
  void FinishTest(CefRefPtr<CefTextfield> textfield) {
    // OnAfterUserAction() should be called for each unhandled character.
    EXPECT_EQ(sizeof(kTestInputMessage) - 1, after_user_action_ct_);

    // Verify the completed contents.
    EXPECT_STREQ(kTestInputMessage, textfield->GetText().ToString().c_str());

    // Close the window to end the test.
    textfield->GetWindow()->Close();
  }

  int index_ = 0;
  size_t after_user_action_ct_ = 0;

  IMPLEMENT_REFCOUNTING(TestTextfieldDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestTextfieldDelegate);
};

void RunTextfieldKeyEvent(CefRefPtr<CefWindow> window) {
  CefRefPtr<CefTextfield> textfield =
      CefTextfield::CreateTextfield(new TestTextfieldDelegate());
  textfield->SetID(kTextfieldID);

  EXPECT_TRUE(textfield->AsTextfield());
  EXPECT_EQ(kTextfieldID, textfield->GetID());
  EXPECT_TRUE(textfield->IsVisible());
  EXPECT_FALSE(textfield->IsDrawn());

  window->AddChildView(textfield);
  window->Layout();

  EXPECT_TRUE(window->IsSame(textfield->GetWindow()));
  EXPECT_TRUE(window->IsSame(textfield->GetParentView()));
  EXPECT_TRUE(textfield->IsSame(window->GetViewForID(kTextfieldID)));
  EXPECT_TRUE(textfield->IsVisible());
  EXPECT_TRUE(textfield->IsDrawn());

  window->Show();

  // Give input focus to the textfield.
  textfield->RequestFocus();

  // Send the contents of |kTestInputMessage| to the textfield.
  for (size_t i = 0; i < sizeof(kTestInputMessage) - 1; ++i) {
    int keycode;
    uint32 modifiers;
    TranslateKey(kTestInputMessage[i], &keycode, &modifiers);
    window->SendKeyPress(keycode, modifiers);
  }

  // Send return to end the text input.
  window->SendKeyPress(VKEY_RETURN, 0);
}

void TextfieldKeyEventImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunTextfieldKeyEvent);
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

}  // namespace

// Test Textfield input and events. This is primarily to exercise exposed CEF
// APIs and is not intended to comprehensively test Textfield-related behavior
// (which we presume that Chromium is testing).
TEXTFIELD_TEST_ASYNC(TextfieldKeyEvent)
