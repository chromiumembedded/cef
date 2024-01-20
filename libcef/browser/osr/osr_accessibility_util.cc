// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/osr/osr_accessibility_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/json/string_escape.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/transform.h"

namespace {
using ui::ToString;

template <typename T>
CefRefPtr<CefListValue> ToCefValue(const std::vector<T>& vecData);

template <>
CefRefPtr<CefListValue> ToCefValue<int>(const std::vector<int>& vecData) {
  CefRefPtr<CefListValue> value = CefListValue::Create();
  for (size_t i = 0; i < vecData.size(); i++) {
    value->SetInt(i, vecData[i]);
  }

  return value;
}

// Helper function for AXNodeData::ToCefValue - Converts AXState attributes to
// CefListValue.
CefRefPtr<CefListValue> ToCefValue(uint32_t state) {
  CefRefPtr<CefListValue> value = CefListValue::Create();

  int index = 0;
  // Iterate and find which states are set.
  for (unsigned i = static_cast<unsigned>(ax::mojom::Role::kMinValue) + 1;
       i <= static_cast<unsigned>(ax::mojom::Role::kMaxValue); i++) {
    if (state & (1 << i)) {
      value->SetString(index++, ToString(static_cast<ax::mojom::State>(i)));
    }
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
  void operator()(const std::pair<ax::mojom::IntAttribute, int32_t> attr) {
    if (attr.first == ax::mojom::IntAttribute::kNone) {
      return;
    }

    switch (attr.first) {
      case ax::mojom::IntAttribute::kNone:
        break;
      case ax::mojom::IntAttribute::kScrollX:
      case ax::mojom::IntAttribute::kScrollXMin:
      case ax::mojom::IntAttribute::kScrollXMax:
      case ax::mojom::IntAttribute::kScrollY:
      case ax::mojom::IntAttribute::kScrollYMin:
      case ax::mojom::IntAttribute::kScrollYMax:
      case ax::mojom::IntAttribute::kHasPopup:
      case ax::mojom::IntAttribute::kIsPopup:
      case ax::mojom::IntAttribute::kHierarchicalLevel:
      case ax::mojom::IntAttribute::kTextSelStart:
      case ax::mojom::IntAttribute::kTextSelEnd:
      case ax::mojom::IntAttribute::kAriaColumnCount:
      case ax::mojom::IntAttribute::kAriaCellColumnIndex:
      case ax::mojom::IntAttribute::kAriaRowCount:
      case ax::mojom::IntAttribute::kAriaCellRowIndex:
      case ax::mojom::IntAttribute::kTableRowCount:
      case ax::mojom::IntAttribute::kTableColumnCount:
      case ax::mojom::IntAttribute::kTableCellColumnIndex:
      case ax::mojom::IntAttribute::kTableCellRowIndex:
      case ax::mojom::IntAttribute::kTableCellColumnSpan:
      case ax::mojom::IntAttribute::kTableCellRowSpan:
      case ax::mojom::IntAttribute::kTableColumnHeaderId:
      case ax::mojom::IntAttribute::kTableColumnIndex:
      case ax::mojom::IntAttribute::kTableHeaderId:
      case ax::mojom::IntAttribute::kTableRowHeaderId:
      case ax::mojom::IntAttribute::kTableRowIndex:
      case ax::mojom::IntAttribute::kActivedescendantId:
      case ax::mojom::IntAttribute::kInPageLinkTargetId:
      case ax::mojom::IntAttribute::kErrormessageIdDeprecated:
      case ax::mojom::IntAttribute::kDOMNodeId:
      case ax::mojom::IntAttribute::kDropeffectDeprecated:
      case ax::mojom::IntAttribute::kMemberOfId:
      case ax::mojom::IntAttribute::kNextFocusId:
      case ax::mojom::IntAttribute::kNextWindowFocusId:
      case ax::mojom::IntAttribute::kNextOnLineId:
      case ax::mojom::IntAttribute::kPreviousFocusId:
      case ax::mojom::IntAttribute::kPreviousWindowFocusId:
      case ax::mojom::IntAttribute::kPreviousOnLineId:
      case ax::mojom::IntAttribute::kSetSize:
      case ax::mojom::IntAttribute::kPosInSet:
      case ax::mojom::IntAttribute::kPopupForId:
        attributes->SetInt(ToString(attr.first), attr.second);
        break;
      case ax::mojom::IntAttribute::kDefaultActionVerb:
        attributes->SetString(
            ToString(attr.first),
            ui::ToString(
                static_cast<ax::mojom::DefaultActionVerb>(attr.second)));
        break;
      case ax::mojom::IntAttribute::kInvalidState: {
        auto state = static_cast<ax::mojom::InvalidState>(attr.second);
        if (ax::mojom::InvalidState::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kCheckedState: {
        auto state = static_cast<ax::mojom::CheckedState>(attr.second);
        if (ax::mojom::CheckedState::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kRestriction:
        attributes->SetString(
            ToString(attr.first),
            ToString(static_cast<ax::mojom::Restriction>(attr.second)));
        break;
      case ax::mojom::IntAttribute::kListStyle: {
        auto state = static_cast<ax::mojom::ListStyle>(attr.second);
        if (ax::mojom::ListStyle::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kSortDirection: {
        auto state = static_cast<ax::mojom::SortDirection>(attr.second);
        if (ax::mojom::SortDirection::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kTextAlign: {
        auto state = static_cast<ax::mojom::TextAlign>(attr.second);
        if (ax::mojom::TextAlign::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kNameFrom:
        attributes->SetString(
            ToString(attr.first),
            ToString(static_cast<ax::mojom::NameFrom>(attr.second)));
        break;
      case ax::mojom::IntAttribute::kColorValue:
      case ax::mojom::IntAttribute::kBackgroundColor:
      case ax::mojom::IntAttribute::kColor:
        attributes->SetString(ToString(attr.first),
                              base::StringPrintf("0x%X", attr.second));
        break;
      case ax::mojom::IntAttribute::kDescriptionFrom:
        attributes->SetString(
            ToString(attr.first),
            ToString(static_cast<ax::mojom::DescriptionFrom>(attr.second)));
        break;
      case ax::mojom::IntAttribute::kAriaCurrentState: {
        auto state = static_cast<ax::mojom::AriaCurrentState>(attr.second);
        if (ax::mojom::AriaCurrentState::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kTextDirection: {
        auto state = static_cast<ax::mojom::WritingDirection>(attr.second);
        if (ax::mojom::WritingDirection::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kTextPosition: {
        auto state = static_cast<ax::mojom::TextPosition>(attr.second);
        if (ax::mojom::TextPosition::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kTextStyle: {
        static ax::mojom::TextStyle textStyleArr[] = {
            ax::mojom::TextStyle::kBold, ax::mojom::TextStyle::kItalic,
            ax::mojom::TextStyle::kUnderline,
            ax::mojom::TextStyle::kLineThrough,
            ax::mojom::TextStyle::kOverline};

        CefRefPtr<CefListValue> list = CefListValue::Create();
        int index = 0;
        // Iterate and find which states are set.
        for (auto& i : textStyleArr) {
          if (attr.second & static_cast<int>(i)) {
            list->SetString(index++, ToString(i));
          }
        }
        attributes->SetList(ToString(attr.first), list);
      } break;
      case ax::mojom::IntAttribute::kTextOverlineStyle:
      case ax::mojom::IntAttribute::kTextStrikethroughStyle:
      case ax::mojom::IntAttribute::kTextUnderlineStyle: {
        auto state = static_cast<ax::mojom::TextDecorationStyle>(attr.second);
        if (ax::mojom::TextDecorationStyle::kNone != state) {
          attributes->SetString(ToString(attr.first), ToString(state));
        }
      } break;
      case ax::mojom::IntAttribute::kAriaCellColumnSpan:
      case ax::mojom::IntAttribute::kAriaCellRowSpan:
      case ax::mojom::IntAttribute::kImageAnnotationStatus: {
        // TODO(cef): Implement support for Image Annotation Status,
        // kAriaCellColumnSpan and kAriaCellRowSpan
      } break;
    }
  }

  // Set Bool Attributes.
  void operator()(const std::pair<ax::mojom::BoolAttribute, bool> attr) {
    if (attr.first != ax::mojom::BoolAttribute::kNone) {
      attributes->SetBool(ToString(attr.first), attr.second);
    }
  }
  // Set String Attributes.
  void operator()(
      const std::pair<ax::mojom::StringAttribute, std::string>& attr) {
    if (attr.first != ax::mojom::StringAttribute::kNone) {
      attributes->SetString(ToString(attr.first), attr.second);
    }
  }
  // Set Float attributes.
  void operator()(const std::pair<ax::mojom::FloatAttribute, float>& attr) {
    if (attr.first != ax::mojom::FloatAttribute::kNone) {
      attributes->SetDouble(ToString(attr.first), attr.second);
    }
  }

  // Set Int list attributes.
  void operator()(const std::pair<ax::mojom::IntListAttribute,
                                  std::vector<int32_t>>& attr) {
    if (attr.first != ax::mojom::IntListAttribute::kNone) {
      CefRefPtr<CefListValue> list;

      if (ax::mojom::IntListAttribute::kMarkerTypes == attr.first) {
        list = CefListValue::Create();
        int index = 0;
        for (int i : attr.second) {
          auto type = static_cast<ax::mojom::MarkerType>(i);

          if (type == ax::mojom::MarkerType::kNone) {
            continue;
          }

          static ax::mojom::MarkerType marktypeArr[] = {
              ax::mojom::MarkerType::kSpelling, ax::mojom::MarkerType::kGrammar,
              ax::mojom::MarkerType::kTextMatch};

          // Iterate and find which markers are set.
          for (auto& j : marktypeArr) {
            if (i & static_cast<int>(j)) {
              list->SetString(index++, ToString(j));
            }
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

  if (node.id != -1) {
    value->SetInt("id", node.id);
  }

  value->SetString("role", ToString(node.role));
  value->SetList("state", ToCefValue(node.state));

  if (node.relative_bounds.offset_container_id != -1) {
    value->SetInt("offset_container_id",
                  node.relative_bounds.offset_container_id);
  }

  value->SetDictionary("location", ToCefValue(node.relative_bounds.bounds));

  // Transform matrix is private, so we set the string that Clients can parse
  // and use if needed.
  if (node.relative_bounds.transform &&
      !node.relative_bounds.transform->IsIdentity()) {
    value->SetString("transform", node.relative_bounds.transform->ToString());
  }

  if (!node.child_ids.empty()) {
    value->SetList("child_ids", ToCefValue(node.child_ids));
  }

  CefRefPtr<CefListValue> actions_strings;
  size_t actions_idx = 0;
  for (int action_index = static_cast<int>(ax::mojom::Action::kMinValue) + 1;
       action_index <= static_cast<int>(ax::mojom::Action::kMaxValue);
       ++action_index) {
    auto action = static_cast<ax::mojom::Action>(action_index);
    if (node.HasAction(action)) {
      if (!actions_strings) {
        actions_strings = CefListValue::Create();
      }
      actions_strings->SetString(actions_idx++, ToString(action));
    }
  }
  if (actions_strings) {
    value->SetList("actions", actions_strings);
  }

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

  if (!treeData.tree_id.ToString().empty()) {
    value->SetString("tree_id", treeData.tree_id.ToString());
  }

  if (!treeData.parent_tree_id.ToString().empty()) {
    value->SetString("parent_tree_id", treeData.parent_tree_id.ToString());
  }

  if (!treeData.focused_tree_id.ToString().empty()) {
    value->SetString("focused_tree_id", treeData.focused_tree_id.ToString());
  }

  if (!treeData.doctype.empty()) {
    value->SetString("doctype", treeData.doctype);
  }

  value->SetBool("loaded", treeData.loaded);

  if (treeData.loading_progress != 0.0) {
    value->SetDouble("loading_progress", treeData.loading_progress);
  }

  if (!treeData.mimetype.empty()) {
    value->SetString("mimetype", treeData.mimetype);
  }
  if (!treeData.url.empty()) {
    value->SetString("url", treeData.url);
  }
  if (!treeData.title.empty()) {
    value->SetString("title", treeData.title);
  }

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

  if (treeData.focus_id != -1) {
    value->SetInt("focus_id", treeData.focus_id);
  }

  return value;
}

// Converts AXTreeUpdate to CefDictionaryValue(like AXTreeUpdate::ToString).
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXTreeUpdate& update) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (update.has_tree_data) {
    value->SetBool("has_tree_data", true);
    value->SetDictionary("tree_data", ToCefValue(update.tree_data));
  }

  if (update.node_id_to_clear != 0) {
    value->SetInt("node_id_to_clear", update.node_id_to_clear);
  }

  if (update.root_id != 0) {
    value->SetInt("root_id", update.root_id);
  }

  value->SetList("nodes", ToCefValue(update.nodes));

  return value;
}

// Converts AXEvent to CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXEvent& event) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (event.event_type != ax::mojom::Event::kNone) {
    value->SetString("event_type", ToString(event.event_type));
  }

  if (event.id != -1) {
    value->SetInt("id", event.id);
  }

  if (event.event_from != ax::mojom::EventFrom::kNone) {
    value->SetString("event_from", ToString(event.event_from));
  }

  if (event.action_request_id != -1) {
    value->SetInt("action_request_id", event.action_request_id);
  }

  return value;
}

// Convert AXEventNotificationDetails to CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ToCefValue(
    const content::AXEventNotificationDetails& eventData) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (!eventData.ax_tree_id.ToString().empty()) {
    value->SetString("ax_tree_id", eventData.ax_tree_id.ToString());
  }

  if (eventData.updates.size() > 0) {
    CefRefPtr<CefListValue> updates = CefListValue::Create();
    updates->SetSize(eventData.updates.size());
    size_t i = 0;
    for (const auto& update : eventData.updates) {
      updates->SetDictionary(i++, ToCefValue(update));
    }
    value->SetList("updates", updates);
  }

  if (eventData.events.size() > 0) {
    CefRefPtr<CefListValue> events = CefListValue::Create();
    events->SetSize(eventData.events.size());
    size_t i = 0;
    for (const auto& event : eventData.events) {
      events->SetDictionary(i++, ToCefValue(event));
    }
    value->SetList("events", events);
  }

  return value;
}

// Convert AXRelativeBounds to CefDictionaryValue. Similar to
// AXRelativeBounds::ToString. See that for more details
CefRefPtr<CefDictionaryValue> ToCefValue(const ui::AXRelativeBounds& location) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (location.offset_container_id != -1) {
    value->SetInt("offset_container_id", location.offset_container_id);
  }

  value->SetDictionary("bounds", ToCefValue(location.bounds));

  // Transform matrix is private, so we set the string that Clients can parse
  // and use if needed.
  if (location.transform && !location.transform->IsIdentity()) {
    value->SetString("transform", location.transform->ToString());
  }

  return value;
}

// Convert AXLocationChangeNotificationDetails to CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ToCefValue(
    const content::AXLocationChangeNotificationDetails& locData) {
  CefRefPtr<CefDictionaryValue> value = CefDictionaryValue::Create();

  if (locData.id != -1) {
    value->SetInt("id", locData.id);
  }

  if (!locData.ax_tree_id.ToString().empty()) {
    value->SetString("ax_tree_id", locData.ax_tree_id.ToString());
  }

  value->SetDictionary("new_location", ToCefValue(locData.new_location));

  return value;
}

template <typename T>
CefRefPtr<CefListValue> ToCefValue(const std::vector<T>& vecData) {
  CefRefPtr<CefListValue> value = CefListValue::Create();

  for (size_t i = 0; i < vecData.size(); i++) {
    value->SetDictionary(i, ToCefValue(vecData[i]));
  }

  return value;
}

}  // namespace

namespace osr_accessibility_util {

CefRefPtr<CefValue> ParseAccessibilityEventData(
    const content::AXEventNotificationDetails& data) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetDictionary(ToCefValue(data));
  return value;
}

CefRefPtr<CefValue> ParseAccessibilityLocationData(
    const std::vector<content::AXLocationChangeNotificationDetails>& data) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetList(ToCefValue(data));
  return value;
}

}  // namespace osr_accessibility_util
