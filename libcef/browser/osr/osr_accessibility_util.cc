// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/osr/osr_accessibility_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"

namespace {
using ui::ToString;

template <typename T>
CefRefPtr<CefListValue> ToCefValue(const std::vector<T>& vecData);

template <>
CefRefPtr<CefListValue> ToCefValue<int>(const std::vector<int>& vecData) {
  CefRefPtr<CefListValue> value = CefListValue::Create();
  for (size_t i = 0; i < vecData.size(); i++)
    value->SetInt(i, vecData[i]);

  return value;
}

// Helper function for AXNodeData::ToCefValue - Converts AXState attributes to
// CefListValue.
CefRefPtr<CefListValue> ToCefValue(uint32_t state) {
  CefRefPtr<CefListValue> value = CefListValue::Create();

  int index = 0;
  // Iterate and find which states are set.
  for (unsigned i = ui::AX_STATE_NONE; i <= ui::AX_STATE_LAST; i++) {
    if (state & (1 << i))
      value->SetString(index++, ToString(static_cast<ui::AXState>(i)));
  }
  return value;
}

// Helper function for AXNodeData::ToCefValue - converts GfxRect to
// CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ToCefValue(const gfx::RectF& bounds) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();
  value->SetDouble("x", bounds.x());
  value->SetDouble("y", bounds.y());
  value->SetDouble("width", bounds.width());
  value->SetDouble("height", bounds.height());
  return value;
}

// Helper Functor for adding AxNodeData::attributes to AXNodeData::ToCefValue.
struct PopulateAxNodeAttributes {
  CefRefPtr<CefDictionaryValue> attributes;

  explicit PopulateAxNodeAttributes(CefRefPtr<CefDictionaryValue> attrs)
      : attributes(attrs) {}

