// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/navigation_entry_impl.h"

#include "libcef/browser/ssl_status_impl.h"
#include "libcef/common/time_util.h"

#include "content/public/browser/navigation_entry.h"
#include "url/gurl.h"

CefNavigationEntryImpl::CefNavigationEntryImpl(content::NavigationEntry* value)
    : CefValueBase<CefNavigationEntry, content::NavigationEntry>(
          value,
          nullptr,
          kOwnerNoDelete,
          false,
          new CefValueControllerNonThreadSafe()) {
  // Indicate that this object owns the controller.
  SetOwnsController();
}

bool CefNavigationEntryImpl::IsValid() {
  return !detached();
}

CefString CefNavigationEntryImpl::GetURL() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return mutable_value()->GetURL().spec();
}

CefString CefNavigationEntryImpl::GetDisplayURL() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return mutable_value()->GetVirtualURL().spec();
}

CefString CefNavigationEntryImpl::GetOriginalURL() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return mutable_value()->GetUserTypedURL().spec();
}

CefString CefNavigationEntryImpl::GetTitle() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return mutable_value()->GetTitle();
}

CefNavigationEntry::TransitionType CefNavigationEntryImpl::GetTransitionType() {
  CEF_VALUE_VERIFY_RETURN(false, TT_EXPLICIT);
  return static_cast<TransitionType>(mutable_value()->GetTransitionType());
}

bool CefNavigationEntryImpl::HasPostData() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return mutable_value()->GetHasPostData();
}

CefBaseTime CefNavigationEntryImpl::GetCompletionTime() {
  CEF_VALUE_VERIFY_RETURN(false, CefBaseTime());
  return mutable_value()->GetTimestamp();
}

int CefNavigationEntryImpl::GetHttpStatusCode() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return mutable_value()->GetHttpStatusCode();
}

CefRefPtr<CefSSLStatus> CefNavigationEntryImpl::GetSSLStatus() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return new CefSSLStatusImpl(mutable_value()->GetSSL());
}
