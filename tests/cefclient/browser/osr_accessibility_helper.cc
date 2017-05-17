// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_accessibility_helper.h"
#include "tests/cefclient/browser/osr_accessibility_node.h"

namespace client {

OsrAccessibilityHelper::OsrAccessibilityHelper(CefRefPtr<CefValue> value,
                                               CefRefPtr<CefBrowser> browser)
    : root_node_id_(-1), focused_node_id_(-1), browser_(browser) {
  UpdateAccessibilityTree(value);
}

void OsrAccessibilityHelper::UpdateAccessibilityTree(
    CefRefPtr<CefValue> value) {
  if (value && value->GetType() == VTYPE_LIST) {
    CefRefPtr<CefListValue> list = value->GetList();
    size_t numEvents = list->GetSize();
    if (numEvents > 0) {
      for (size_t i = 0; i < numEvents; i++) {
        CefRefPtr<CefDictionaryValue> event = list->GetDictionary(i);
        if (event && event->HasKey("event_type") && event->HasKey("update")) {
          std::string event_type = event->GetString("event_type");

          CefRefPtr<CefDictionaryValue> update = event->GetDictionary("update");
          if (event_type == "layoutComplete")
            UpdateLayout(update);
          if (event_type == "focus" && event->HasKey("id")) {
            // Update focused node id
            focused_node_id_ = event->GetInt("id");
            UpdateFocusedNode(update);
          }
        }
      }
    }
  }
}

void OsrAccessibilityHelper::UpdateLayout(
    CefRefPtr<CefDictionaryValue> update) {
  if (update) {
    CefRefPtr<CefDictionaryValue> tree_data;
    // get tree data
    if (update->HasKey("has_tree_data") && update->GetBool("has_tree_data"))
      tree_data = update->GetDictionary("tree_data");

    // If a node is to be cleared
    if (update->HasKey("node_id_to_clear")) {
      int node_id_to_clear = update->GetInt("node_id_to_clear");

      // reset root node if that is to be cleared
      if (node_id_to_clear == root_node_id_)
        root_node_id_ = -1;
      OsrAXNode* node = GetNode(node_id_to_clear);
      DestroyNode(node);
    }

    if (update->HasKey("root_id"))
      root_node_id_ = update->GetInt("root_id");

    if (tree_data && tree_data->HasKey("focus_id"))
      focused_node_id_ = tree_data->GetInt("focus_id");
    // Now initialize/update the node data.
    if (update->HasKey("nodes")) {
      CefRefPtr<CefListValue> nodes = update->GetList("nodes");

      for (size_t index = 0; index < nodes->GetSize(); index++) {
        CefRefPtr<CefDictionaryValue> node = nodes->GetDictionary(index);
        if (node) {
          int node_id = node->GetInt("id");
          OsrAXNode* axNode = GetNode(node_id);
          // Create if it is a new one
          if (axNode) {
            axNode->UpdateValue(node);
          } else {
            axNode = OsrAXNode::CreateNode(node, this);
            accessibility_node_map_[node_id] = axNode;
          }
        }
      }
    }
  }
}

void OsrAccessibilityHelper::UpdateFocusedNode(
    CefRefPtr<CefDictionaryValue> update) {
  if (update && update->HasKey("nodes")) {
    CefRefPtr<CefListValue> nodes = update->GetList("nodes");

    for (size_t index = 0; index < nodes->GetSize(); index++) {
      CefRefPtr<CefDictionaryValue> node = nodes->GetDictionary(index);
      if (node) {
        int node_id = node->GetInt("id");
        OsrAXNode* axNode = GetNode(node_id);
        // Create if it is a new one
        if (axNode) {
          axNode->UpdateValue(node);
        } else {
          axNode = OsrAXNode::CreateNode(node, this);
          accessibility_node_map_[node_id] = axNode;
        }
      }
    }
  }
  // Now Notify Screen Reader
  OsrAXNode* axNode = GetFocusedNode();
  // Fallback to Root
  if (!axNode)
    axNode = GetRootNode();

  axNode->NotifyAccessibilityEvent("focus");
}

void OsrAccessibilityHelper::Reset() {
  accessibility_node_map_.clear();
  root_node_id_ = focused_node_id_ = -1;
}

void OsrAccessibilityHelper::DestroyNode(OsrAXNode* node) {
  if (node) {
    int numChilds = node->GetChildCount();
    if (numChilds > 0) {
      for (int i = 0; i < numChilds; i++) {
        DestroyNode(node->ChildAtIndex(i));
      }
    }
    accessibility_node_map_.erase(node->OsrAXNodeId());
    node->Destroy();
  }
}

OsrAXNode* OsrAccessibilityHelper::GetNode(int nodeId) const {
  if (nodeId != -1 &&
      accessibility_node_map_.find(nodeId) != accessibility_node_map_.end()) {
    return accessibility_node_map_.at(nodeId);
  }

  return NULL;
}

}  // namespace client