  // Int Attributes
  void operator()(const std::pair<ui::AXIntAttribute, int32_t> attr) {
    if (attr.first == ui::AX_INT_ATTRIBUTE_NONE)
      return;

    switch (attr.first) {
      case ui::AX_INT_ATTRIBUTE_NONE:
        break;
      case ui::AX_ATTR_SCROLL_X:
      case ui::AX_ATTR_SCROLL_X_MIN:
      case ui::AX_ATTR_SCROLL_X_MAX:
      case ui::AX_ATTR_SCROLL_Y:
      case ui::AX_ATTR_SCROLL_Y_MIN:
      case ui::AX_ATTR_SCROLL_Y_MAX:
      case ui::AX_ATTR_HIERARCHICAL_LEVEL:
      case ui::AX_ATTR_TEXT_SEL_START:
      case ui::AX_ATTR_TEXT_SEL_END:
      case ui::AX_ATTR_ARIA_COLUMN_COUNT:
      case ui::AX_ATTR_ARIA_CELL_COLUMN_INDEX:
      case ui::AX_ATTR_ARIA_ROW_COUNT:
      case ui::AX_ATTR_ARIA_CELL_ROW_INDEX:
      case ui::AX_ATTR_TABLE_ROW_COUNT:
      case ui::AX_ATTR_TABLE_COLUMN_COUNT:
      case ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX:
      case ui::AX_ATTR_TABLE_CELL_ROW_INDEX:
      case ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN:
      case ui::AX_ATTR_TABLE_CELL_ROW_SPAN:
      case ui::AX_ATTR_TABLE_COLUMN_HEADER_ID:
      case ui::AX_ATTR_TABLE_COLUMN_INDEX:
      case ui::AX_ATTR_TABLE_HEADER_ID:
      case ui::AX_ATTR_TABLE_ROW_HEADER_ID:
      case ui::AX_ATTR_TABLE_ROW_INDEX:
      case ui::AX_ATTR_ACTIVEDESCENDANT_ID:
      case ui::AX_ATTR_IN_PAGE_LINK_TARGET_ID:
      case ui::AX_ATTR_ERRORMESSAGE_ID:
      case ui::AX_ATTR_DETAILS_ID:
      case ui::AX_ATTR_MEMBER_OF_ID:
      case ui::AX_ATTR_NEXT_ON_LINE_ID:
      case ui::AX_ATTR_PREVIOUS_ON_LINE_ID:
      case ui::AX_ATTR_CHILD_TREE_ID:
      case ui::AX_ATTR_SET_SIZE:
      case ui::AX_ATTR_POS_IN_SET:
        attributes->SetInt(ToString(attr.first), attr.second);
        break;
      case ui::AX_ATTR_DEFAULT_ACTION_VERB:
        attributes->SetString(
            ToString(attr.first),
            ui::ActionVerbToUnlocalizedString(
                static_cast<ui::AXDefaultActionVerb>(attr.second)));
        break;
      case ui::AX_ATTR_INVALID_STATE:
        if (ui::AX_INVALID_STATE_NONE != attr.second) {
          attributes->SetString(
              ToString(attr.first),
              ToString(static_cast<ui::AXInvalidState>(attr.second)));
        }
        break;
      case ui::AX_ATTR_CHECKED_STATE:
        if (ui::AX_CHECKED_STATE_NONE != attr.second) {
          attributes->SetString(
              ToString(attr.first),
              ToString(static_cast<ui::AXCheckedState>(attr.second)));
        }
        break;
      case ui::AX_ATTR_RESTRICTION:
        attributes->SetString(
            ToString(attr.first),
            ToString(static_cast<ui::AXRestriction>(attr.second)));
        break;
      case ui::AX_ATTR_SORT_DIRECTION:
        if (ui::AX_SORT_DIRECTION_NONE != attr.second) {
          attributes->SetString(
              ToString(attr.first),
              ToString(static_cast<ui::AXSortDirection>(attr.second)));
        }
        break;
      case ui::AX_ATTR_NAME_FROM:
        attributes->SetString(
            ToString(attr.first),
            ToString(static_cast<ui::AXNameFrom>(attr.second)));
        break;
      case ui::AX_ATTR_COLOR_VALUE:
      case ui::AX_ATTR_BACKGROUND_COLOR:
      case ui::AX_ATTR_COLOR:
        attributes->SetString(ToString(attr.first),
                              base::StringPrintf("0x%X", attr.second));
        break;
      case ui::AX_ATTR_DESCRIPTION_FROM:
        attributes->SetString(
            ToString(attr.first),
            ToString(static_cast<ui::AXDescriptionFrom>(attr.second)));
        break;
      case ui::AX_ATTR_ARIA_CURRENT_STATE:
        if (ui::AX_ARIA_CURRENT_STATE_NONE != attr.second) {
          attributes->SetString(
              ToString(attr.first),
              ToString(static_cast<ui::AXAriaCurrentState>(attr.second)));
        }
        break;
      case ui::AX_ATTR_TEXT_DIRECTION:
        if (ui::AX_TEXT_DIRECTION_NONE != attr.second) {
          attributes->SetString(
              ToString(attr.first),
              ToString(static_cast<ui::AXTextDirection>(attr.second)));
        }
        break;
      case ui::AX_ATTR_TEXT_STYLE: {
        auto text_style = static_cast<ui::AXTextStyle>(attr.second);
        if (text_style == ui::AX_TEXT_STYLE_NONE)
          break;

        static ui::AXTextStyle textStyleArr[] = {
            ui::AX_TEXT_STYLE_BOLD, ui::AX_TEXT_STYLE_ITALIC,
            ui::AX_TEXT_STYLE_UNDERLINE, ui::AX_TEXT_STYLE_LINE_THROUGH};

        CefRefPtr<CefListValue> list = CefListValue::Create();
        int index = 0;
        // Iterate and find which states are set.
        for (unsigned i = 0; i < arraysize(textStyleArr); i++) {
          if (text_style & textStyleArr[i])
            list->SetString(index++, ToString(textStyleArr[i]));
        }
        attributes->SetList(ToString(attr.first), list);
      } break;
    }
  }

  // Set Bool Attributes.
  void operator()(const std::pair<ui::AXBoolAttribute, bool> attr) {
    if (attr.first != ui::AX_BOOL_ATTRIBUTE_NONE)
      attributes->SetBool(ToString(attr.first), attr.second);
  }
  // Set String Attributes.
  void operator()(const std::pair<ui::AXStringAttribute, std::string>& attr) {
    if (attr.first != ui::AX_STRING_ATTRIBUTE_NONE)
      attributes->SetString(ToString(attr.first), attr.second);
  }
  // Set Float attributes.
  void operator()(const std::pair<ui::AXFloatAttribute, float>& attr) {
    if (attr.first != ui::AX_FLOAT_ATTRIBUTE_NONE)
      attributes->SetDouble(ToString(attr.first), attr.second);
  }

