// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/printing_message_filter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"

#if defined(OS_LINUX)
#include "libcef/browser/printing/print_dialog_linux.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

class CefPrintingMessageFilterShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static CefPrintingMessageFilterShutdownNotifierFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<
      CefPrintingMessageFilterShutdownNotifierFactory>;

  CefPrintingMessageFilterShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "CefPrintingMessageFilter") {}

  ~CefPrintingMessageFilterShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(CefPrintingMessageFilterShutdownNotifierFactory);
};

base::LazyInstance<CefPrintingMessageFilterShutdownNotifierFactory>::Leaky
    g_printing_message_filter_shutdown_notifier_factory =
        LAZY_INSTANCE_INITIALIZER;

// static
CefPrintingMessageFilterShutdownNotifierFactory*
CefPrintingMessageFilterShutdownNotifierFactory::GetInstance() {
  return g_printing_message_filter_shutdown_notifier_factory.Pointer();
}

}  // namespace

CefPrintingMessageFilter::CefPrintingMessageFilter(int render_process_id,
                                                   Profile* profile)
    : content::BrowserMessageFilter(PrintMsgStart),
      render_process_id_(render_process_id),
      queue_(g_browser_process->print_job_manager()->queue()) {
  DCHECK(queue_.get());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  printing_shutdown_notifier_ =
      CefPrintingMessageFilterShutdownNotifierFactory::GetInstance()
          ->Get(profile)
          ->Subscribe(base::Bind(&CefPrintingMessageFilter::ShutdownOnUIThread,
                                 base::Unretained(this)));
  is_printing_enabled_.Init(prefs::kPrintingEnabled, profile->GetPrefs());
  is_printing_enabled_.MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
}

void CefPrintingMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  CefPrintingMessageFilterShutdownNotifierFactory::GetInstance();
}

CefPrintingMessageFilter::~CefPrintingMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CefPrintingMessageFilter::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_printing_enabled_.Destroy();
  printing_shutdown_notifier_.reset();
}

void CefPrintingMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool CefPrintingMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefPrintingMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_UpdatePrintSettings,
                                    OnUpdatePrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefPrintingMessageFilter::OnGetDefaultPrintSettings(
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_LINUX)
  // Send notification to the client.
  CefPrintDialogLinux::OnPrintStart(render_process_id_,
                                    reply_msg->routing_id());
#endif

  scoped_refptr<PrinterQuery> printer_query;
  if (!is_printing_enabled_.GetValue()) {
    // Reply with NULL query.
    OnGetDefaultPrintSettingsReply(printer_query, reply_msg);
    return;
  }
  printer_query = queue_->PopPrinterQuery(0);
  if (!printer_query.get()) {
    printer_query =
        queue_->CreatePrinterQuery(render_process_id_, reply_msg->routing_id());
  }

  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(
      PrinterQuery::GetSettingsAskParam::DEFAULTS, 0, false, DEFAULT_MARGINS,
      false, false,
      base::Bind(&CefPrintingMessageFilter::OnGetDefaultPrintSettingsReply,
                 this, printer_query, reply_msg));
}

void CefPrintingMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  PrintMsg_Print_Params params;
  if (!printer_query.get() ||
      printer_query->last_status() != PrintingContext::OK) {
    params.Reset();
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If printing was enabled.
  if (printer_query.get()) {
    // If user hasn't cancelled.
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      queue_->QueuePrinterQuery(printer_query.get());
    } else {
      printer_query->StopWorker();
    }
  }
}

void CefPrintingMessageFilter::OnScriptedPrint(
    const PrintHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
  scoped_refptr<PrinterQuery> printer_query =
      queue_->PopPrinterQuery(params.cookie);
  if (!printer_query.get()) {
    printer_query =
        queue_->CreatePrinterQuery(render_process_id_, reply_msg->routing_id());
  }
  printer_query->GetSettings(
      PrinterQuery::GetSettingsAskParam::ASK_USER, params.expected_pages_count,
      params.has_selection, params.margin_type, params.is_scripted,
      params.is_modifiable,
      base::Bind(&CefPrintingMessageFilter::OnScriptedPrintReply, this,
                 printer_query, reply_msg));
}

void CefPrintingMessageFilter::OnScriptedPrintReply(
    scoped_refptr<PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  PrintMsg_PrintPages_Params params;
  if (printer_query->last_status() != PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    params.Reset();
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages = PageRange::GetPages(printer_query->settings().ranges());
  }
  PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (!params.params.dpi.IsEmpty() && params.params.document_cookie) {
    queue_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

void CefPrintingMessageFilter::OnUpdatePrintSettings(int document_cookie,
                                                     base::Value job_settings,
                                                     IPC::Message* reply_msg) {
  scoped_refptr<PrinterQuery> printer_query;
  if (!is_printing_enabled_.GetValue()) {
    // Reply with NULL query.
    OnUpdatePrintSettingsReply(printer_query, reply_msg);
    return;
  }
  printer_query = queue_->PopPrinterQuery(document_cookie);
  if (!printer_query.get()) {
    printer_query = queue_->CreatePrinterQuery(
        content::ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }
  printer_query->SetSettings(
      std::move(job_settings),
      base::Bind(&CefPrintingMessageFilter::OnUpdatePrintSettingsReply, this,
                 printer_query, reply_msg));
}

void CefPrintingMessageFilter::OnUpdatePrintSettingsReply(
    scoped_refptr<PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  PrintMsg_PrintPages_Params params;
  if (!printer_query.get() ||
      printer_query->last_status() != PrintingContext::OK) {
    params.Reset();
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages = PageRange::GetPages(printer_query->settings().ranges());
  }
  bool canceled = printer_query.get() &&
                  (printer_query->last_status() == PrintingContext::CANCEL);
  PrintHostMsg_UpdatePrintSettings::WriteReplyParams(reply_msg, params,
                                                     canceled);
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query.get()) {
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      queue_->QueuePrinterQuery(printer_query.get());
    } else {
      printer_query->StopWorker();
    }
  }
}

void CefPrintingMessageFilter::OnCheckForCancel(
    const PrintHostMsg_PreviewIds& ids,
    bool* cancel) {
  *cancel = false;
}

}  // namespace printing
