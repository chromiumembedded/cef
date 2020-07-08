// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/media_router/media_source_impl.h"

CefMediaSourceImpl::CefMediaSourceImpl(
    const media_router::MediaSource::Id& source_id)
    : source_(source_id) {}

CefMediaSourceImpl::CefMediaSourceImpl(const GURL& presentation_url)
    : source_(presentation_url) {}

CefString CefMediaSourceImpl::GetId() {
  return source_.id();
}

bool CefMediaSourceImpl::IsCastSource() {
  return !IsDialSource();
}

bool CefMediaSourceImpl::IsDialSource() {
  return source_.IsDialSource();
}
