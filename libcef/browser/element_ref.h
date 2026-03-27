// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ELEMENT_REF_H_
#define CEF_LIBCEF_BROWSER_ELEMENT_REF_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "ui/gfx/geometry/rect.h"

// Represents a single element reference from an annotated snapshot/screenshot.
// Each ref corresponds to an interactive element on the page.
struct CefElementRef {
  // The ref ID (1-based). Used as @e1, @e2, etc.
  int id = 0;

  // Accessibility role (e.g., "button", "link", "textbox").
  std::string role;

  // Accessible name (e.g., "Submit", "Email", "Home").
  std::string name;

  // Bounding rectangle in page coordinates (CSS pixels).
  gfx::Rect bounds;

  // The accessibility node ID from the AX tree. Used for targeting actions
  // like click, fill, hover via the accessibility API.
  int ax_node_id = -1;

  // The backend DOM node ID, if available. Used for CDP DOM operations.
  int backend_dom_node_id = -1;

  // Whether the element is currently visible (not clipped, not hidden).
  bool visible = true;

  // Element tag name (e.g., "BUTTON", "A", "INPUT").
  std::string tag_name;

  // For input elements, the input type (e.g., "text", "password", "email").
  std::string input_type;

  // Element value or current text content, truncated to reasonable length.
  std::string value;

  // Whether the element is focused.
  bool focused = false;

  // Whether the element is disabled.
  bool disabled = false;
};

// Manages the current set of element references for a browser instance.
// Refs are created during annotated screenshots and persist until the next
// screenshot or explicit invalidation (e.g., navigation).
//
// Thread-safe via internal locking.
class CefElementRefIndex {
 public:
  CefElementRefIndex();
  ~CefElementRefIndex();

  CefElementRefIndex(const CefElementRefIndex&) = delete;
  CefElementRefIndex& operator=(const CefElementRefIndex&) = delete;

  // Replace all refs with a new set. Called after annotated screenshot capture.
  void SetRefs(std::vector<CefElementRef> refs);

  // Get a ref by ID (1-based). Returns nullptr if not found.
  const CefElementRef* GetRef(int id) const;

  // Get a ref by ID string (e.g., "@e1" or "e1" or "1"). Returns nullptr if
  // not found.
  const CefElementRef* GetRefByString(const std::string& ref_str) const;

  // Get all current refs.
  std::vector<CefElementRef> GetAllRefs() const;

  // Get the number of refs.
  size_t GetRefCount() const;

  // Clear all refs (e.g., on navigation).
  void Clear();

  // Build the text representation of the ref index for output to agents.
  // Format: "[1] @e1 button \"Submit\"\n[2] @e2 link \"Home\"\n..."
  std::string BuildRefText() const;

  // Check if refs are available (non-empty and not stale).
  bool HasRefs() const;

  // Get the generation counter. Incremented on each SetRefs() or Clear().
  uint64_t GetGeneration() const;

 private:
  // Parse a ref string like "@e1", "e1", or "1" to an integer ID.
  static int ParseRefId(const std::string& ref_str);

  mutable base::Lock lock_;
  std::vector<CefElementRef> refs_;
  uint64_t generation_ = 0;
};

#endif  // CEF_LIBCEF_BROWSER_ELEMENT_REF_H_
