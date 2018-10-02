/// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_message_filter.h"

#include "libcef/common/cef_messages.h"
#include "libcef/renderer/thread_util.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/task/post_task.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "url/gurl.h"
#include "url/url_util.h"

using content::BrowserThread;

CefRenderMessageFilter::CefRenderMessageFilter() : channel_(NULL) {}

CefRenderMessageFilter::~CefRenderMessageFilter() {}

void CefRenderMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void CefRenderMessageFilter::OnFilterRemoved() {
  channel_ = nullptr;
}

bool CefRenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsCrashReportingEnabled,
                        OnIsCrashReportingEnabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Based on BrowserMessageFilter::Send.
bool CefRenderMessageFilter::Send(IPC::Message* message) {
  if (message->is_sync()) {
    // We don't support sending synchronous messages from the browser.  If we
    // really needed it, we can make this class derive from SyncMessageFilter
    // but it seems better to not allow sending synchronous messages from the
    // browser, since it might allow a corrupt/malicious renderer to hang us.
    NOTREACHED() << "Can't send sync message through CefRenderMessageFilter!";
    return false;
  }

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::Bind(base::IgnoreResult(&CefRenderMessageFilter::Send), this,
                   message));
    return true;
  }

  if (channel_)
    return channel_->Send(message);

  delete message;
  return false;
}

void CefRenderMessageFilter::OnIsCrashReportingEnabled(bool* enabled) {
  // TODO(cef): Explore whether it's useful for CEF clients to report when crash
  // reporting is enabled.
  *enabled = false;
}