  // Set Int list attributes.
  void operator()(
      const std::pair<ui::AXIntListAttribute, std::vector<int32_t>>& attr) {
    if (attr.first != ui::AX_INT_LIST_ATTRIBUTE_NONE) {
      CefRefPtr<CefListValue> list;

      if (ui::AX_ATTR_MARKER_TYPES == attr.first) {
        list = CefListValue::Create();
        int index = 0;
        for (size_t i = 0; i < attr.second.size(); ++i) {
          auto type = static_cast<ui::AXMarkerType>(attr.second[i]);

          if (type == ui::AX_MARKER_TYPE_NONE)
            continue;

          static ui::AXMarkerType marktypeArr[] = {
              ui::AX_MARKER_TYPE_SPELLING, ui::AX_MARKER_TYPE_GRAMMAR,
              ui::AX_MARKER_TYPE_TEXT_MATCH};

          // Iterate and find which markers are set.
          for (unsigned j = 0; j < arraysize(marktypeArr); j++) {
            if (type & marktypeArr[j])
              list->SetString(index++, ToString(marktypeArr[j]));
          }
        }
      } else {
        list = ToCefValue(attr.second);
      }
      attributes->SetList(ToString(attr.first), list);
    }
  }
};

// Converts AXNodeData to CefDictionaryValue(like AXNodeData::ToString).
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXNodeData& node) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (node.id != -1)
    value->SetInt("id", node.id);

  value->SetString("role", ToString(node.role));
  value->SetList("state", ToCefValue(node.state));

  if (node.offset_container_id != -1)
    value->SetInt("offset_container_id", node.offset_container_id);

  value->SetDictionary("location", ToCefValue(node.location));

  // Transform matrix is private, so we set the string that Clients can parse
  // and use if needed.
  if (node.transform && !node.transform->IsIdentity())
    value->SetString("transform", node.transform->ToString());

  if (!node.child_ids.empty())
    value->SetList("child_ids", ToCefValue(node.child_ids));

  CefRefPtr<CefListValue> actions_strings;
  size_t actions_idx = 0;
  for (int action_index = ui::AX_ACTION_NONE + 1;
       action_index <= ui::AX_ACTION_LAST; ++action_index) {
    auto action = static_cast<ui::AXAction>(action_index);
    if (node.HasAction(action)) {
      if (!actions_strings)
        actions_strings = CefListValue::Create();
      actions_strings->SetString(actions_idx++, ToString(action));
    }
  }
  if (actions_strings)
    value->SetList("actions", actions_strings);

  CefRefPtr<CefDictionaryValue> attributes = CefDictionaryValue::Create();
  PopulateAxNodeAttributes func(attributes);

  // Poupulate Int Attributes.
  std::for_each(node.int_attributes.begin(), node.int_attributes.end(), func);

  // Poupulate String Attributes.
  std::for_each(node.string_attributes.begin(), node.string_attributes.end(),
                func);

  // Poupulate Float Attributes.
  std::for_each(node.float_attributes.begin(), node.float_attributes.end(),
                func);

  // Poupulate Bool Attributes.
  std::for_each(node.bool_attributes.begin(), node.bool_attributes.end(), func);

  // Populate int list attributes.
  std::for_each(node.intlist_attributes.begin(), node.intlist_attributes.end(),
                func);

  value->SetDictionary("attributes", attributes);

  return value;
}

