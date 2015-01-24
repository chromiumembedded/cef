// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/print_view_manager.h"

#include <map>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(printing::PrintViewManager);

namespace printing {

PrintViewManager::PrintViewManager(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents) {
}

PrintViewManager::~PrintViewManager() {
}

#if defined(ENABLE_BASIC_PRINTING)
bool PrintViewManager::PrintForSystemDialogNow() {
  return PrintNowInternal(new PrintMsg_PrintForSystemDialog(routing_id()));
}
#endif  // ENABLE_BASIC_PRINTING

void PrintViewManager::RenderProcessGone(base::TerminationStatus status) {
  PrintViewManagerBase::RenderProcessGone(status);
}

void PrintViewManager::OnDidShowPrintDialog() {
}

bool PrintViewManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintViewManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidShowPrintDialog, OnDidShowPrintDialog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled ? true : PrintViewManagerBase::OnMessageReceived(message);
}

}  // namespace printing
