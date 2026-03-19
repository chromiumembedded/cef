// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Included from inspector_accessibility_agent.cc via patch (anonymous
// namespace). Viewport-collapse-aware variants of AddChildren and
// WalkAXNodesToDepth.

// Check if a node might be in-viewport despite having an off-screen parent.
// Only position:fixed and position:sticky elements can escape their parent's
// off-screen bounds. This is much cheaper than GetBoundsInFrameCoordinates().
bool MayBeVisibleDespiteOffscreenParent(const AXObject& ax_object) {
  auto* layout = ax_object.GetLayoutObject();
  return layout &&
         (layout->IsFixedPositioned() || layout->IsStickyPositioned());
}

struct BfsEntry {
  AXID id;
  int depth;
  bool parent_offscreen;
};

void AddChildrenWithCollapse(AXObject& ax_object,
                             bool follow_ignored,
                             std::unique_ptr<protocol::Array<AXNode>>& nodes,
                             AXObjectCacheImpl& cache,
                             const PhysicalRect& viewport_rect,
                             HashSet<AXID>& serialized_ids,
                             HashSet<AXID>& in_viewport_ids,
                             bool parent_offscreen,
                             Deque<BfsEntry>* queue,
                             int depth,
                             int max_depth) {
  // Track IDs of nodes reached via pruned off-screen parents. If such a
  // node turns out to be in-viewport (e.g. position:fixed), it must be
  // enqueued in the BFS since the normal child-enqueue loop won't find it.
  HashSet<AXID> from_pruned_ids;

  HeapVector<Member<AXObject>> reachable;
  reachable.AppendRange(ax_object.ChildrenIncludingIgnored().rbegin(),
                        ax_object.ChildrenIncludingIgnored().rend());

  while (!reachable.empty()) {
    AXObject* descendant = reachable.back();
    reachable.pop_back();
    if (descendant->IsDetached()) {
      continue;
    }

    bool from_pruned = from_pruned_ids.Contains(descendant->AXObjectID());

    if (follow_ignored &&
        (descendant->IsIgnoredButIncludedInTree() || !descendant->GetNode())) {
      const auto& children = descendant->ChildrenIncludingIgnored();
      reachable.AppendRange(children.rbegin(), children.rend());
      if (from_pruned) {
        for (const auto& child : children) {
          from_pruned_ids.insert(child->AXObjectID());
        }
      }
    }

    // Determine viewport status. If parent is off-screen, skip the expensive
    // bounds check unless this node has fixed/sticky positioning.
    bool effectively_offscreen = parent_offscreen || from_pruned;
    bool in_viewport;
    if (effectively_offscreen &&
        !MayBeVisibleDespiteOffscreenParent(*descendant)) {
      in_viewport = false;
    } else {
      in_viewport = cef::IsNodeInViewport(*descendant, viewport_rect);
    }

    if (!in_viewport) {
      auto action = cef::ClassifyOffScreenNode(descendant->RoleValue());
      if (action == cef::ViewportCollapseAction::kSummary) {
        // Landmark/heading: emit summary, cut subtree entirely.
        nodes->emplace_back(BuildCollapsedSummaryNode(*descendant));
        serialized_ids.insert(descendant->AXObjectID());
      } else {
        // Pruned generic node: still visit children so that position:fixed
        // or position:sticky descendants can escape the off-screen parent.
        const auto& children = descendant->ChildrenIncludingIgnored();
        reachable.AppendRange(children.rbegin(), children.rend());
        for (const auto& child : children) {
          from_pruned_ids.insert(child->AXObjectID());
        }
      }
      continue;
    }

    in_viewport_ids.insert(descendant->AXObjectID());
    auto child_node = BuildProtocolAXNodeForAXObject(*descendant);
    serialized_ids.insert(descendant->AXObjectID());
    nodes->emplace_back(std::move(child_node));

    // If this in-viewport node was discovered via a pruned off-screen parent,
    // it won't be enqueued by the normal BFS child loop (which only checks
    // direct unignored children). Enqueue it here so its subtree is visited.
    if (from_pruned && queue && (max_depth == -1 || depth < max_depth)) {
      queue->emplace_back(BfsEntry{descendant->AXObjectID(), depth + 1, false});
    }
  }
}

std::unique_ptr<protocol::Array<AXNode>> WalkAXNodesToDepthWithCollapse(
    AXObjectCacheImpl& cache,
    int max_depth) {
  auto nodes = std::make_unique<protocol::Array<AXNode>>();
  HashSet<AXID> serialized_ids;
  HashSet<AXID> in_viewport_ids;  // Tracks which nodes passed bounds check
  PhysicalRect viewport_rect = cef::ComputeViewportRect(cache.GetDocument());

  Deque<BfsEntry> queue;
  queue.emplace_back(BfsEntry{cache.Root()->AXObjectID(), 1, false});
  nodes->emplace_back(BuildProtocolAXNodeForAXObject(*cache.Root()));
  serialized_ids.insert(cache.Root()->AXObjectID());
  in_viewport_ids.insert(cache.Root()->AXObjectID());

  while (!queue.empty()) {
    auto [id, depth, parent_offscreen] = queue.front();
    queue.pop_front();
    AXObject* ax_object = cache.ObjectFromAXID(id);
    if (!ax_object) {
      continue;
    }

    // Serialize children — records viewport status in in_viewport_ids.
    // Passes queue so that in-viewport nodes found under pruned parents
    // can be enqueued for further traversal.
    AddChildrenWithCollapse(*ax_object, true, nodes, cache, viewport_rect,
                            serialized_ids, in_viewport_ids, parent_offscreen,
                            &queue, depth, max_depth);

    // Enqueue in-viewport unignored children for depth traversal.
    // Uses in_viewport_ids instead of recomputing bounds.
    const auto& children = ax_object->UnignoredChildrenSlow();
    for (auto& child : children) {
      bool child_in_viewport = in_viewport_ids.Contains(child->AXObjectID());
      if (!child_in_viewport) {
        // Already handled (serialized as summary or pruned) in
        // AddChildrenWithCollapse. Don't recurse.
        continue;
      }
      if (max_depth == -1 || depth < max_depth) {
        queue.emplace_back(BfsEntry{child->AXObjectID(), depth + 1, false});
      }
    }
  }

  // Filter childIds to only reference serialized nodes.
  // Summary nodes already have empty childIds so this only matters for
  // full nodes whose children were partially pruned.
  for (auto& node : *nodes) {
    auto* child_ids = node->getChildIds(nullptr);
    if (!child_ids || child_ids->empty()) {
      continue;
    }
    auto filtered = std::make_unique<protocol::Array<AXNodeId>>();
    for (const auto& child_id : *child_ids) {
      if (serialized_ids.Contains(child_id.ToInt())) {
        filtered->emplace_back(child_id);
      }
    }
    node->setChildIds(std::move(filtered));
  }

  return nodes;
}
