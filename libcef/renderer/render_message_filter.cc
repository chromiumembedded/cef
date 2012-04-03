/// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_message_filter.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/common/cef_messages.h"

#include "base/bind.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

CefRenderMessageFilter::CefRenderMessageFilter()
    : channel_(NULL) {
}

CefRenderMessageFilter::~CefRenderMessageFilter() {
}

void CefRenderMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void CefRenderMessageFilter::OnFilterRemoved() {
}

bool CefRenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(CefProcessMsg_RegisterScheme, OnRegisterScheme)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefRenderMessageFilter::OnRegisterScheme(
    const std::string& scheme_name,
    bool is_standard,
    bool is_local,
    bool is_display_isolated) {
  if (is_standard) {
    // Make this registration as early as possible.
    url_parse::Component scheme_comp(0, scheme_name.length());
    if (!url_util::IsStandard(scheme_name.c_str(), scheme_comp))
      url_util::AddStandardScheme(scheme_name.c_str());
  }

  CEF_POST_TASK_RT(
      base::Bind(&CefRenderMessageFilter::RegisterSchemeOnRenderThread, this,
                 scheme_name, is_local, is_display_isolated));
}

void CefRenderMessageFilter::RegisterSchemeOnRenderThread(
    const std::string& scheme_name,
    bool is_local,
    bool is_display_isolated) {
  CEF_REQUIRE_RT();
  if (is_local) {
    WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
        WebKit::WebString::fromUTF8(scheme_name));
  }
  if (is_display_isolated) {
    WebKit::WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(
        WebKit::WebString::fromUTF8(scheme_name));
  }
}
