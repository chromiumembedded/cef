// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_VIEW_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_VIEW_IMPL_H_
#pragma once

// CEF exposes views framework functionality via a hierarchy of CefView and
// related objects. While the goal is to accurately represent views framework
// capabilities there is not always a direct 1:1 mapping between the CEF
// implementation and the underlying views implementation. Certain liberties
// have been taken with the CEF API design to clarify the user experience.
//
// CEF implementation overview:
//
// CefView-derived classes (CefPanel, CefLabelButton, etc.) are implemented
// using a specialization of the CefViewImpl template. On Initialize() the
// CefViewImpl object creates an underlying views::View object via the
// CreateRootView() method. The views::View objects are implemented using a
// specialization of the CefViewView template. CefViewView extends the
// views::View-derived class and executes CefViewDelegate-derived callbacks by
// overriding views::View methods.
//
// Example 1: The CefBasicPanelImpl object created via CefPanel::CreatePanel()
// has the following object hierarchy:
//
//   CefView => CefPanel =>
//   CefViewImpl<views::View, CefPanel, CefPanelDelegate> =>
//   CefPanelImpl<views::View, CefPanel, CefPanelDelegate> =>
//   CefBasicPanelImpl.
//
// And the CefBasicPanelView object created via
// CefBasicPanelImpl::CreateRootView() has the following object hierarchy:
//
//   views::View =>
//   CefViewView<views::View, CefPanelDelegate> =>
//   CefPanelView<views::View, CefPanelDelegate> =>
//   CefBasicPanelView.
//
// Example 2: In some cases an intermediary type is required to meet CEF
// template requirements (e.g. CefViewView requires a no-argument constructor). 
// The CefBasicLabelButtonImpl object created via
// CefLabelButton::CreateLabelButton() has the following object hierarchy:
//
//   CefView => CefButton => CefLabelButton =>
//   CefViewImpl<views::LabelButton, CefLabelButton, CefButtonDelegate> =>
//   CefButtonImpl<views::LabelButton, CefLabelButton, CefButtonDelegate> =>
//   CefLabelButtonImpl<views::LabelButton, CefLabelButton,
//                      CefButtonDelegate> =>
//   CefBasicLabelButtonImpl
//
// And the CefBasicLabelButtonView object created via
// CefBasicLabelButtonImpl::CreateRootView() has the following object hierarchy:
//
//   views::View => views::Button => views::CustomButton =>
//   views::LabelButton =>
//   LabelButtonEx (used to implement the required no-argument constructor) =>
//   CefViewView<LabelButtonEx, CefButtonDelegate> =>
//   CefButtonView<LabelButtonEx, CefButtonDelegate> =>
//   CefLabelButtonView<LabelButtonEx, CefButtonDelegate> =>
//   CefBasicLabelButtonView.
//
//
// General design considerations:
//
// CefView classes are ref-counted whereas views::View classes are not. There
// is generally a 1:1 relationship between CefView and views::View objects.
// However, there may be intermediary views::View objects that are not exposed
// by the CEF layer. For example:
// - views::Widget creates views::RootView and views::ContentView child objects;
// - views::ScrollView creates views::ScrollView::Viewport child objects.
//
// The views::View class exposes methods that are not applicable for a subset of
// views implementations. For example:
// - Calling AddChildView() on a views::LabelButton is unexpected;
// - Adding a child to a views::ScrollView requires calling the SetContents()
//   method instead of AddChildView().
// To avoid user confusion CEF introduces a CefPanel type that extends CefView
// and exposes common child management functionality. Types that allow
// arbitrary children extend CefPanel instead of CefView.
//
//
// Object ownership considerations:
//
// On initial creation the CefViewImpl object owns an underlying views::View
// object (created by overriding the CreateRootView() method) and the
// views::View object holds a non-ref-counted reference to the CefViewImpl
// object. If a CefViewImpl is destroyed (all refs released) then the underlying
// views::View object is deleted.
//
// When a views::View object is parented to another views::View (via
// CefPanel::AddChildView or similar) the ownership semantics change. The
// CefViewImpl swaps its owned reference for an unowned reference and the
// views::View gains a ref-counted reference to the CefViewImpl
// (CefView::IsAttached() now returns true).
//
// When a parent views::View is deleted all child views::Views in the view
// hierarchy are also deleted (see [1] for exceptions). When this happens the
// ref-counted CefViewImpl reference held by the views::View is released. The
// CefViewImpl is deleted if the client kept no references, otherwise the
// CefViewImpl is marked as invalid (CefView::IsValid() now returns false).
//
// When a views::View is removed from the view hierarchy (via
// CefPanel::RemoveChildView or similar) the initial ownership state is
// restored. The CefViewImpl regains ownership of the views::View and the
// ref-counted CefViewImpl reference held by the views::View is released.
//
// The relationship between CefViewImpl and views::View objects is managed using
// the view_util:: functions. Type conversion is facilitated using the As*()
// methods exposed by CefView-derived classes and the CefViewAdapter interface
// implemented by CefViewImpl. See view_util.[cc|h] for implementation details.
//
// Some other object types are also tied to views::View lifetime. For example,
// CefLayout and the underling views::LayoutManager objects are owned by the
// views::View that they're assigned to. This relationship is managed using the
// layout_util:: functions in layout_util.[cc|h].
//
// [1] By default views::View objects are deleted when the parent views::View
//     object is deleted. However, this behavior can be changed either
//     explicitly by calling set_owned_by_client() or implicitly by using
//     interfaces like WidgetDelegateView (where WidgetDelegate is-a View, and
//     the View is deleted when the native Widget is destroyed). CEF
//     implementations that utilize this behavior must take special care with
//     object ownership management.
//
//
// To implement a new CefView-derived class:
//
// 1. Choose a views class to expose.
//    * We'll create a new CefFooBar class which exposes a hypothetical
//      views::FooBar class. The views::FooBar class might look like this:
//
//      File ui/views/foo_bar.h:
//
//      namespace views {
//
//      // FooBar view does a task on child views.
//      class FooBar : public View {
//       public:
//        FooBar();
//
//        // Do a task.
//        void DoTask();
//        // Called when the task is done.
//        virtual void OnTaskDone();
//
//        // View methods:
//        void Layout() override;  // Implements custom layout of child views.
//      };
//
//      }  // namespace views
//
// 2. Determine the existing CefView-derived class that the new view class
//    should extend.
//    * Since in this example CefFooBar can have arbitrary child views we'll
//      have it extend CefPanel.
//
// 3. Determine whether the new view class can use an existing delegate class
//    (like CefPanelDelegate) or whether it needs its own delegate class.
//    * Since CefFooBar has an OnTaskDone() callback we'll add a new
//      CefFooBarDelegate class to expose it.
//
// 4. Create new header files in the cef/include/views/ directory.
//    * Using existing files as a model, the resulting header contents might
//      look like this:
//
//      File cef/include/views/cef_foo_bar.h:
//
//      ///
//      // A FooBar view does a task on child views.
//      ///
//      /*--cef(source=library)--*/
//      class CefFooBar : public CefPanel {
//       public:
//        ///
//        // Create a new FooBar.
//        ///
//        /*--cef(optional_param=delegate)--*/
//        static CefRefPtr<CefFooBar> CreateFooBar(
//            CefRefPtr<CefFooBarDelegate> delegate);
// 
//        ///
//        // Do a task.
//        ///
//        /*--cef()--*/
//        virtual void DoTask() =0;
//      };
//
//      File cef/include/views/cef_foo_bar_delegate.h:
//
//      ///
//      // Implement this interface to handle FooBar events.
//      ///
//      /*--cef(source=client)--*/
//      class CefFooBarDelegate : public CefPanelDelegate {
//       public:
//        ///
//        // Called when the task is done.
//        ///
//        /*--cef()--*/
//        virtual void OnTaskDone(CefRefPtr<CefFooBar> foobar) {}
//      };
//
// 5. Add an As*() method to the CefView-derived class.
//    * Using existing file contents as a model, make the following changes in
//      cef/include/views/cef_panel.h:
//      * Forward declare the CefFooBar class.
//      * Add a new CefPanel::AsFooBar() method:
//
//        ///
//        // Returns this Panel as a FooBar or NULL if this is not a FooBar.
//        ///
//        /*--cef()--*/
//        virtual CefRefPtr<CefFooBar> AsFooBar() =0;
//
// 6. Add a default implementation for the As*() method to the CefViewImpl-
//    derived class.
//    * Using existing file contents as a model, make the following changes in
//      cef/libcef/browser/views/panel_impl.h:
//      * Include "include/views/cef_foo_bar.h".
//      * Add a default CefPanelImpl::AsFooBar() implementation:
//
//        CefRefPtr<CefFooBar> AsFooBar() override { return nullptr; }
//
// 7. Update the CefViewAdapter::GetFor() method implementation to call the
//    As*() method.
//    * Using existing file contents as a model, make the following changes in
//      cef/libcef/browser/views/view_adapter.cc:
//      * Include "include/views/cef_foo_bar.h".
//      * Call the AsFooBar() method to identify the adapter object:
//
//        ... if (view->AsPanel()) {
//          CefRefPtr<CefPanel> panel = view->AsPanel();
//          if (panel->AsFooBar()) {
//            adapter = static_cast<CefFooBarImpl*>(panel->AsFooBar().get());
//          } else ...
//        } else ...
//
// 8. Implement the CefViewView-derived class.
//    * Using existing files as a model (for example, CefBasicPanelView), create
//      a CefFooBarView class at cef/libcef/browser/views/foo_bar_view.[cc|h].
//      This class:
//      * Extends CefPanelView<views::FooBar, CefFooBarDelegate>.
//      * Overrides the views::FooBar::OnTaskDone method to execute the
//        CefFooBarDelegate::OnTaskDone callback:
//
//        void CefFooBarView::OnTaskDone() {
//          if (cef_delegate())
//            cef_delegate()->OnTaskDone(GetCefFooBar());
//        }
//
// 9. Implement the CefViewImpl-derived class.
//    * Use existing files as a model (for example, CefBasicPanelImpl), create a
//      CefFooBarImpl class at cef/libcef/browser/views/foo_bar_impl.[cc|h].
//      This class:
//      * Extends CefPanelImpl<views::FooBar, CefFooBar, CefFooBarDelegate>.
//      * Implements AsFooBar() to return |this|.
//      * Implements CreateRootView() to return a new CefFooBarView instance.
//      * Implements the CefFooBar::DoTask() method to call
//        views::FooBar::DoTask():
//
//        void CefFooBarImpl::DoTask() {
//          CEF_REQUIRE_VALID_RETURN_VOID();
//          root_view()->DoTask();
//        }
//
// 10. Implement the static method that creates the CefViewImpl-derived object
//     instance.
//     * Use existing files as a model (for example, CefBasicPanelImpl),
//       implement the CefFooBar::CreateFooBar static method in
//       cef/libcef/browser/views/foo_bar_impl.cc. This method:
//       * Creates a new CefFooBarImpl object.
//       * Calls Initialize() on the CefFooBarImpl object.
//       * Returns the CefFooBarImpl object.
//
// 11. Add the new source files from #7 and #8 to the 'libcef_static' target in
//     cef.gyp.
//
// 12. Update the CEF project files and build.
//     * Run cef/tools/translator.[bat|sh] to update the translation layer for
//       the new/modified classes. This tool needs to be run whenever header
//       files in the cef/include/ directory are changed.
//     * Run cef/cef_create_projects.[bat|sh] to update the Ninja build files.
//     * Build CEF using Ninja.
//

