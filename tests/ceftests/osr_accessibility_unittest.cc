// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_accessibility_handler.h"
#include "include/cef_parser.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kTestUrl[] = "https://tests/AccessibilityTestHandler";
const char kTipText[] = "Also known as User ID";

// Default OSR widget size.
const int kOsrWidth = 600;
const int kOsrHeight = 400;

// Test type.
enum AccessibilityTestType {
  // Enabling Accessibility should trigger the AccessibilityHandler callback
  // with Accessibility tree details
  TEST_ENABLE,
  // Disabling Accessibility should disable accessibility notification changes
  TEST_DISABLE,
  // Focus change on element should trigger Accessibility focus event
  TEST_FOCUS_CHANGE,
  // Hide/Show etc should trigger Location Change callbacks
  TEST_LOCATION_CHANGE
};

class AccessibilityTestHandler : public TestHandler,
                                 public CefRenderHandler,
                                 public CefAccessibilityHandler {
 public:
  AccessibilityTestHandler(const AccessibilityTestType& type)
      : test_type_(type), edit_box_id_(-1), accessibility_disabled_(false) {}

  CefRefPtr<CefAccessibilityHandler> GetAccessibilityHandler() override {
    return this;
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  // Cef Renderer Handler Methods
  bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    return true;
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override {
    screen_info.rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    screen_info.available_rect = screen_info.rect;
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {
    // Do nothing.
  }

  void RunTest() override {
    std::string html =
        "<html><head><title>AccessibilityTest</title></head>"
        "<body><span id='tipspan' role='tooltip' style='color:red;"
        "margin:20px'>";
    html += kTipText;
    html +=
        "</span>"
        "<input id='editbox' type='text' aria-describedby='tipspan' "
        "value='editbox' size='25px'/><input id='button' type='button' "
        "value='button' style='margin:20px'/></body></html>";
    AddResource(kTestUrl, html, "text/html");

    // Create the browser
    CreateOSRBrowser(kTestUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(5000);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    // Enable Accessibility
    browser->GetHost()->SetAccessibilityState(STATE_ENABLED);
    switch (test_type_) {
      case TEST_ENABLE: {
        // This should trigger OnAccessibilityTreeChange
        // And update will be validated
      } break;
      case TEST_DISABLE: {
        // Post a delayed task to disable Accessibility
        CefPostDelayedTask(
            TID_UI,
            base::Bind(&AccessibilityTestHandler::DisableAccessibility, this,
                       browser),
            200);
      } break;
      // Delayed task will posted later after we have initial details
      case TEST_FOCUS_CHANGE: {
      } break;
      case TEST_LOCATION_CHANGE: {
      } break;
    }
  }

  void OnAccessibilityTreeChange(CefRefPtr<CefValue> value) override {
    switch (test_type_) {
      case TEST_ENABLE: {
        TestEnableAccessibilityUpdate(value);
      } break;
      case TEST_DISABLE: {
        // Once Accessibility is disabled in the delayed Task
        // We should not reach here
        EXPECT_FALSE(accessibility_disabled_);
      } break;
      case TEST_LOCATION_CHANGE: {
        // find accessibility id of the edit box, before setting focus
        if (edit_box_id_ == -1) {
          EXPECT_TRUE(value.get());
          CefRefPtr<CefListValue> list = value->GetList();
          EXPECT_TRUE(list.get());
          EXPECT_GT(list->GetSize(), (size_t)0);

          // Get the first update dict and validate event type
          CefRefPtr<CefDictionaryValue> dict = list->GetDictionary(0);

          // Ignore other events.
          if (dict->GetString("event_type") != "layoutComplete")
            break;

          SetEditBoxIdAndRect(dict->GetDictionary("update"));
          EXPECT_TRUE(edit_box_id_ != -1);
          // Post a delayed task to hide the span and trigger location change
          CefPostDelayedTask(TID_UI,
                             base::Bind(&AccessibilityTestHandler::HideEditBox,
                                        this, GetBrowser()),
                             200);
        }
      } break;
      case TEST_FOCUS_CHANGE: {
        // find accessibility id of the edit box, before setting focus
        if (edit_box_id_ == -1) {
          EXPECT_TRUE(value.get());
          CefRefPtr<CefListValue> list = value->GetList();
          EXPECT_TRUE(list.get());
          EXPECT_GT(list->GetSize(), (size_t)0);

          // Get the first update dict and validate event type
          CefRefPtr<CefDictionaryValue> dict = list->GetDictionary(0);

          // Ignore other events.
          if (dict->GetString("event_type") != "layoutComplete")
            break;

          // Now post a delayed task to trigger focus to edit box
          SetEditBoxIdAndRect(dict->GetDictionary("update"));
          EXPECT_TRUE(edit_box_id_ != -1);

          CefPostDelayedTask(
              TID_UI,
              base::Bind(&AccessibilityTestHandler::SetFocusOnEditBox, this,
                         GetBrowser()),
              200);
        } else {
          EXPECT_TRUE(value.get());
          // Change has a valid non empty list
          EXPECT_TRUE(value->GetType() == VTYPE_LIST);
          CefRefPtr<CefListValue> list = value->GetList();
          EXPECT_TRUE(list.get());
          EXPECT_GT(list->GetSize(), (size_t)0);

          // Get the first update dict and validate event type
          CefRefPtr<CefDictionaryValue> dict = list->GetDictionary(0);

          // Ignore other events.
          if (dict->GetString("event_type") != "focus")
            return;

          // And Focus is set to expected element edit_box
          EXPECT_TRUE(edit_box_id_ == dict->GetInt("id"));

          // Now Post a delayed task to destroy the test giving
          // sufficient time for any accessibility updates to come through
          CefPostDelayedTask(
              TID_UI, base::Bind(&AccessibilityTestHandler::DestroyTest, this),
              500);
        }
      } break;
    }
  }

  void OnAccessibilityLocationChange(CefRefPtr<CefValue> value) override {
    if (test_type_ == TEST_LOCATION_CHANGE) {
      EXPECT_TRUE(edit_box_id_ != -1);
      EXPECT_TRUE(value.get());

      // Change has a valid list
      EXPECT_TRUE(value->GetType() == VTYPE_LIST);
      CefRefPtr<CefListValue> list = value->GetList();
      EXPECT_TRUE(list.get());
      // Ignore empty events
      if (!list->GetSize())
        return;

      // Hiding edit box should only change position of subsequent button
      EXPECT_EQ(list->GetSize(), (size_t)1);

      CefRefPtr<CefDictionaryValue> dict = list->GetDictionary(0);
      EXPECT_TRUE(dict.get());

      // New location of button should be same as older location of edit box
      // as that is hidden now
      CefRefPtr<CefDictionaryValue> location =
          dict->GetDictionary("new_location");
      EXPECT_TRUE(location.get());

      CefRefPtr<CefDictionaryValue> bounds = location->GetDictionary("bounds");
      EXPECT_TRUE(bounds.get());

      EXPECT_EQ(bounds->GetInt("x"), edit_box_rect_.x);
      EXPECT_EQ(bounds->GetInt("y"), edit_box_rect_.y);
      EXPECT_EQ(bounds->GetInt("width"), edit_box_rect_.width);
      EXPECT_EQ(bounds->GetInt("height"), edit_box_rect_.height);

      // Now Post a delayed task to destroy the test
      // giving sufficient time for any accessibility updates to come through
      CefPostDelayedTask(
          TID_UI, base::Bind(&AccessibilityTestHandler::DestroyTest, this),
          500);
    }
  }

 private:
  void CreateOSRBrowser(const CefString& url) {
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;

#if defined(OS_WIN)
    windowInfo.SetAsWindowless(GetDesktopWindow());
#elif defined(OS_MACOSX)
    windowInfo.SetAsWindowless(kNullWindowHandle);
#elif defined(OS_LINUX)
    windowInfo.SetAsWindowless(kNullWindowHandle);
#else
#error "Unsupported platform"
#endif
    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, NULL);
  }

  void HideEditBox(CefRefPtr<CefBrowser> browser) {
    // Set focus on edit box
    // This should trigger Location update if enabled
    browser->GetMainFrame()->ExecuteJavaScript(
        "document.getElementById('editbox').style.display = 'none';", kTestUrl,
        0);
  }

  void SetFocusOnEditBox(CefRefPtr<CefBrowser> browser) {
    // Set focus on edit box
    // This should trigger accessibility update if enabled
    browser->GetMainFrame()->ExecuteJavaScript(
        "document.getElementById('editbox').focus();", kTestUrl, 0);
  }

  void DisableAccessibility(CefRefPtr<CefBrowser> browser) {
    browser->GetHost()->SetAccessibilityState(STATE_DISABLED);
    accessibility_disabled_ = true;
    // Set focus on edit box
    SetFocusOnEditBox(browser);

    // Now Post a delayed task to destroy the test
    // giving sufficient time for any accessibility updates to come through
    CefPostDelayedTask(
        TID_UI, base::Bind(&AccessibilityTestHandler::DestroyTest, this), 500);
  }

  void TestEnableAccessibilityUpdate(CefRefPtr<CefValue> value) {
    // Validate enabling accessibility change returns valid accessibility tree.
    // Change has a valid non empty list.
    EXPECT_TRUE(value->GetType() == VTYPE_LIST);
    CefRefPtr<CefListValue> list = value->GetList();
    EXPECT_TRUE(list.get());
    EXPECT_GT(list->GetSize(), (size_t)0);

    // Get the first update dict and validate event type.
    CefRefPtr<CefDictionaryValue> dict = list->GetDictionary(0);

    // Ignore other events.
    if (dict->GetString("event_type") != "layoutComplete")
      return;

    // Get update and validate it has tree data
    CefRefPtr<CefDictionaryValue> update = dict->GetDictionary("update");
    EXPECT_TRUE(update.get());
    EXPECT_TRUE(update->GetBool("has_tree_data"));
    CefRefPtr<CefDictionaryValue> treeData = update->GetDictionary("tree_data");

    // Validate title and Url
    EXPECT_STREQ("AccessibilityTest",
                 treeData->GetString("title").ToString().c_str());
    EXPECT_STREQ(kTestUrl, treeData->GetString("url").ToString().c_str());

    // Validate node data
    CefRefPtr<CefListValue> nodes = update->GetList("nodes");
    EXPECT_TRUE(nodes.get());
    EXPECT_GT(nodes->GetSize(), (size_t)0);

    // Update has a valid root
    CefRefPtr<CefDictionaryValue> root;
    for (size_t index = 0; index < nodes->GetSize(); index++) {
      CefRefPtr<CefDictionaryValue> node = nodes->GetDictionary(index);
      if (node->GetString("role").ToString() == "rootWebArea") {
        root = node;
        break;
      }
    }
    EXPECT_TRUE(root.get());

    // One div containing the tree elements.
    CefRefPtr<CefListValue> childIDs = root->GetList("child_ids");
    EXPECT_TRUE(childIDs.get());
    EXPECT_EQ(childIDs->GetSize(), (size_t)1);

    // A parent Group div containing the child.
    CefRefPtr<CefDictionaryValue> group;
    for (size_t index = 0; index < nodes->GetSize(); index++) {
      CefRefPtr<CefDictionaryValue> node = nodes->GetDictionary(index);
      if (node->GetString("role").ToString() == "genericContainer") {
        group = node;
        break;
      }
    }
    EXPECT_TRUE(group.get());
    // Validate Group is child of root WebArea.
    EXPECT_EQ(group->GetInt("id"), childIDs->GetInt(0));

    CefRefPtr<CefListValue> parentdiv = group->GetList("child_ids");
    EXPECT_TRUE(parentdiv.get());
    EXPECT_EQ(parentdiv->GetSize(), (size_t)3);

    int tipId = parentdiv->GetInt(0);
    int editBoxId = parentdiv->GetInt(1);
    int buttonId = parentdiv->GetInt(2);

    // A parent Group div containing the child.
    CefRefPtr<CefDictionaryValue> tip, editbox, button;
    for (size_t index = 0; index < nodes->GetSize(); index++) {
      CefRefPtr<CefDictionaryValue> node = nodes->GetDictionary(index);
      if (node->GetInt("id") == tipId) {
        tip = node;
      }
      if (node->GetInt("id") == editBoxId) {
        editbox = node;
      }
      if (node->GetInt("id") == buttonId) {
        button = node;
      }
    }
    EXPECT_TRUE(tip.get());
    EXPECT_STREQ("tooltip", tip->GetString("role").ToString().c_str());
    CefRefPtr<CefDictionaryValue> tipattr = tip->GetDictionary("attributes");
    EXPECT_TRUE(tipattr.get());

    EXPECT_TRUE(editbox.get());
    EXPECT_STREQ("textField", editbox->GetString("role").ToString().c_str());
    CefRefPtr<CefDictionaryValue> editattr =
        editbox->GetDictionary("attributes");
    // Validate ARIA Description tags for tipIdare associated with editbox.
    EXPECT_TRUE(editattr.get());
    EXPECT_EQ(tipId, editattr->GetList("describedbyIds")->GetInt(0));
    EXPECT_STREQ(kTipText,
                 editattr->GetString("description").ToString().c_str());

    EXPECT_TRUE(button.get());
    EXPECT_STREQ("button", button->GetString("role").ToString().c_str());

    // Now Post a delayed task to destroy the test
    // giving sufficient time for any accessibility updates to come through
    CefPostDelayedTask(
        TID_UI, base::Bind(&AccessibilityTestHandler::DestroyTest, this), 500);
  }

  // Find Edit box Id in accessibility tree.
  void SetEditBoxIdAndRect(CefRefPtr<CefDictionaryValue> value) {
    EXPECT_TRUE(value.get());
    // Validate node data.
    CefRefPtr<CefListValue> nodes = value->GetList("nodes");
    EXPECT_TRUE(nodes.get());
    EXPECT_GT(nodes->GetSize(), (size_t)0);

    // Find accessibility id for the text field.
    for (size_t index = 0; index < nodes->GetSize(); index++) {
      CefRefPtr<CefDictionaryValue> node = nodes->GetDictionary(index);
      if (node->GetString("role").ToString() == "textField") {
        edit_box_id_ = node->GetInt("id");
        CefRefPtr<CefDictionaryValue> loc = node->GetDictionary("location");
        EXPECT_TRUE(loc.get());
        edit_box_rect_.x = loc->GetInt("x");
        edit_box_rect_.y = loc->GetInt("y");
        edit_box_rect_.width = loc->GetInt("width");
        edit_box_rect_.height = loc->GetInt("height");
        break;
      }
    }
  }

  AccessibilityTestType test_type_;
  int edit_box_id_;
  CefRect edit_box_rect_;
  bool accessibility_disabled_;

  IMPLEMENT_REFCOUNTING(AccessibilityTestHandler);
};

}  // namespace

TEST(OSRTest, AccessibilityEnable) {
  CefRefPtr<AccessibilityTestHandler> handler =
      new AccessibilityTestHandler(TEST_ENABLE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, AccessibilityDisable) {
  CefRefPtr<AccessibilityTestHandler> handler =
      new AccessibilityTestHandler(TEST_DISABLE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, AccessibilityFocusChange) {
  CefRefPtr<AccessibilityTestHandler> handler =
      new AccessibilityTestHandler(TEST_FOCUS_CHANGE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, AccessibilityLocationChange) {
  CefRefPtr<AccessibilityTestHandler> handler =
      new AccessibilityTestHandler(TEST_LOCATION_CHANGE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
