// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/extensions/chrome_extension_util.h"

#include "cef/libcef/browser/browser_host_base.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace cef {

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

}  // namespace cef
