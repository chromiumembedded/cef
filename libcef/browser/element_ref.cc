// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/element_ref.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace {

// Maximum length for element names in the text representation.
constexpr size_t kMaxNameLength = 50;

std::string TruncateName(const std::string& name) {
  if (name.length() <= kMaxNameLength) {
    return name;
  }
  return name.substr(0, kMaxNameLength - 3) + "...";
}

}  // namespace

CefElementRefIndex::CefElementRefIndex() = default;
CefElementRefIndex::~CefElementRefIndex() = default;

void CefElementRefIndex::SetRefs(std::vector<CefElementRef> refs) {
  base::AutoLock lock(lock_);
  refs_ = std::move(refs);
  generation_++;
}

const CefElementRef* CefElementRefIndex::GetRef(int id) const {
  base::AutoLock lock(lock_);
  if (id < 1 || static_cast<size_t>(id) > refs_.size()) {
    return nullptr;
  }
  return &refs_[id - 1];
}

const CefElementRef* CefElementRefIndex::GetRefByString(
    const std::string& ref_str) const {
  const int id = ParseRefId(ref_str);
  if (id <= 0) {
    return nullptr;
  }
  return GetRef(id);
}

std::vector<CefElementRef> CefElementRefIndex::GetAllRefs() const {
  base::AutoLock lock(lock_);
  return refs_;
}

size_t CefElementRefIndex::GetRefCount() const {
  base::AutoLock lock(lock_);
  return refs_.size();
}

void CefElementRefIndex::Clear() {
  base::AutoLock lock(lock_);
  refs_.clear();
  generation_++;
}

std::string CefElementRefIndex::BuildRefText() const {
  base::AutoLock lock(lock_);
  std::string result;

  for (const auto& ref : refs_) {
    // Format: [1] @e1 button "Submit"
    std::string line =
        base::StringPrintf("[%d] @e%d %s", ref.id, ref.id, ref.role.c_str());

    // Add the element name if present.
    if (!ref.name.empty()) {
      line += base::StringPrintf(" \"%s\"", TruncateName(ref.name).c_str());
    }

    // Add extra annotations for relevant properties.
    if (!ref.input_type.empty()) {
      line += base::StringPrintf(" type=%s", ref.input_type.c_str());
    }
    if (ref.disabled) {
      line += " [disabled]";
    }
    if (ref.focused) {
      line += " [focused]";
    }

    result += line;
    result += "\n";
  }

  return result;
}

bool CefElementRefIndex::HasRefs() const {
  base::AutoLock lock(lock_);
  return !refs_.empty();
}

uint64_t CefElementRefIndex::GetGeneration() const {
  base::AutoLock lock(lock_);
  return generation_;
}

// static
int CefElementRefIndex::ParseRefId(const std::string& ref_str) {
  if (ref_str.empty()) {
    return -1;
  }

  std::string num_str;

  // Handle "@e5" format.
  if (ref_str.size() > 2 && ref_str[0] == '@' && ref_str[1] == 'e') {
    num_str = ref_str.substr(2);
  }
  // Handle "e5" format.
  else if (ref_str.size() > 1 && ref_str[0] == 'e') {
    num_str = ref_str.substr(1);
  }
  // Handle "5" format (plain number).
  else {
    num_str = ref_str;
  }

  int id = -1;
  if (!base::StringToInt(num_str, &id) || id <= 0) {
    return -1;
  }
  return id;
}
