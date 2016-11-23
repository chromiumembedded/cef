// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_message_filter.h"
#include "printing/features/features.h"

struct PrintHostMsg_ScriptedPrint_Params;
class Profile;

namespace base {
class DictionaryValue;
}

namespace printing {

class PrintQueriesQueue;
class PrinterQuery;

// This class filters out incoming printing related IPC messages for the
// renderer process on the IPC thread.
class CefPrintingMessageFilter : public content::BrowserMessageFilter {
 public:
  CefPrintingMessageFilter(int render_process_id, Profile* profile);

  static void EnsureShutdownNotifierFactoryBuilt();

  // content::BrowserMessageFilter methods.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  friend class base::DeleteHelper<CefPrintingMessageFilter>;
  friend class content::BrowserThread;

  ~CefPrintingMessageFilter() override;

  void ShutdownOnUIThread();

  // Get the default print setting.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(scoped_refptr<PrinterQuery> printer_query,
                                      IPC::Message* reply_msg);

  // The renderer host have to show to the user the print dialog and returns
  // the selected print settings. The task is handled by the print worker
  // thread and the UI thread. The reply occurs on the IO thread.
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(scoped_refptr<PrinterQuery> printer_query,
                            IPC::Message* reply_msg);

  // Modify the current print settings based on |job_settings|. The task is
  // handled by the print worker thread and the UI thread. The reply occurs on
  // the IO thread.
  void OnUpdatePrintSettings(int document_cookie,
                             const base::DictionaryValue& job_settings,
                             IPC::Message* reply_msg);
  void OnUpdatePrintSettingsReply(scoped_refptr<PrinterQuery> printer_query,
                                  IPC::Message* reply_msg);

  // Check to see if print preview has been cancelled.
  void OnCheckForCancel(int32_t preview_ui_id,
                        int preview_request_id,
                        bool* cancel);

  BooleanPrefMember is_printing_enabled_;

  const int render_process_id_;

  scoped_refptr<PrintQueriesQueue> queue_;

  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  DISALLOW_COPY_AND_ASSIGN(CefPrintingMessageFilter);
};

}  // namespace printing

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