#include "include/views/cef_browser_view.h"
#include "include/views/cef_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_scroll_view.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_view.h"

#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/view_adapter.h"
#include "libcef/browser/views/view_util.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

// Helpers for template boiler-plate.
#define CEF_VIEW_IMPL_T \
    template <class ViewsViewClass, class CefViewClass, \
              class CefViewDelegateClass>
#define CEF_VIEW_IMPL_A \
    ViewsViewClass, CefViewClass, CefViewDelegateClass
#define CEF_VIEW_IMPL_D CefViewImpl<CEF_VIEW_IMPL_A>

// Base template for implementing CefView-derived classes. See above comments
// for a usage overview.
CEF_VIEW_IMPL_T class CefViewImpl : public CefViewAdapter,
                                    public CefViewClass {
 public:
  // Necessary for the CEF_REQUIRE_VALID_*() macros to compile.
  typedef CEF_VIEW_IMPL_D ParentClass;

  // Returns the content views::View object that should be the target of most
  // customization actions. May be the root view or a child of the root view.
  virtual views::View* content_view() const { return root_view(); }

  // Returns the CEF delegate as the derived type which may be nullptr.
  CefViewDelegateClass* delegate() const { return delegate_.get(); }

  // Returns the root views::View object owned by this CefView.
  ViewsViewClass* root_view() const { return root_view_ref_; }

  // CefViewAdapter methods:
  views::View* Get() const override {
    return root_view();
  }
  std::unique_ptr<views::View> PassOwnership() override {
    DCHECK(root_view_);
    return std::move(root_view_);
  }
  void ResumeOwnership() override {
    DCHECK(root_view_ref_);
    DCHECK(!root_view_);
    root_view_.reset(root_view_ref_);
  }
  void Detach() override {
    if (root_view_)
      root_view_.reset();
    root_view_ref_ = nullptr;
  }
  void GetDebugInfo(base::DictionaryValue* info,
                    bool include_children) override {
    info->SetString("type", GetDebugType());
    info->SetInteger("id", root_view()->id());

    // Use GetBounds() because some subclasses (like CefWindowImpl) override it.
    const CefRect& bounds = GetBounds();
    std::unique_ptr<base::DictionaryValue> bounds_value(new base::DictionaryValue());
    bounds_value->SetInteger("x", bounds.x);
    bounds_value->SetInteger("y", bounds.y);
    bounds_value->SetInteger("width", bounds.width);
    bounds_value->SetInteger("height", bounds.height);
    info->Set("bounds", std::move(bounds_value));
  }

  // CefView methods. When adding new As*() methods make sure to update
  // CefViewAdapter::GetFor() in view_adapter.cc.
  CefRefPtr<CefBrowserView> AsBrowserView() override { return nullptr; }
  CefRefPtr<CefButton> AsButton() override { return nullptr; }
  CefRefPtr<CefPanel> AsPanel() override { return nullptr; }
  CefRefPtr<CefScrollView> AsScrollView() override { return nullptr; }
  CefRefPtr<CefTextfield> AsTextfield() override { return nullptr; }
  CefString GetTypeString() override;
  CefString ToString(bool include_children) override;
  bool IsValid() override;
  bool IsAttached() override;
  bool IsSame(CefRefPtr<CefView> that) override;
  CefRefPtr<CefViewDelegate> GetDelegate() override;
  CefRefPtr<CefWindow> GetWindow() override;
  int GetID() override;
  void SetID(int id) override;
  int GetGroupID() override;
  void SetGroupID(int group_id) override;
  CefRefPtr<CefView> GetParentView() override;
  CefRefPtr<CefView> GetViewForID(int id) override;
  void SetBounds(const CefRect& bounds) override;
  CefRect GetBounds() override;
  CefRect GetBoundsInScreen() override;
  void SetSize(const CefSize& size) override;
  CefSize GetSize() override;
  void SetPosition(const CefPoint& position) override;
  CefPoint GetPosition() override;
  CefSize GetPreferredSize() override;
  void SizeToPreferredSize() override;
  CefSize GetMinimumSize() override;
  CefSize GetMaximumSize() override;
  int GetHeightForWidth(int width) override;
  void InvalidateLayout() override;
  void SetVisible(bool visible) override;
  bool IsVisible() override;
  bool IsDrawn() override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() override;
  void SetFocusable(bool focusable) override;
  bool IsFocusable() override;;
  bool IsAccessibilityFocusable() override;
  void RequestFocus() override;
  void SetBackgroundColor(cef_color_t color) override;
  cef_color_t GetBackgroundColor() override;
  bool ConvertPointToScreen(CefPoint& point) override;
  bool ConvertPointFromScreen(CefPoint& point) override;
  bool ConvertPointToWindow(CefPoint& point) override;
  bool ConvertPointFromWindow(CefPoint& point) override;
  bool ConvertPointToView(CefRefPtr<CefView> view,
                          CefPoint& point) override;
  bool ConvertPointFromView(CefRefPtr<CefView> view,
                            CefPoint& point) override;

 protected:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefViewImpl(CefRefPtr<CefViewDelegateClass> delegate)
      : delegate_(delegate),
        root_view_ref_(nullptr) {
  }

  // Initialize this object.
  virtual void Initialize() {
    root_view_.reset(CreateRootView());
    DCHECK(root_view_.get());
    root_view_ref_ = root_view_.get();
    view_util::Register(this);
    InitializeRootView();
  }

  // Create the root views::View object.
  virtual ViewsViewClass* CreateRootView() = 0;

  // Perform required initialization of the root_view() object created by
  // CreateRootView(). Called after this object has been registered.
  virtual void InitializeRootView() = 0;

 private:
  CefRefPtr<CefViewDelegateClass> delegate_;

  // Owned reference to the views::View wrapped by this object. Will be nullptr
  // before the View is created and after the View's ownership is transferred.
  std::unique_ptr<ViewsViewClass> root_view_;

  // Unowned reference to the views::View wrapped by this object. Will be
  // nullptr before the View is created and after the View is destroyed.
  ViewsViewClass* root_view_ref_;
};

