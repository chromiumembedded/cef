// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SNAPSHOT_FORMATTER_H_
#define CEF_LIBCEF_BROWSER_SNAPSHOT_FORMATTER_H_
#pragma once

#include <string>
#include <vector>

#include "cef/libcef/browser/element_ref.h"

// Output format options for snapshot data.
enum class SnapshotFormat {
  kAccessibilityTree = 0,  // Default hierarchical tree text
  kTSV = 1,                // Tab-separated values (40% fewer tokens)
  kMarkdown = 2,           // Markdown table
};

// Formats a collection of element refs into the specified output format.
// Used by both Snapshot() and annotated screenshot ref output.
class CefSnapshotFormatter {
 public:
  // Format element refs as TSV.
  // Columns: ref\trole\tname\tvalue\tbounds\tstate
  // Header row included.
  // Example:
  //   ref	role	name	value	bounds	state
  //   @e1	button	Submit		120,340,80,32
  //   @e2	textbox	Email	user@test.com	120,380,200,28	focused
  //   @e3	link	Home		10,10,60,20
  static std::string FormatAsTSV(const std::vector<CefElementRef>& refs);

  // Format element refs as markdown table.
  // | Ref | Role | Name | Value | Bounds | State |
  // |-----|------|------|-------|--------|-------|
  // | @e1 | button | Submit | | 120,340,80,32 | |
  static std::string FormatAsMarkdown(const std::vector<CefElementRef>& refs);

  // Format element refs as accessibility tree text (existing format).
  // - heading "Title" [ref=e1] [level=1]
  //   - button "Submit" [ref=e2]
  //   - textbox "Email" [ref=e3] [value="user@test.com"]
  static std::string FormatAsTree(const std::vector<CefElementRef>& refs);

  // Auto-select format based on settings enum value.
  static std::string Format(const std::vector<CefElementRef>& refs,
                            int format);

 private:
  // Escape a string for TSV (replace tabs and newlines).
  static std::string EscapeTSV(const std::string& str);

  // Escape a string for markdown (replace pipes).
  static std::string EscapeMarkdown(const std::string& str);

  // Truncate a string to max_len, adding "..." if truncated.
  static std::string Truncate(const std::string& str, size_t max_len = 50);

  // Format bounds as "x,y,w,h" string.
  static std::string FormatBounds(const gfx::Rect& bounds);

  // Format element state flags as string.
  static std::string FormatState(const CefElementRef& ref);
};

#endif  // CEF_LIBCEF_BROWSER_SNAPSHOT_FORMATTER_H_
