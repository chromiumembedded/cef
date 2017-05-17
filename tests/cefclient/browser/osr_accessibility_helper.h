// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_HELPER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_HELPER_H_

#include <map>

#include "include/cef_browser.h"

namespace client {

class OsrAXNode;

// Helper class that abstracts Renderer Accessibility tree and provides a
// uniform interface to be consumed by IAccessible interface on Windows and
// NSAccessibility implementation on Mac in CefClient.
class OsrAccessibilityHelper {
 public:
  OsrAccessibilityHelper(CefRefPtr<CefValue> value,
                         CefRefPtr<CefBrowser> browser);

  void UpdateAccessibilityTree(CefRefPtr<CefValue> value);

  OsrAXNode* GetRootNode() const { return GetNode(root_node_id_); }

  OsrAXNode* GetFocusedNode() const { return GetNode(focused_node_id_); }

  CefWindowHandle GetWindowHandle() const {
    return browser_->GetHost()->GetWindowHandle();
  }

  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; };

  OsrAXNode* GetNode(int nodeId) const;

 private:
  OsrAXNode* CreateNode(OsrAXNode* parent, CefRefPtr<CefDictionaryValue> value);

  void Reset();

  void UpdateLayout(CefRefPtr<CefDictionaryValue> update);

  void UpdateFocusedNode(CefRefPtr<CefDictionaryValue> update);

  // Destroy the node and remove from Map
  void DestroyNode(OsrAXNode* node);
  int root_node_id_;
  int focused_node_id_;
  CefRefPtr<CefBrowser> browser_;
  std::map<int, OsrAXNode*> accessibility_node_map_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_HELPER_H_
