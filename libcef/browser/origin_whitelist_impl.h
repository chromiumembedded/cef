// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ORIGIN_WHITELIST_IMPL_H_
#define CEF_LIBCEF_BROWSER_ORIGIN_WHITELIST_IMPL_H_

#include <list>
#include <vector>

namespace content {
class RenderProcessHost;
}

class GURL;

struct Cef_CrossOriginWhiteListEntry_Params;

// Called to retrieve the current list of cross-origin white list entries. This
// method is thread safe.
void GetCrossOriginWhitelistEntries(
    std::vector<Cef_CrossOriginWhiteListEntry_Params>* entries);

// Returns true if |source| can access |target| based on the cross-origin white
// list settings.
bool HasCrossOriginWhitelistEntry(const GURL& source, const GURL& target);

#endif  // CEF_LIBCEF_BROWSER_ORIGIN_WHITELIST_IMPL_H_
