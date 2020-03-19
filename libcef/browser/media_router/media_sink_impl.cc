// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/media_router/media_sink_impl.h"

CefMediaSinkImpl::CefMediaSinkImpl(const media_router::MediaSink& sink)
    : sink_(sink) {}

CefMediaSinkImpl::CefMediaSinkImpl(const media_router::MediaSink::Id& sink_id,
                                   const std::string& sink_name)
    : sink_(sink_id, sink_name, media_router::SinkIconType::GENERIC) {}

CefString CefMediaSinkImpl::GetId() {
  return sink_.id();
}

bool CefMediaSinkImpl::IsValid() {
  return true;
}

CefString CefMediaSinkImpl::GetName() {
  return sink_.name();
}

CefString CefMediaSinkImpl::GetDescription() {
  return sink_.description().value_or("");
}

bool CefMediaSinkImpl::IsCastSink() {
  return sink_.provider_id() == media_router::CAST;
}

bool CefMediaSinkImpl::IsDialSink() {
  return sink_.provider_id() == media_router::DIAL;
}

bool CefMediaSinkImpl::IsCompatibleWith(CefRefPtr<CefMediaSource> source) {
  if (source) {
    if (IsCastSink())
      return source->IsCastSource();
    if (IsDialSink())
      return source->IsDialSource();
  }
  return false;
}
