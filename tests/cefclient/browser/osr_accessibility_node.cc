// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

// Base class implementation for CEF Acccessibility node. This is subclassed and
// used by both IAccessible/NSAccessibility protocol implementation.

#include "tests/cefclient/browser/osr_accessibility_node.h"

#include "tests/cefclient/browser/osr_accessibility_helper.h"

namespace client {

OsrAXNode::OsrAXNode(CefRefPtr<CefDictionaryValue> value,
                     OsrAccessibilityHelper* helper)
    : node_id_(-1),
      platform_accessibility_(NULL),
      parent_(NULL),
      accessibility_helper_(helper) {
  UpdateValue(value);
}

void OsrAXNode::UpdateValue(CefRefPtr<CefDictionaryValue> value) {
  if (value && value->HasKey("id")) {
    node_id_ = value->GetInt("id");

    if (value->HasKey("role"))
      role_ = value->GetString("role");

    if (value->HasKey("child_ids")) {
      CefRefPtr<CefListValue> childs = value->GetList("child_ids");
      // Reset child Ids
      child_ids_.clear();
      for (size_t idx = 0; idx < childs->GetSize(); idx++)
        child_ids_.push_back(childs->GetInt(idx));
    }
    // Update Location
    if (value->HasKey("location")) {
      CefRefPtr<CefDictionaryValue> loc = value->GetDictionary("location");
      if (loc) {
        location_ = CefRect(loc->GetDouble("x"), loc->GetDouble("y"),
                            loc->GetDouble("width"), loc->GetDouble("height"));
      }
    }
    // Update offsets
    if (value->HasKey("offset_container_id")) {
      offset_container_id_ = value->GetInt("offset_container_id");
    }
    // Update attributes
    if (value->HasKey("attributes")) {
      attributes_ = value->GetDictionary("attributes");

      if (attributes_ && attributes_->HasKey("name"))
        name_ = attributes_->GetString("name");
      if (attributes_ && attributes_->HasKey("value"))
        value_ = attributes_->GetString("value");
      if (attributes_ && attributes_->HasKey("description"))
        description_ = attributes_->GetString("description");
    }
  }
}

CefWindowHandle OsrAXNode::GetWindowHandle() const {
  if (accessibility_helper_)
    return accessibility_helper_->GetWindowHandle();
  return NULL;
}

CefRefPtr<CefBrowser> OsrAXNode::GetBrowser() const {
  if (accessibility_helper_)
    return accessibility_helper_->GetBrowser();
  return NULL;
}

void OsrAXNode::SetParent(OsrAXNode* parent) {
  parent_ = parent;
}

CefRect OsrAXNode::AxLocation() const {
  CefRect loc = location_;
  OsrAXNode* offsetNode = accessibility_helper_->GetNode(offset_container_id_);
  // Add offset from parent Lcoation
  if (offsetNode) {
    CefRect offset = offsetNode->AxLocation();
    loc.x += offset.x;
    loc.y += offset.y;
  }
  return loc;
}

OsrAXNode* OsrAXNode::ChildAtIndex(int index) const {
  if (index < GetChildCount())
    return accessibility_helper_->GetNode(child_ids_[index]);
  else
    return NULL;
}

// Create and return the platform specific OsrAXNode Object
OsrAXNode* OsrAXNode::CreateNode(CefRefPtr<CefDictionaryValue> value,
                                 OsrAccessibilityHelper* helper) {
  return new OsrAXNode(value, helper);
}

}  // namespace client