CEF_VIEW_IMPL_T CefString CEF_VIEW_IMPL_D::GetTypeString() {
  CEF_REQUIRE_UIT_RETURN(CefString());
  return GetDebugType();
}

CEF_VIEW_IMPL_T CefString CEF_VIEW_IMPL_D::ToString(bool include_children) {
  CEF_REQUIRE_UIT_RETURN(CefString());
  std::unique_ptr<base::DictionaryValue> info(new base::DictionaryValue());
  if (IsValid())
    GetDebugInfo(info.get(), include_children);
  else
    info->SetString("type", GetDebugType());

  std::string json_string;
  base::JSONWriter::WriteWithOptions(*info, 0, &json_string);
  return json_string;
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsValid() {
  CEF_REQUIRE_UIT_RETURN(false);
  return !!root_view_ref_; 
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsAttached() {
  CEF_REQUIRE_UIT_RETURN(false);
  return !root_view_.get();
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsSame(CefRefPtr<CefView> that) {
  CEF_REQUIRE_UIT_RETURN(false);
  CefViewImpl* that_impl = static_cast<CefViewImpl*>(that.get());
  if (!that_impl)
    return false;
  return this == that_impl;
}

CEF_VIEW_IMPL_T CefRefPtr<CefViewDelegate> CEF_VIEW_IMPL_D::GetDelegate() {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return delegate();
}

CEF_VIEW_IMPL_T CefRefPtr<CefWindow> CEF_VIEW_IMPL_D::GetWindow() {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  if (root_view())
    return view_util::GetWindowFor(root_view()->GetWidget());
  return nullptr;
}

CEF_VIEW_IMPL_T int CEF_VIEW_IMPL_D::GetID() {
  CEF_REQUIRE_VALID_RETURN(0);
  return root_view()->id();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetID(int id) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->set_id(id);
}


CEF_VIEW_IMPL_T int CEF_VIEW_IMPL_D::GetGroupID() {
  CEF_REQUIRE_VALID_RETURN(0);
  return root_view()->GetGroup();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetGroupID(int group_id) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view()->GetGroup() != -1)
    return;
  root_view()->SetGroup(group_id);
}

CEF_VIEW_IMPL_T CefRefPtr<CefView> CEF_VIEW_IMPL_D::GetParentView() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  views::View* view = root_view()->parent();
  if (!view)
    return nullptr;
  return view_util::GetFor(view, true);
}

CEF_VIEW_IMPL_T CefRefPtr<CefView> CEF_VIEW_IMPL_D::GetViewForID(int id) {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  views::View* view = root_view()->GetViewByID(id);
  if (!view)
    return nullptr;
  return view_util::GetFor(view, true);
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetBounds(const CefRect& bounds) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetBoundsRect(
      gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height));
}

