// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/snapshot_formatter.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

// static
std::string CefSnapshotFormatter::FormatAsTSV(
    const std::vector<CefElementRef>& refs) {
  std::ostringstream out;
  out << "ref\trole\tname\tvalue\tbounds\tstate\n";

  for (const auto& ref : refs) {
    // Skip elements with empty bounds.
    if (ref.bounds.IsEmpty()) {
      continue;
    }

    out << "@e" << ref.id << '\t' << EscapeTSV(ref.role) << '\t'
        << EscapeTSV(Truncate(ref.name)) << '\t'
        << EscapeTSV(Truncate(ref.value)) << '\t' << FormatBounds(ref.bounds)
        << '\t' << FormatState(ref) << '\n';
  }

  return out.str();
}

// static
std::string CefSnapshotFormatter::FormatAsMarkdown(
    const std::vector<CefElementRef>& refs) {
  std::ostringstream out;
  out << "| Ref | Role | Name | Value | Bounds | State |\n";
  out << "|-----|------|------|-------|--------|-------|\n";

  for (const auto& ref : refs) {
    if (ref.bounds.IsEmpty()) {
      continue;
    }

    out << "| @e" << ref.id << " | " << EscapeMarkdown(ref.role) << " | "
        << EscapeMarkdown(Truncate(ref.name)) << " | "
        << EscapeMarkdown(Truncate(ref.value)) << " | "
        << FormatBounds(ref.bounds) << " | " << EscapeMarkdown(FormatState(ref))
        << " |\n";
  }

  return out.str();
}

// static
std::string CefSnapshotFormatter::FormatAsTree(
    const std::vector<CefElementRef>& refs) {
  std::ostringstream out;

  for (const auto& ref : refs) {
    out << "- " << ref.role;

    if (!ref.name.empty()) {
      out << " \"" << Truncate(ref.name) << "\"";
    }

    out << " [ref=e" << ref.id << "]";

    if (!ref.value.empty()) {
      out << " [value=\"" << Truncate(ref.value) << "\"]";
    }

    if (!ref.input_type.empty()) {
      out << " [type=" << ref.input_type << "]";
    }

    if (ref.disabled) {
      out << " [disabled]";
    }

    if (ref.focused) {
      out << " [focused]";
    }

    out << '\n';
  }

  return out.str();
}

// static
std::string CefSnapshotFormatter::Format(
    const std::vector<CefElementRef>& refs,
    int format) {
  switch (static_cast<SnapshotFormat>(format)) {
    case SnapshotFormat::kTSV:
      return FormatAsTSV(refs);
    case SnapshotFormat::kMarkdown:
      return FormatAsMarkdown(refs);
    case SnapshotFormat::kAccessibilityTree:
    default:
      return FormatAsTree(refs);
  }
}

// static
std::string CefSnapshotFormatter::EscapeTSV(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  for (char c : str) {
    switch (c) {
      case '\t':
        result += ' ';
        break;
      case '\n':
        result += ' ';
        break;
      case '\r':
        // Strip carriage returns entirely.
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
}

// static
std::string CefSnapshotFormatter::EscapeMarkdown(const std::string& str) {
  std::string result;
  result.reserve(str.size() + str.size() / 8);
  for (char c : str) {
    if (c == '|') {
      result += "\\|";
    } else {
      result += c;
    }
  }
  return result;
}

// static
std::string CefSnapshotFormatter::Truncate(const std::string& str,
                                           size_t max_len) {
  if (str.size() <= max_len) {
    return str;
  }
  // Ensure max_len is large enough for the ellipsis.
  if (max_len <= 3) {
    return str.substr(0, max_len);
  }
  return str.substr(0, max_len - 3) + "...";
}

// static
std::string CefSnapshotFormatter::FormatBounds(const gfx::Rect& bounds) {
  return base::StringPrintf("%d,%d,%d,%d", bounds.x(), bounds.y(),
                            bounds.width(), bounds.height());
}

// static
std::string CefSnapshotFormatter::FormatState(const CefElementRef& ref) {
  std::vector<std::string> states;
  if (ref.focused) {
    states.push_back("focused");
  }
  if (ref.disabled) {
    states.push_back("disabled");
  }
  return base::JoinString(states, ",");
}
