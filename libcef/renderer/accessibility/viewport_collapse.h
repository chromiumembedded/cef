// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_ACCESSIBILITY_VIEWPORT_COLLAPSE_H_
#define CEF_LIBCEF_RENDERER_ACCESSIBILITY_VIEWPORT_COLLAPSE_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {
class AXObject;
class Document;
}  // namespace blink

namespace cef {

enum class ViewportCollapseAction { kSummary, kPrune };

// Check if viewport collapse is enabled for this document (reads Blink
// Settings).
bool IsViewportCollapseEnabled(blink::Document& document);

// Compute viewport rect from the document's visual viewport.
blink::PhysicalRect ComputeViewportRect(blink::Document& document);

// Check if a node intersects the viewport.
bool IsNodeInViewport(const blink::AXObject& ax_object,
                      const blink::PhysicalRect& viewport_rect);

// Classify an off-screen node by role.
ViewportCollapseAction ClassifyOffScreenNode(ax::mojom::blink::Role role);

}  // namespace cef

#endif  // CEF_LIBCEF_RENDERER_ACCESSIBILITY_VIEWPORT_COLLAPSE_H_
