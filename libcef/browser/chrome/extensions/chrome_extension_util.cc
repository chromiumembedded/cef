// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/extensions/chrome_extension_util.h"

#include <optional>

#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace cef {

namespace {

// WebContentsUserData attached to an extension action popup's host WebContents,
// recording the session tab id of the Alloy style source tab the popup acts on.
// Its lifetime is exactly the popup host WebContents' lifetime; the id (not a
// WebContents pointer) is stored so the source tab is always re-resolved fresh.
class AlloyPopupSourceTab
    : public content::WebContentsUserData<AlloyPopupSourceTab> {
 public:
  int source_tab_id() const { return source_tab_id_; }

 private:
  friend class content::WebContentsUserData<AlloyPopupSourceTab>;

  AlloyPopupSourceTab(content::WebContents* contents, int source_tab_id)
      : content::WebContentsUserData<AlloyPopupSourceTab>(*contents),
        source_tab_id_(source_tab_id) {}

  const int source_tab_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AlloyPopupSourceTab);

// Mirrors TabsQueryFunction's MatchesBool in tabs_api.cc: an unset filter
// matches any value.
bool MatchesBool(const std::optional<bool>& filter, bool value) {
  return !filter || *filter == value;
}

}  // namespace

bool GetAlloyTabById(int tab_id,
                     Profile* profile,
                     bool include_incognito,
                     content::WebContents** contents) {
  for (auto rph_iterator = content::RenderProcessHost::AllHostsIterator();
       !rph_iterator.IsAtEnd(); rph_iterator.Advance()) {
    content::RenderProcessHost* rph = rph_iterator.GetCurrentValue();

    // Ignore renderers that aren't ready.
    if (!rph->IsInitializedAndNotDead()) {
      continue;
    }
    // Ignore renderers that aren't from a valid profile. This is either the
    // same profile or the incognito profile if `include_incognito` is true.
    Profile* process_profile =
        Profile::FromBrowserContext(rph->GetBrowserContext());
    if (process_profile != profile &&
        !(include_incognito && profile->IsSameOrParent(process_profile))) {
      continue;
    }

    rph->ForEachRenderFrameHost([&contents,
                                 tab_id](content::RenderFrameHost* rfh) {
      CHECK(rfh);
      auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
      CHECK(web_contents);
      if (sessions::SessionTabHelper::IdForTab(web_contents).id() != tab_id) {
        return;
      }

      // We only consider Alloy style CefBrowserHosts in this loop. Otherwise,
      // we could end up returning a WebContents that shouldn't be exposed to
      // extensions.
      auto browser = CefBrowserHostBase::GetBrowserForContents(web_contents);
      if (!browser || !browser->IsAlloyStyle()) {
        return;
      }

      if (contents) {
        *contents = web_contents;
      }
    });

    if (contents && *contents) {
      return true;
    }
  }

  return false;
}

bool IsAlloyContents(content::WebContents* contents, bool primary_only) {
  auto browser = CefBrowserHostBase::GetBrowserForContents(contents);
  if (browser && browser->IsAlloyStyle()) {
    return !primary_only || browser->GetWebContents() == contents;
  }
  return false;
}

void SetAlloyPopupSourceTabId(content::WebContents* popup_contents,
                              int source_tab_id) {
  AlloyPopupSourceTab::CreateForWebContents(popup_contents, source_tab_id);
}

void MaybeAppendAlloyPopupQueryTab(
    ExtensionFunction* function,
    const extensions::api::tabs::Query::Params::QueryInfo& query_info,
    const extensions::URLPatternSet& url_patterns,
    const std::string& window_type,
    int window_id,
    int tab_index,
    base::ListValue* result) {
  // Only tabs.query calls coming from a CEF-opened Alloy popup carry the
  // marker.
  content::WebContents* sender = function->GetSenderWebContents();
  if (!sender) {
    return;
  }
  auto* source = AlloyPopupSourceTab::FromWebContents(sender);
  if (!source) {
    return;
  }

  // Re-resolve the source tab from its id each time so a stale WebContents is
  // never dereferenced if the Alloy tab closed while the popup is still open.
  Profile* profile = Profile::FromBrowserContext(function->browser_context());
  content::WebContents* source_wc = nullptr;
  if (!GetAlloyTabById(source->source_tab_id(), profile,
                       function->include_incognito_information(), &source_wc) ||
      !source_wc) {
    return;
  }

  // An Alloy tab has no window object; present it as the active tab at index 0
  // of a normal, current, last-focused window. Reject queries incompatible with
  // that before doing the work of building the tab object.
  if (!window_type.empty() && window_type != "normal") {
    return;
  }
  if (!MatchesBool(query_info.current_window, true) ||
      !MatchesBool(query_info.last_focused_window, true)) {
    return;
  }
  if (tab_index > -1 && tab_index != 0) {
    return;
  }

  // Reuse Chrome's own population + permission scrubbing to build the tab from
  // the bare WebContents (the lightweight overload needs no
  // BrowserWindowInterface), then mark it active/selected/highlighted at index
  // 0 since it is the tab the popup acts on.
  const extensions::Extension* extension = function->extension();
  auto scrub_behavior = extensions::ExtensionTabUtil::GetScrubTabBehavior(
      extension, function->source_context_type(), source_wc);
  auto tab = extensions::ExtensionTabUtil::CreateTabObject(
      source_wc, scrub_behavior, extension);
  tab.active = true;
  tab.selected = true;
  tab.highlighted = true;
  tab.index = 0;

  // A concrete target window id must match; -1 (unknown) and the current-window
  // sentinel (both < 0) fall through, already covered by current_window above.
  if (window_id >= 0 && window_id != tab.window_id) {
    return;
  }

  // Remaining MatchesTab bool filters, read from the populated object.
  const bool muted = tab.muted_info && tab.muted_info->muted;
  if (!MatchesBool(query_info.active, tab.active) ||
      !MatchesBool(query_info.highlighted, tab.highlighted) ||
      !MatchesBool(query_info.pinned, tab.pinned) ||
      !MatchesBool(query_info.audible, tab.audible.value_or(false)) ||
      !MatchesBool(query_info.muted, muted) ||
      !MatchesBool(query_info.discarded, tab.discarded) ||
      !MatchesBool(query_info.auto_discardable, tab.auto_discardable) ||
      !MatchesBool(query_info.frozen, tab.frozen)) {
    return;
  }

  // group/split filters: tab.group_id and tab.split_view_id are -1 when absent.
  if (query_info.group_id && *query_info.group_id != tab.group_id) {
    return;
  }
  if (query_info.split_view_id &&
      *query_info.split_view_id != tab.split_view_id.value_or(-1)) {
    return;
  }

  // Loading-status filter.
  if (query_info.status != extensions::api::tabs::TabStatus::kNone &&
      query_info.status != tab.status) {
    return;
  }

  // url/title operate on privileged data: CreateTabObject already scrubbed them
  // to nullopt when the extension lacks permission, so a set filter simply
  // fails to match in that case (mirroring MatchesTab).
  if (!url_patterns.is_empty() &&
      (!tab.url || !url_patterns.MatchesURL(GURL(*tab.url)))) {
    return;
  }
  if (query_info.title && !query_info.title->empty() &&
      (!tab.title ||
       !base::MatchPattern(base::UTF8ToUTF16(*tab.title),
                           base::UTF8ToUTF16(*query_info.title)))) {
    return;
  }

  result->Append(tab.ToValue());
}

}  // namespace cef
