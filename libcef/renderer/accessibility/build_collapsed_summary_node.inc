// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Included from inspector_type_builder_helper.cc via patch.
// Builds a minimal AXNode for off-screen landmarks and headings:
// role, name, and heading level (if applicable). No children.

#include "ui/accessibility/ax_role_properties.h"

namespace blink {

std::unique_ptr<AXNode> BuildCollapsedSummaryNode(AXObject& ax_object) {
  auto node = AXNode::create()
                  .setNodeId(String::Number(ax_object.AXObjectID()))
                  .setIgnored(false)
                  .build();

  ax::mojom::blink::Role role = ax_object.RoleValue();
  node->setRole(CreateRoleNameValue(role));
  node->setChromeRole(CreateInternalRoleValue(role));

  // Accessible name.
  ax::mojom::blink::NameFrom name_from;
  String computed_name = ax_object.GetName(name_from, nullptr, nullptr);
  node->setName(CreateValue(computed_name, AXValueTypeEnum::ComputedString));

  // Heading level.
  if (ui::IsHeading(static_cast<ax::mojom::Role>(role))) {
    auto properties = std::make_unique<protocol::Array<AXProperty>>();
    int level = ax_object.HeadingLevel();
    if (level > 0) {
      properties->emplace_back(
          CreateProperty(AXPropertyNameEnum::Level, CreateValue(level)));
    }
    node->setProperties(std::move(properties));
  }

  // Empty children.
  node->setChildIds(std::make_unique<protocol::Array<AXNodeId>>());

  // DOM node reference.
  Node* dom_node = ax_object.GetNode();
  if (dom_node) {
    node->setBackendDOMNodeId(IdentifiersFactory::IntIdForNode(dom_node));
  }

  // Parent reference.
  const AXObject* parent = ax_object.ParentObjectIncludedInTree();
  if (parent) {
    node->setParentId(String::Number(parent->AXObjectID()));
  } else if (ax_object.GetDocument() && ax_object.GetDocument()->GetFrame()) {
    auto& frame_token =
        ax_object.GetDocument()->GetFrame()->GetDevToolsFrameToken();
    node->setFrameId(IdentifiersFactory::IdFromToken(frame_token));
  }

  return node;
}

}  // namespace blink
