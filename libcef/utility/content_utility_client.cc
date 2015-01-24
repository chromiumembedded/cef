// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/content_utility_client.h"

#include "chrome/common/chrome_utility_messages.h"
#include "chrome/utility/utility_message_handler.h"
#include "content/public/utility/utility_thread.h"

#if defined(OS_WIN)
#include "libcef/utility/printing_handler.h"
#endif

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

}  // namespace

CefContentUtilityClient::CefContentUtilityClient() {
#if defined(OS_WIN)
  handlers_.push_back(new PrintingHandler());
#endif
}

CefContentUtilityClient::~CefContentUtilityClient() {
}

bool CefContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(CefContentUtilityClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_StartupPing, OnStartupPing)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  for (Handlers::iterator it = handlers_.begin();
       !handled && it != handlers_.end(); ++it) {
    handled = (*it)->OnMessageReceived(message);
  }

  return handled;
}

// static
void CefContentUtilityClient::PreSandboxStartup() {
#if defined(OS_WIN)
  PrintingHandler::PreSandboxStartup();
#endif
}

void CefContentUtilityClient::OnStartupPing() {
  Send(new ChromeUtilityHostMsg_ProcessStarted);
  // Don't release the process, we assume further messages are on the way.
}
