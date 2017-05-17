// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_NODE_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_NODE_H_
#pragma once

#include <vector>

#include "include/cef_browser.h"

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSObject;
#else
class NSObject;
#endif
typedef NSObject CefNativeAccessible;
#elif defined(OS_WIN)
struct IAccessible;
typedef IAccessible CefNativeAccessible;
#else
#error "Unsupported platform"
#endif

namespace client {

class OsrAccessibilityHelper;

// OsrAXNode is the base class for implementation for the NSAccessibility
// protocol for interacting with VoiceOver and other accessibility clients.
class OsrAXNode {
 public:
  // Create and return the platform specific OsrAXNode Object.
  static OsrAXNode* CreateNode(CefRefPtr<CefDictionaryValue> value,
                               OsrAccessibilityHelper* helper);

  // Update Value.
  void UpdateValue(CefRefPtr<CefDictionaryValue> value);

  // Fire a platform-specific notification that an event has occurred on
  // this object.
  void NotifyAccessibilityEvent(std::string event_type) const;

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  void Destroy();

  // Return NSAccessibility Object for Mac/ IAccessible for Windows
  CefNativeAccessible* GetNativeAccessibleObject(OsrAXNode* parent);

  CefNativeAccessible* GetParentAccessibleObject() const {
    return parent_ ? parent_->platform_accessibility_ : NULL;
  }

  OsrAccessibilityHelper* GetAccessibilityHelper() const {
    return accessibility_helper_;
  };

  int GetChildCount() const { return static_cast<int>(child_ids_.size()); }

  // Return the Child at the specified index
  OsrAXNode* ChildAtIndex(int index) const;

  const CefString& AxRole() const { return role_; }

  int OsrAXNodeId() const { return node_id_; }

  const CefString& AxValue() const { return value_; }

  const CefString& AxName() const { return name_; }

  const CefString& AxDescription() const { return description_; }

  CefRect AxLocation() const;

  CefWindowHandle GetWindowHandle() const;

  CefRefPtr<CefBrowser> GetBrowser() const;

  void SetParent(OsrAXNode* parent);

 protected:
  OsrAXNode(CefRefPtr<CefDictionaryValue> value,
            OsrAccessibilityHelper* helper);

  int node_id_;
  CefString role_;
  CefString value_;
  CefString name_;
  CefString description_;
  CefRect location_;
  std::vector<int> child_ids_;
  CefNativeAccessible* platform_accessibility_;
  OsrAXNode* parent_;
  int offset_container_id_;
  OsrAccessibilityHelper* accessibility_helper_;
  CefRefPtr<CefDictionaryValue> attributes_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_NODE_H_