CEF_VIEW_IMPL_T CefRect CEF_VIEW_IMPL_D::GetBounds() {
  CEF_REQUIRE_VALID_RETURN(CefRect());
  const gfx::Rect& bounds = root_view()->bounds();
  return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

CEF_VIEW_IMPL_T CefRect CEF_VIEW_IMPL_D::GetBoundsInScreen() {
  CEF_REQUIRE_VALID_RETURN(CefRect());
  const gfx::Rect& bounds = root_view()->GetBoundsInScreen();
  return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetSize(const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetSize(gfx::Size(size.width, size.height));
}

CEF_VIEW_IMPL_T CefSize CEF_VIEW_IMPL_D::GetSize() {
  CEF_REQUIRE_VALID_RETURN(CefSize());
  // Call GetBounds() since child classes may override it.
  const CefRect& bounds = GetBounds();
  return CefSize(bounds.width, bounds.height);
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetPosition(const CefPoint& position) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetPosition(gfx::Point(position.x, position.y));
}

CEF_VIEW_IMPL_T CefPoint CEF_VIEW_IMPL_D::GetPosition() {
  CEF_REQUIRE_VALID_RETURN(CefPoint());
  // Call GetBounds() since child classes may override it.
  const CefRect& bounds = GetBounds();
  return CefPoint(bounds.x, bounds.y);
}

CEF_VIEW_IMPL_T CefSize CEF_VIEW_IMPL_D::GetPreferredSize() {
  CEF_REQUIRE_VALID_RETURN(CefSize());
  const gfx::Size& size = root_view()->GetPreferredSize();
  return CefSize(size.width(), size.height());
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SizeToPreferredSize() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SizeToPreferredSize();
}

CEF_VIEW_IMPL_T CefSize CEF_VIEW_IMPL_D::GetMinimumSize() {
  CEF_REQUIRE_VALID_RETURN(CefSize());
  const gfx::Size& size = root_view()->GetMinimumSize();
  return CefSize(size.width(), size.height());
}

CEF_VIEW_IMPL_T CefSize CEF_VIEW_IMPL_D::GetMaximumSize() {
  CEF_REQUIRE_VALID_RETURN(CefSize());
  const gfx::Size& size = root_view()->GetMaximumSize();
  return CefSize(size.width(), size.height());
}

CEF_VIEW_IMPL_T int CEF_VIEW_IMPL_D::GetHeightForWidth(int width) {
  CEF_REQUIRE_VALID_RETURN(0);
  return root_view()->GetHeightForWidth(width);
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::InvalidateLayout() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->InvalidateLayout();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetVisible(bool visible) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetVisible(visible);
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsVisible() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->visible();
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsDrawn() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->IsDrawn();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetEnabled(bool enabled) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetEnabled(enabled);
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsEnabled() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->enabled();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetFocusable(bool focusable) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetFocusBehavior(focusable ? views::View::FocusBehavior::ALWAYS :
                                            views::View::FocusBehavior::NEVER);
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsFocusable() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->IsFocusable();
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::IsAccessibilityFocusable() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->IsAccessibilityFocusable();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::RequestFocus() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->RequestFocus();
}

CEF_VIEW_IMPL_T void CEF_VIEW_IMPL_D::SetBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  content_view()->set_background(
      views::Background::CreateSolidBackground(color));
}

CEF_VIEW_IMPL_T cef_color_t CEF_VIEW_IMPL_D::GetBackgroundColor() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return content_view()->background()->get_color();
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::ConvertPointToScreen(CefPoint& point) {
  CEF_REQUIRE_VALID_RETURN(false);
  gfx::Point gfx_point = gfx::Point(point.x, point.y);
  if (!view_util::ConvertPointToScreen(root_view(), &gfx_point, false))
    return false;
  point = CefPoint(gfx_point.x(), gfx_point.y());
  return true;
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::ConvertPointFromScreen(CefPoint& point) {
  CEF_REQUIRE_VALID_RETURN(false);
  gfx::Point gfx_point = gfx::Point(point.x, point.y);
  if (!view_util::ConvertPointFromScreen(root_view(), &gfx_point, false))
    return false;
  point = CefPoint(gfx_point.x(), gfx_point.y());
  return true;
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::ConvertPointToWindow(CefPoint& point) {
  CEF_REQUIRE_VALID_RETURN(false);
  gfx::Point gfx_point = gfx::Point(point.x, point.y);
  if (!view_util::ConvertPointToWindow(root_view(), &gfx_point))
    return false;
  point = CefPoint(gfx_point.x(), gfx_point.y());
  return true;
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::ConvertPointFromWindow(CefPoint& point) {
  CEF_REQUIRE_VALID_RETURN(false);
  gfx::Point gfx_point = gfx::Point(point.x, point.y);
  if (!view_util::ConvertPointFromWindow(root_view(), &gfx_point))
    return false;
  point = CefPoint(gfx_point.x(), gfx_point.y());
  return true;
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::ConvertPointToView(
    CefRefPtr<CefView> view,
    CefPoint& point) {
  CEF_REQUIRE_VALID_RETURN(false);
  if (!root_view()->GetWidget())
    return false;
  views::View* target_view = view_util::GetFor(view);
  if (!target_view || target_view->GetWidget() != root_view()->GetWidget())
    return false;
  gfx::Point gfx_point = gfx::Point(point.x, point.y);
  views::View::ConvertPointToTarget(root_view(), target_view, &gfx_point);
  point = CefPoint(gfx_point.x(), gfx_point.y());
  return true;
}

CEF_VIEW_IMPL_T bool CEF_VIEW_IMPL_D::ConvertPointFromView(
    CefRefPtr<CefView> view,
    CefPoint& point) {
  CEF_REQUIRE_VALID_RETURN(false);
  if (!root_view()->GetWidget())
    return false;
  views::View* target_view = view_util::GetFor(view);
  if (!target_view || target_view->GetWidget() != root_view()->GetWidget())
    return false;
  gfx::Point gfx_point = gfx::Point(point.x, point.y);
  views::View::ConvertPointToTarget(target_view, root_view(), &gfx_point);
  point = CefPoint(gfx_point.x(), gfx_point.y());
  return true;
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_VIEW_IMPL_H_
