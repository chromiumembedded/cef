// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/renderer/accessibility/viewport_collapse.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "ui/accessibility/ax_role_properties.h"

namespace cef {

bool IsViewportCollapseEnabled(blink::Document& document) {
  auto* settings = document.GetSettings();
  return settings && settings->GetAccessibilityViewportCollapse();
}

blink::PhysicalRect ComputeViewportRect(blink::Document& document) {
  blink::LocalFrameView* frame_view = document.View();

  // The visual viewport's VisibleContentRect accounts for pinch-zoom scroll
  // but not layout viewport scroll. For a normal (non-zoomed) page, the
  // visual viewport offset is (0,0) and all scrolling is on the layout
  // viewport. We need the full visible area in document coordinates, so
  // combine both: start from the visual viewport rect (which has the correct
  // size and visual viewport offset) and add the layout viewport scroll.
  auto rect = blink::PhysicalRect(
      frame_view->GetPage()->GetVisualViewport().VisibleContentRect());
  if (auto* layout_viewport = frame_view->LayoutViewport()) {
    blink::ScrollOffset scroll = layout_viewport->GetScrollOffset();
    rect.offset.left += blink::LayoutUnit(scroll.x());
    rect.offset.top += blink::LayoutUnit(scroll.y());
  }
  return rect;
}

bool IsNodeInViewport(const blink::AXObject& ax_object,
                      const blink::PhysicalRect& viewport_rect) {
  blink::PhysicalRect bounds = ax_object.GetBoundsInFrameCoordinates();
  return bounds.Intersects(viewport_rect);
}

ViewportCollapseAction ClassifyOffScreenNode(ax::mojom::blink::Role role) {
  ax::mojom::Role r = static_cast<ax::mojom::Role>(role);
  if (ui::IsLandmark(r) || ui::IsHeading(r)) {
    return ViewportCollapseAction::kSummary;
  }
  // Chromium maps <footer> to kFooter and <header> to kHeader, which are
  // distinct from kContentInfo/kBanner used by IsLandmark(). Treat them
  // as landmarks for collapsing purposes.
  if (r == ax::mojom::Role::kFooter || r == ax::mojom::Role::kHeader) {
    return ViewportCollapseAction::kSummary;
  }
  return ViewportCollapseAction::kPrune;
}

}  // namespace cef
