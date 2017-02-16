// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_pack_strings.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_scroll_view.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/views/test_window_delegate.h"
#include "tests/gtest/include/gtest/gtest.h"

#define SCROLL_VIEW_TEST_ASYNC(name) \
    UI_THREAD_TEST_ASYNC(ViewsScrollViewTest, name)

namespace {

const int kScrollViewID = 1;
const int kContentPanelID = 2;

// Make the Panel larger then the Window so scroll bars appear.
const int kContentPanelSize = TestWindowDelegate::kWSize + 200;

class TestScrollViewDelegate : public CefViewDelegate {
 public:
  TestScrollViewDelegate() {
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    EXPECT_EQ(kScrollViewID, view->GetID());
    got_get_preferred_size_ = true;
    return CefSize(kContentPanelSize, kContentPanelSize);
  }

  bool got_get_preferred_size_ = false;

 private:
  IMPLEMENT_REFCOUNTING(TestScrollViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestScrollViewDelegate);
};

class TestPanelDelegate : public CefPanelDelegate {
 public:
  TestPanelDelegate() {
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    EXPECT_EQ(kContentPanelID, view->GetID());
    got_get_preferred_size_ = true;
    return CefSize(kContentPanelSize, kContentPanelSize);
  }

  bool got_get_preferred_size_ = false;

 private:
  IMPLEMENT_REFCOUNTING(TestPanelDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestPanelDelegate);
};

void RunScrollViewLayout(bool with_delegate,
                         CefRefPtr<CefWindow> window) {
  CefRefPtr<TestScrollViewDelegate> scroll_view_delegate;
  CefRefPtr<TestPanelDelegate> panel_delegate;
  if (with_delegate) {
    scroll_view_delegate = new TestScrollViewDelegate();
    panel_delegate = new TestPanelDelegate();
  }

  CefRefPtr<CefScrollView> scroll_view =
      CefScrollView::CreateScrollView(scroll_view_delegate);
  EXPECT_TRUE(scroll_view.get());
  EXPECT_TRUE(scroll_view->AsScrollView().get());

  // Verify default state.
  EXPECT_FALSE(scroll_view->GetContentView().get());
  EXPECT_EQ(CefRect(0, 0, 0, 0), scroll_view->GetVisibleContentRect());
  EXPECT_FALSE(scroll_view->HasHorizontalScrollbar());
  EXPECT_FALSE(scroll_view->HasVerticalScrollbar());

  scroll_view->SetID(kScrollViewID);
  scroll_view->SetBackgroundColor(CefColorSetARGB(255, 0, 255, 0));

  CefRefPtr<CefPanel> content_panel = CefPanel::CreatePanel(panel_delegate);
  content_panel->SetID(kContentPanelID);
  content_panel->SetBackgroundColor(CefColorSetARGB(255, 255, 0, 0));

  if (!with_delegate) {
    // Explicitly set the content panel size. Otherwise, it will come from the
    // delegate.
    content_panel->SetSize(CefSize(kContentPanelSize, kContentPanelSize));
  }

  scroll_view->SetContentView(content_panel);
  EXPECT_TRUE(content_panel->IsSame(scroll_view->GetContentView()));

  window->AddChildView(scroll_view);

  // Force layout.
  window->Layout();

  EXPECT_TRUE(scroll_view->HasHorizontalScrollbar());
  EXPECT_TRUE(scroll_view->HasVerticalScrollbar());

  if (with_delegate) {
    EXPECT_TRUE(scroll_view_delegate->got_get_preferred_size_);
    EXPECT_TRUE(panel_delegate->got_get_preferred_size_);
  }

  window->Show();

  // With default FillLayout the ScrollView should be the size of the Window's
  // client area.
  const CefRect& client_bounds = window->GetClientAreaBoundsInScreen();
  const CefRect& scroll_view_bounds = scroll_view->GetBoundsInScreen();
  EXPECT_EQ(client_bounds, scroll_view_bounds);

  // Content panel size should be unchanged.
  const CefSize& content_panel_size = content_panel->GetSize();
  EXPECT_EQ(CefSize(kContentPanelSize, kContentPanelSize), content_panel_size);

  const int sb_height = scroll_view->GetHorizontalScrollbarHeight();
  EXPECT_GT(sb_height, 0);
  const int sb_width = scroll_view->GetVerticalScrollbarWidth();
  EXPECT_GT(sb_width, 0);

  // Verify visible content panel region.
  const CefRect& visible_rect = scroll_view->GetVisibleContentRect();
  EXPECT_EQ(CefRect(0, 0, scroll_view_bounds.width - sb_width,
                    scroll_view_bounds.height - sb_height), visible_rect);
}

void ScrollViewLayout(CefRefPtr<CefWaitableEvent> event,
                      bool with_delegate) {
  TestWindowDelegate::Config config;
  config.on_window_created = base::Bind(RunScrollViewLayout, with_delegate);
  TestWindowDelegate::RunTest(event, config);
}

void ScrollViewLayoutWithDelegateImpl(CefRefPtr<CefWaitableEvent> event) {
  ScrollViewLayout(event, true);
}

void ScrollViewLayoutNoDelegateImpl(CefRefPtr<CefWaitableEvent> event) {
  ScrollViewLayout(event, false);
}

}  // namespace

// Test ScrollView layout. This is primarily to exercise exposed CEF APIs and is
// not intended to comprehensively test ScrollView-related behavior (which we
// presume that Chromium is testing).
SCROLL_VIEW_TEST_ASYNC(ScrollViewLayoutWithDelegate);
SCROLL_VIEW_TEST_ASYNC(ScrollViewLayoutNoDelegate);