// Converts AXTreeData to CefDictionaryValue(like AXTreeData::ToString).
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXTreeData& treeData) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (treeData.tree_id != -1)
    value->SetInt("tree_id", treeData.tree_id);

  if (treeData.parent_tree_id != -1)
    value->SetInt("parent_tree_id", treeData.parent_tree_id);

  if (treeData.focused_tree_id != -1)
    value->SetInt("focused_tree_id", treeData.focused_tree_id);

  if (!treeData.doctype.empty())
    value->SetString("doctype", treeData.doctype);

  value->SetBool("loaded", treeData.loaded);

  if (treeData.loading_progress != 0.0)
    value->SetDouble("loading_progress", treeData.loading_progress);

  if (!treeData.mimetype.empty())
    value->SetString("mimetype", treeData.mimetype);
  if (!treeData.url.empty())
    value->SetString("url", treeData.url);
  if (!treeData.title.empty())
    value->SetString("title", treeData.title);

  if (treeData.sel_anchor_object_id != -1) {
    value->SetInt("sel_anchor_object_id", treeData.sel_anchor_object_id);
    value->SetInt("sel_anchor_offset", treeData.sel_anchor_offset);
    value->SetString("sel_anchor_affinity",
                     ToString(treeData.sel_anchor_affinity));
  }
  if (treeData.sel_focus_object_id != -1) {
    value->SetInt("sel_focus_object_id", treeData.sel_anchor_object_id);
    value->SetInt("sel_focus_offset", treeData.sel_anchor_offset);
    value->SetString("sel_focus_affinity",
                     ToString(treeData.sel_anchor_affinity));
  }

  if (treeData.focus_id != -1)
    value->SetInt("focus_id", treeData.focus_id);

  return value;
}

// Converts AXTreeUpdate to CefDictionaryValue(like AXTreeUpdate::ToString).
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXTreeUpdate& update) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (update.has_tree_data) {
    value->SetBool("has_tree_data", true);
    value->SetDictionary("tree_data", ToCefValue(update.tree_data));
  }

  if (update.node_id_to_clear != 0)
    value->SetInt("node_id_to_clear", update.node_id_to_clear);

  if (update.root_id != 0)
    value->SetInt("root_id", update.root_id);

  value->SetList("nodes", ToCefValue(update.nodes));

  return value;
}

// Convert AXEventNotificationDetails to CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ToCefValue(
    const content::AXEventNotificationDetails& eventData) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (eventData.id != -1)
    value->SetInt("id", eventData.id);

  if (eventData.ax_tree_id != -1)
    value->SetInt("ax_tree_id", eventData.ax_tree_id);

  if (eventData.event_type != ui::AX_EVENT_NONE)
    value->SetString("event_type", ToString(eventData.event_type));

  if (eventData.event_from != ui::AX_EVENT_FROM_NONE)
    value->SetString("event_from", ToString(eventData.event_from));

  value->SetDictionary("update", ToCefValue(eventData.update));
  return value;
}

// Convert AXRelativeBounds to CefDictionaryValue. Similar to
// AXRelativeBounds::ToString. See that for more details
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXRelativeBounds& location) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (location.offset_container_id != -1)
    value->SetInt("offset_container_id", location.offset_container_id);

  value->SetDictionary("bounds", ToCefValue(location.bounds));

  // Transform matrix is private, so we set the string that Clients can parse
  // and use if needed.
  if (location.transform && !location.transform->IsIdentity())
    value->SetString("transform", location.transform->ToString());

  return value;
}

// Convert AXEventNotificationDetails to CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ToCefValue(
    const content::AXLocationChangeNotificationDetails& locData) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (locData.id != -1)
    value->SetInt("id", locData.id);

  if (locData.ax_tree_id != -1)
    value->SetInt("ax_tree_id", locData.ax_tree_id);

  value->SetDictionary("new_location", ToCefValue(locData.new_location));

  return value;
}

template <typename T>
CefRefPtr<CefListValue> ToCefValue(const std::vector<T>& vecData) {
  CefRefPtr<CefListValue> value = CefListValue::Create();

  for (size_t i = 0; i < vecData.size(); i++)
    value->SetDictionary(i, ToCefValue(vecData[i]));

  return value;
}

}  // namespace

namespace osr_accessibility_util {

CefRefPtr<CefValue> ParseAccessibilityEventData(
    const std::vector<content::AXEventNotificationDetails>& data) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetList(ToCefValue(data));
  return value;
}

CefRefPtr<CefValue> ParseAccessibilityLocationData(
    const std::vector<content::AXLocationChangeNotificationDetails>& data) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetList(ToCefValue(data));
  return value;
}

}  // namespace osr_accessibility_util
