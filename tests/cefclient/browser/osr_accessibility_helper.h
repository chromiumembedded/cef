// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_HELPER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_HELPER_H_

#include <map>

#include "include/cef_browser.h"

namespace client {

class OsrAXNode;
class OsrAccessibilityHelper;

class OsrAXTree {
 public:
  OsrAXTree();
  OsrAXNode* GetNode(int nodeId) const;
  void EraseNode(int nodeId);
  void UpdateTreeData(CefRefPtr<CefDictionaryValue> value);
  void AddNode(OsrAXNode* node);
  const CefString& GetParentTreeId() const { return parent_tree_id_; }
  int GetRootNodeId() const { return root_node_id_; }
  void SetRootNodeId(int nodeId) { root_node_id_ = nodeId; }

 private:
  CefString parent_tree_id_;
  int root_node_id_;
  std::map<int, OsrAXNode*> node_map_;
};

// Helper class that abstracts Renderer Accessibility tree and provides a
// uniform interface to be consumed by IAccessible interface on Windows and
// NSAccessibility implementation on Mac in CefClient.
class OsrAccessibilityHelper {
 public:
  OsrAccessibilityHelper(CefRefPtr<CefValue> value,
                         CefRefPtr<CefBrowser> browser);

  void UpdateAccessibilityTree(CefRefPtr<CefValue> value);

  void UpdateAccessibilityLocation(CefRefPtr<CefValue> value);

  OsrAXNode* GetRootNode() const;

  OsrAXNode* GetFocusedNode() const;

  CefWindowHandle GetWindowHandle() const {
    return browser_->GetHost()->GetWindowHandle();
  }

  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

  OsrAXNode* GetNode(const CefString& treeId, int nodeId) const;

  OsrAXNode* GetTreeRootNode(const CefString& treeId) const;

  static int CastToInt(CefRefPtr<CefValue> value);

 private:
  void Reset();

  void UpdateLayout(const CefString& treeId,
                    CefRefPtr<CefDictionaryValue> update);

  void UpdateFocusedNode(const CefString& treeId, int nodeId);

  // Destroy the node and remove from Map
  void DestroyNode(OsrAXNode* node);
  CefString root_tree_id_;
  CefString focused_tree_id_;
  int focused_node_id_;
  CefRefPtr<CefBrowser> browser_;
  std::map<CefString, OsrAXTree> accessibility_node_map_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_ACCESSIBILITY_HELPER_H_
