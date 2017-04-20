// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/printing_message_filter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "printing/features/features.h"

#if defined(OS_LINUX)
#include "libcef/browser/printing/print_dialog_linux.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

// There's a race condition between deletion of the CefPrintingMessageFilter
// object on the UI thread and deletion of the PrefService (owned by Profile)
// on the UI thread. If the PrefService will be deleted first then
// PrefMember::Destroy() must be called from ShutdownOnUIThread() to avoid
// heap-use-after-free on CefPrintingMessageFilter destruction (due to
// ~PrefMember trying to access the already-deleted PrefService).
// ShutdownNotifierFactory makes sure that ShutdownOnUIThread() is called in
// this case.
class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "CefPrintingMessageFilter") {
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

base::LazyInstance<ShutdownNotifierFactory>::Leaky
    g_shutdown_notifier_factory = LAZY_INSTANCE_INITIALIZER;

// static
ShutdownNotifierFactory* ShutdownNotifierFactory::GetInstance() {
  return g_shutdown_notifier_factory.Pointer();
}

}  // namespace

CefPrintingMessageFilter::CefPrintingMessageFilter(int render_process_id,
                                                   Profile* profile)
    : content::BrowserMessageFilter(PrintMsgStart),
      render_process_id_(render_process_id),
      queue_(g_browser_process->print_job_manager()->queue()) {
  DCHECK(queue_.get());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(profile)->Subscribe(
          base::Bind(&CefPrintingMessageFilter::ShutdownOnUIThread,
                     base::Unretained(this)));

  is_printing_enabled_.Init(prefs::kPrintingEnabled, profile->GetPrefs());
  is_printing_enabled_.MoveToThread(
      content::BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
}

void CefPrintingMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

CefPrintingMessageFilter::~CefPrintingMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CefPrintingMessageFilter::ShutdownOnUIThread() {
  is_printing_enabled_.Destroy();
  shutdown_notifier_.reset();
}

void CefPrintingMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
#if defined(OS_ANDROID)
  if (message.type() == PrintHostMsg_AllocateTempFileForPrinting::ID ||
      message.type() == PrintHostMsg_TempFileForPrintingWritten::ID) {
    *thread = BrowserThread::UI;
  }
#endif
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
#if defined(OS_ANDROID)
  // We need to save the routing ID here because Send method below deletes the
  // |reply_msg| before we can get the routing ID for the Android code.
  int routing_id = reply_msg->routing_id();
#endif
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
  if (params.params.dpi && params.params.document_cookie) {
#if defined(OS_ANDROID)
    int file_descriptor;
    const base::string16& device_name = printer_query->settings().device_name();
    if (base::StringToInt(device_name, &file_descriptor)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CefPrintingMessageFilter::UpdateFileDescriptor, this,
                     routing_id, file_descriptor));
    }
#endif
    queue_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

void CefPrintingMessageFilter::OnUpdatePrintSettings(
    int document_cookie, const base::DictionaryValue& job_settings,
    IPC::Message* reply_msg) {
  std::unique_ptr<base::DictionaryValue> new_settings(job_settings.DeepCopy());

  scoped_refptr<PrinterQuery> printer_query;
  if (!is_printing_enabled_.GetValue()) {
    // Reply with NULL query.
    OnUpdatePrintSettingsReply(printer_query, reply_msg);
    return;
  }
  printer_query = queue_->PopPrinterQuery(document_cookie);
  if (!printer_query.get()) {
    int host_id;
    int routing_id;
    if (!new_settings->GetInteger(kPreviewInitiatorHostId, &host_id) ||
        !new_settings->GetInteger(kPreviewInitiatorRoutingId, &routing_id)) {
      host_id = content::ChildProcessHost::kInvalidUniqueID;
      routing_id = MSG_ROUTING_NONE;
    }
    printer_query = queue_->CreatePrinterQuery(host_id, routing_id);
  }
  printer_query->SetSettings(
      std::move(new_settings),
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

void CefPrintingMessageFilter::OnCheckForCancel(int32_t preview_ui_id,
                                                int preview_request_id,
                                                bool* cancel) {
  *cancel = false;
}

}  // namespace printing
