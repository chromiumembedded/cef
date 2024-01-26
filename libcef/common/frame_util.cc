// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/frame_util.h"

#include "libcef/browser/thread_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace frame_util {

content::GlobalRenderFrameHostId GetGlobalId(
    content::NavigationHandle* navigation_handle) {
  CEF_REQUIRE_UIT();
  return navigation_handle->HasCommitted()
             ? navigation_handle->GetRenderFrameHost()->GetGlobalId()
             : navigation_handle->GetPreviousRenderFrameHostId();
}

std::optional<content::GlobalRenderFrameHostToken> ParseFrameIdentifier(
    const std::string& identifier) {
  if (identifier.size() < 3) {
    return std::nullopt;
  }

  const size_t pos = identifier.find('-');
  if (pos == std::string::npos || pos == 0 || pos == identifier.size() - 1) {
    return std::nullopt;
  }

  std::string_view process_id_str(identifier.c_str(), pos);
  int process_id;
  if (!base::HexStringToInt(process_id_str, &process_id) ||
      !IsValidChildId(process_id)) {
    return std::nullopt;
  }

  std::string_view frame_token_str(identifier.c_str() + pos + 1,
                                   identifier.size() - pos - 1);
  auto token = base::Token::FromString(frame_token_str);
  if (token) {
    auto unguessable_token =
        base::UnguessableToken::Deserialize(token->high(), token->low());
    if (unguessable_token) {
      return content::GlobalRenderFrameHostToken(
          process_id, blink::LocalFrameToken(unguessable_token.value()));
    }
  }

  return std::nullopt;
}

std::string MakeFrameIdentifier(
    const content::GlobalRenderFrameHostToken& global_token) {
  if (!IsValidGlobalToken(global_token)) {
    return std::string();
  }

  // All upper-case hex values.
  return base::StringPrintf("%X-%s", global_token.child_id,
                            global_token.frame_token.ToString().c_str());
}

std::string GetFrameDebugString(
    const content::GlobalRenderFrameHostId& global_id) {
  return base::StringPrintf("[%d,%d]", global_id.child_id,
                            global_id.frame_routing_id);
}

std::string GetFrameDebugString(
    const content::GlobalRenderFrameHostToken& global_token) {
  return MakeFrameIdentifier(global_token);
}

}  // namespace frame_util
