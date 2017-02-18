// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_VIEW_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_VIEW_VIEW_H_
#pragma once

#include "include/views/cef_view.h"
#include "include/views/cef_view_delegate.h"

#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/view_util.h"

#include "base/logging.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

// Helpers for template boiler-plate.
#define CEF_VIEW_VIEW_T \
    template <class ViewsViewClass, class CefViewDelegateClass>
#define CEF_VIEW_VIEW_A \
    ViewsViewClass, CefViewDelegateClass
#define CEF_VIEW_VIEW_D CefViewView<CEF_VIEW_VIEW_A>

// Base template for implementing views::View-derived classes. The views::View-
// derived type passed to this template must provide a no-argument constructor
// (for example, see LabelButtonEx from basic_label_button_view.h). See comments
// in view_impl.h for a usage overview.
CEF_VIEW_VIEW_T class CefViewView : public ViewsViewClass {
 public:
  typedef ViewsViewClass ParentClass;

  // Should be created from CreateRootView() in a CefViewImpl-derived class.
  // Do not call complex views::View-derived methods from a CefViewView-derived
  // constructor as they may attempt to call back into CefViewImpl before
  // registration has been performed. |cef_delegate| may be nullptr.
  explicit CefViewView(CefViewDelegateClass* cef_delegate)
      : cef_delegate_(cef_delegate) {
  }

  // Should be called from InitializeRootView() in the CefViewImpl-derived
  // class that created this object. This method will be called after
  // CefViewImpl registration has completed so it is safe to call complex
  // views::View-derived methods here.
  virtual void Initialize() {
    // Use our defaults instead of the Views framework defaults.
    ParentClass::set_background(views::Background::CreateSolidBackground(
        view_util::kDefaultBackgroundColor));
  }

  // Returns the CefViewDelegate-derived delegate associated with this view.
  // May return nullptr.
  CefViewDelegateClass* cef_delegate() const { return cef_delegate_; }

  // Returns the CefView associated with this view. May return nullptr during
  // CefViewImpl initialization. If callbacks to the CefViewImpl-derived class
  // are required define an interface that the CefViewImpl-derived class can
  // implement and pass as an unowned instance to this object's constructor (see
  // for example CefWindowView).
  CefRefPtr<CefView> GetCefView() const {
    CefRefPtr<CefView> view = view_util::GetFor(this, false);
    DCHECK(view);
    return view;
  }

  // views::View methods:
  gfx::Size GetPreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  void ViewHierarchyChanged(
      const views::View::ViewHierarchyChangedDetails& details) override;
  void OnFocus() override;
  void OnBlur() override;

  // Return true if this View is expected to have a minimum size (for example,
  // a button where the minimum size is based on the label).
  virtual bool HasMinimumSize() const { return false; }

 private:
  void NotifyChildViewChanged(
      const views::View::ViewHierarchyChangedDetails& details);
  void NotifyParentViewChanged(
      const views::View::ViewHierarchyChangedDetails& details);

  // Not owned by this object.
  CefViewDelegateClass* cef_delegate_;
};

CEF_VIEW_VIEW_T gfx::Size CEF_VIEW_VIEW_D::GetPreferredSize() const {
  gfx::Size result;
  if (cef_delegate()) {
    CefSize cef_size = cef_delegate()->GetPreferredSize(GetCefView());
    if (!cef_size.IsEmpty())
      result = gfx::Size(cef_size.width, cef_size.height);
  }
  if (result.IsEmpty())
    result = ParentClass::GetPreferredSize();
  if (result.IsEmpty()) {
    // Some layouts like BoxLayout expect the preferred size to be non-empty.
    // The user may have set the size explicitly. Therefore return the current
    // size as the preferred size.
    result = ParentClass::size();
  }
  return result;
}

CEF_VIEW_VIEW_T gfx::Size CEF_VIEW_VIEW_D::GetMinimumSize() const {
  gfx::Size result;
  if (cef_delegate()) {
    CefSize cef_size = cef_delegate()->GetMinimumSize(GetCefView());
    if (!cef_size.IsEmpty())
      result = gfx::Size(cef_size.width, cef_size.height);
  }
  // We don't want to call ParentClass::GetMinimumSize() in all cases because
  // the default views::View implementation will call GetPreferredSize(). That
  // may result in size() being returned which keeps the View from shrinking.
  if (result.IsEmpty() && HasMinimumSize())
    result = ParentClass::GetMinimumSize();
  return result;
}

CEF_VIEW_VIEW_T gfx::Size CEF_VIEW_VIEW_D::GetMaximumSize() const {
  gfx::Size result;
  if (cef_delegate()) {
    CefSize cef_size = cef_delegate()->GetMaximumSize(GetCefView());
    if (!cef_size.IsEmpty())
      result = gfx::Size(cef_size.width, cef_size.height);
  }
  if (result.IsEmpty())
    result = ParentClass::GetMaximumSize();
  return result;
}

CEF_VIEW_VIEW_T int CEF_VIEW_VIEW_D::GetHeightForWidth(int w) const {
  int result = 0;
  if (cef_delegate())
    result = cef_delegate()->GetHeightForWidth(GetCefView(), w);
  if (result == 0)
    result = ParentClass::GetHeightForWidth(w);
  if (result == 0) {
    // Some layouts like FillLayout will ignore the preferred size if this view
    // has no children. We want to use the preferred size if not otherwise
    // specified.
    result = GetPreferredSize().height();
  }
  return result;
}

CEF_VIEW_VIEW_T void CEF_VIEW_VIEW_D::Layout() {
  ParentClass::Layout();

  // If Layout() did not provide a size then use the preferred size.
  if (ParentClass::size().IsEmpty())
    ParentClass::SizeToPreferredSize();
}

CEF_VIEW_VIEW_T void CEF_VIEW_VIEW_D::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  NotifyChildViewChanged(details);
  NotifyParentViewChanged(details);
  ParentClass::ViewHierarchyChanged(details);
}

CEF_VIEW_VIEW_T void CEF_VIEW_VIEW_D::OnFocus() {
  if (cef_delegate())
    cef_delegate()->OnFocus(GetCefView());
  ParentClass::OnFocus();
}

CEF_VIEW_VIEW_T void CEF_VIEW_VIEW_D::OnBlur() {
  if (cef_delegate())
    cef_delegate()->OnBlur(GetCefView());
  ParentClass::OnBlur();
}

CEF_VIEW_VIEW_T void CEF_VIEW_VIEW_D::NotifyChildViewChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  if (!cef_delegate())
    return;

  // Only interested with the parent is |this| object and the notification is
  // about an immediate child (notifications are also sent for grandchildren).
  if (details.parent != this || details.child->parent() != this)
    return;

  // Only notify for children that have a known CEF root view. For example,
  // don't notify when ScrollView adds child scroll bars.
  CefRefPtr<CefView> child = view_util::GetFor(details.child, false);
  if (child)
    cef_delegate()->OnChildViewChanged(GetCefView(), details.is_add, child);
}

CEF_VIEW_VIEW_T void CEF_VIEW_VIEW_D::NotifyParentViewChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  if (!cef_delegate())
    return;

  // Only interested when the child is |this| object and notification is about
  // the immediate parent (notifications are sent for all parents).
  if (details.child != this || details.parent != ParentClass::parent())
    return;

  // The immediate parent might be an intermediate view so find the closest
  // known CEF root view.
  CefRefPtr<CefView> parent = view_util::GetFor(details.parent, true);
  DCHECK(parent);
  cef_delegate()->OnParentViewChanged(GetCefView(), details.is_add, parent);
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_VIEW_VIEW_H_
