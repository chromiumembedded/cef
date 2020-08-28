// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_NODE_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_NODE_H_
#pragma once

#include <vector>

#include "include/cef_browser.h"

#if defined(OS_MAC)
typedef void CefNativeAccessible;
#if __OBJC__
#if __has_feature(objc_arc)
#define CAST_CEF_NATIVE_ACCESSIBLE_TO_NSOBJECT(accessible) \
  (__bridge NSObject*)accessible
#define CAST_NSOBJECT_TO_CEF_NATIVE_ACCESSIBLE(object) \
  (__bridge CefNativeAccessible*)object
#else  // __has_feature(objc_arc)
#define CAST_CEF_NATIVE_ACCESSIBLE_TO_NSOBJECT(accessible) (NSObject*)accessible
#define CAST_NSOBJECT_TO_CEF_NATIVE_ACCESSIBLE(object) \
  (__bridge CefNativeAccessible*)object
#endif  // __has_feature(objc_arc)
#endif  // __OBJC__
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
  static OsrAXNode* CreateNode(const CefString& treeId,
                               int nodeId,
                               CefRefPtr<CefDictionaryValue> value,
                               OsrAccessibilityHelper* helper);

  // Update Value.
  void UpdateValue(CefRefPtr<CefDictionaryValue> value);

  // UpdateLocation
  void UpdateLocation(CefRefPtr<CefDictionaryValue> value);

  // Fire a platform-specific notification that an event has occurred on
  // this object.
  void NotifyAccessibilityEvent(std::string event_type) const;

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  void Destroy();

  // Return NSAccessibility Object for Mac/ IAccessible for Windows
  CefNativeAccessible* GetNativeAccessibleObject(OsrAXNode* parent);

  CefNativeAccessible* GetParentAccessibleObject() const {
    return parent_ ? parent_->platform_accessibility_ : nullptr;
  }

  OsrAccessibilityHelper* GetAccessibilityHelper() const {
    return accessibility_helper_;
  }

  int GetChildCount() const;

  // Return the Child at the specified index
  OsrAXNode* ChildAtIndex(int index) const;

  const CefString& AxRole() const { return role_; }

  const CefString& OsrAXTreeId() const { return tree_id_; }

  int OsrAXNodeId() const { return node_id_; }

  const CefString& AxValue() const { return value_; }

  const CefString& AxName() const { return name_; }

  const CefString& AxDescription() const { return description_; }

  CefRect AxLocation() const;

  CefWindowHandle GetWindowHandle() const;

  CefRefPtr<CefBrowser> GetBrowser() const;

  void SetParent(OsrAXNode* parent);

 protected:
  OsrAXNode(const CefString& treeId,
            int nodeId,
            CefRefPtr<CefDictionaryValue> value,
            OsrAccessibilityHelper* helper);

  CefString tree_id_;
  int node_id_;
  CefString child_tree_id_;
  CefString role_;
  CefString value_;
  CefString name_;
  CefString description_;
  CefRect location_;
  CefPoint scroll_;
  std::vector<int> child_ids_;
  CefNativeAccessible* platform_accessibility_;
  OsrAXNode* parent_;
  int offset_container_id_;
  OsrAccessibilityHelper* accessibility_helper_;
  CefRefPtr<CefDictionaryValue> attributes_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_NODE_H_
