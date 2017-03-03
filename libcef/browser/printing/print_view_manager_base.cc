// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/print_view_manager_base.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/features/features.h"
#include "printing/pdf_metafile_skia.h"
#include "printing/printed_document.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

using base::TimeDelta;
using content::BrowserThread;

namespace printing {

CefPrintViewManagerBase::CefPrintViewManagerBase(
    content::WebContents* web_contents)
    : PrintManager(web_contents),
      printing_rfh_(nullptr),
      printing_succeeded_(false),
      inside_inner_message_loop_(false),
#if !defined(OS_MACOSX)
      expecting_first_page_(true),
#endif
      queue_(g_browser_process->print_job_manager()->queue()) {
  DCHECK(queue_.get());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  printing_enabled_.Init(
      prefs::kPrintingEnabled, profile->GetPrefs(),
      base::Bind(&CefPrintViewManagerBase::UpdatePrintingEnabled,
                 base::Unretained(this)));
}

CefPrintViewManagerBase::~CefPrintViewManagerBase() {
  ReleasePrinterQuery();
  DisconnectFromCurrentPrintJob();
}

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
bool CefPrintViewManagerBase::PrintNow(content::RenderFrameHost* rfh) {
  DisconnectFromCurrentPrintJob();

  SetPrintingRFH(rfh);
  int32_t id = rfh->GetRoutingID();
  return PrintNowInternal(rfh, base::MakeUnique<PrintMsg_PrintPages>(id));
}
#endif

void CefPrintViewManagerBase::UpdatePrintingEnabled() {
  web_contents()->ForEachFrame(
      base::Bind(&CefPrintViewManagerBase::SendPrintingEnabled,
                 base::Unretained(this), printing_enabled_.GetValue()));
}

void CefPrintViewManagerBase::NavigationStopped() {
  // Cancel the current job, wait for the worker to finish.
  TerminatePrintJob(true);
}

base::string16 CefPrintViewManagerBase::RenderSourceName() {
  base::string16 name(web_contents()->GetTitle());
  if (name.empty())
    name = l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE);
  return name;
}

void CefPrintViewManagerBase::OnDidGetPrintedPagesCount(int cookie,
                                                     int number_pages) {
  PrintManager::OnDidGetPrintedPagesCount(cookie, number_pages);
  OpportunisticallyCreatePrintJob(cookie);
}

void CefPrintViewManagerBase::OnDidPrintPage(
  const PrintHostMsg_DidPrintPage_Params& params) {
  if (!OpportunisticallyCreatePrintJob(params.document_cookie))
    return;

  PrintedDocument* document = print_job_->document();
  if (!document || params.document_cookie != document->cookie()) {
    // Out of sync. It may happen since we are completely asynchronous. Old
    // spurious messages can be received if one of the processes is overloaded.
    return;
  }

#if defined(OS_MACOSX)
  const bool metafile_must_be_valid = true;
#else
  const bool metafile_must_be_valid = expecting_first_page_;
  expecting_first_page_ = false;
#endif

  // Only used when |metafile_must_be_valid| is true.
  std::unique_ptr<base::SharedMemory> shared_buf;
  if (metafile_must_be_valid) {
    if (!base::SharedMemory::IsHandleValid(params.metafile_data_handle)) {
      NOTREACHED() << "invalid memory handle";
      web_contents()->Stop();
      return;
    }
    shared_buf =
        base::MakeUnique<base::SharedMemory>(params.metafile_data_handle, true);
    if (!shared_buf->Map(params.data_size)) {
      NOTREACHED() << "couldn't map";
      web_contents()->Stop();
      return;
    }
  } else {
    if (base::SharedMemory::IsHandleValid(params.metafile_data_handle)) {
      NOTREACHED() << "unexpected valid memory handle";
      web_contents()->Stop();
      base::SharedMemory::CloseHandle(params.metafile_data_handle);
      return;
    }
  }

  std::unique_ptr<PdfMetafileSkia> metafile(
      new PdfMetafileSkia(PDF_SKIA_DOCUMENT_TYPE));
  if (metafile_must_be_valid) {
    if (!metafile->InitFromData(shared_buf->memory(), params.data_size)) {
      NOTREACHED() << "Invalid metafile header";
      web_contents()->Stop();
      return;
    }
  }

#if defined(OS_WIN)
  print_job_->AppendPrintedPage(params.page_number);
  if (metafile_must_be_valid) {
    scoped_refptr<base::RefCountedBytes> bytes = new base::RefCountedBytes(
        reinterpret_cast<const unsigned char*>(shared_buf->memory()),
        params.data_size);

    document->DebugDumpData(bytes.get(), FILE_PATH_LITERAL(".pdf"));

    const auto& settings = document->settings();
    if ((settings.printer_is_ps2() || settings.printer_is_ps3()) &&
        base::FeatureList::IsEnabled(features::kPostScriptPrinting)) {
      print_job_->StartPdfToPostScriptConversion(bytes, params.content_area,
                                                 params.physical_offsets,
                                                 settings.printer_is_ps2());
    } else {
      // TODO(thestig): Figure out why rendering text with GDI results in random
      // missing characters for some users. https://crbug.com/658606
      // Update : The missing letters seem to have been caused by the same
      // problem as https://crbug.com/659604 which was resolved. GDI printing
      // seems to work with the fix for this bug applied.
      bool print_text_with_gdi = settings.print_text_with_gdi() &&
                                 !settings.printer_is_xps() &&
                                 base::FeatureList::IsEnabled(
                                     features::kGdiTextPrinting);
      print_job_->StartPdfToEmfConversion(
          bytes, params.page_size, params.content_area, print_text_with_gdi);
    }
  }
#else
  // Update the rendered document. It will send notifications to the listener.
  document->SetPage(params.page_number,
                    std::move(metafile),
#if defined(OS_WIN)
                    0.0f /* dummy shrink_factor */,
#endif
                    params.page_size,
                    params.content_area);

  ShouldQuitFromInnerMessageLoop();
#endif
}

void CefPrintViewManagerBase::OnPrintingFailed(int cookie) {
  PrintManager::OnPrintingFailed(cookie);

  ReleasePrinterQuery();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_RELEASED,
      content::Source<content::WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void CefPrintViewManagerBase::OnShowInvalidPrinterSettingsError() {
}

void CefPrintViewManagerBase::DidStartLoading() {
  UpdatePrintingEnabled();
}

void CefPrintViewManagerBase::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // Terminates or cancels the print job if one was pending.
  if (render_frame_host != printing_rfh_)
    return;

  printing_rfh_ = nullptr;

  PrintManager::PrintingRenderFrameDeleted();
  ReleasePrinterQuery();

  if (!print_job_.get())
    return;

  scoped_refptr<PrintedDocument> document(print_job_->document());
  if (document.get()) {
    // If IsComplete() returns false, the document isn't completely rendered.
    // Since our renderer is gone, there's nothing to do, cancel it. Otherwise,
    // the print job may finish without problem.
    TerminatePrintJob(!document->IsComplete());
  }
}

bool CefPrintViewManagerBase::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefPrintViewManagerBase, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintPage, OnDidPrintPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ShowInvalidPrinterSettingsError,
                        OnShowInvalidPrinterSettingsError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || PrintManager::OnMessageReceived(message, render_frame_host);
}

void CefPrintViewManagerBase::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);
  OnNotifyPrintJobEvent(*content::Details<JobEventDetails>(details).ptr());
}

void CefPrintViewManagerBase::OnNotifyPrintJobEvent(
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::FAILED: {
      TerminatePrintJob(true);

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PRINT_JOB_RELEASED,
          content::Source<content::WebContents>(web_contents()),
          content::NotificationService::NoDetails());
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED: {
      NOTREACHED();
      break;
    }
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      ShouldQuitFromInnerMessageLoop();
      break;
    }
    case JobEventDetails::NEW_DOC:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE:
    case JobEventDetails::DOC_DONE: {
      // Don't care about the actual printing process.
      break;
    }
    case JobEventDetails::JOB_DONE: {
      // Printing is done, we don't need it anymore.
      // print_job_->is_job_pending() may still be true, depending on the order
      // of object registration.
      printing_succeeded_ = true;
      ReleasePrintJob();

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PRINT_JOB_RELEASED,
          content::Source<content::WebContents>(web_contents()),
          content::NotificationService::NoDetails());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

bool CefPrintViewManagerBase::RenderAllMissingPagesNow() {
  if (!print_job_.get() || !print_job_->is_job_pending())
    return false;

  // We can't print if there is no renderer.
  if (!web_contents() ||
      !web_contents()->GetRenderViewHost() ||
      !web_contents()->GetRenderViewHost()->IsRenderViewLive()) {
    return false;
  }

  // Is the document already complete?
  if (print_job_->document() && print_job_->document()->IsComplete()) {
    printing_succeeded_ = true;
    return true;
  }

  // WebContents is either dying or a second consecutive request to print
  // happened before the first had time to finish. We need to render all the
  // pages in an hurry if a print_job_ is still pending. No need to wait for it
  // to actually spool the pages, only to have the renderer generate them. Run
  // a message loop until we get our signal that the print job is satisfied.
  // PrintJob will send a ALL_PAGES_REQUESTED after having received all the
  // pages it needs. MessageLoop::current()->QuitWhenIdle() will be called as
  // soon as print_job_->document()->IsComplete() is true on either
  // ALL_PAGES_REQUESTED or in DidPrintPage(). The check is done in
  // ShouldQuitFromInnerMessageLoop().
  // BLOCKS until all the pages are received. (Need to enable recursive task)
  if (!RunInnerMessageLoop()) {
    // This function is always called from DisconnectFromCurrentPrintJob() so we
    // know that the job will be stopped/canceled in any case.
    return false;
  }
  return true;
}

void CefPrintViewManagerBase::ShouldQuitFromInnerMessageLoop() {
  // Look at the reason.
  DCHECK(print_job_->document());
  if (print_job_->document() &&
      print_job_->document()->IsComplete() &&
      inside_inner_message_loop_) {
    // We are in a message loop created by RenderAllMissingPagesNow. Quit from
    // it.
    base::MessageLoop::current()->QuitWhenIdle();
    inside_inner_message_loop_ = false;
  }
}

bool CefPrintViewManagerBase::CreateNewPrintJob(PrintJobWorkerOwner* job) {
  DCHECK(!inside_inner_message_loop_);

  // Disconnect the current |print_job_|.
  DisconnectFromCurrentPrintJob();

  // We can't print if there is no renderer.
  if (!web_contents()->GetRenderViewHost() ||
      !web_contents()->GetRenderViewHost()->IsRenderViewLive()) {
    return false;
  }

  // Ask the renderer to generate the print preview, create the print preview
  // view and switch to it, initialize the printer and show the print dialog.
  DCHECK(!print_job_.get());
  DCHECK(job);
  if (!job)
    return false;

  print_job_ = new PrintJob();
  print_job_->Initialize(job, this, number_pages_);
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::Source<PrintJob>(print_job_.get()));
  printing_succeeded_ = false;
  return true;
}

void CefPrintViewManagerBase::DisconnectFromCurrentPrintJob() {
  // Make sure all the necessary rendered page are done. Don't bother with the
  // return value.
  bool result = RenderAllMissingPagesNow();

  // Verify that assertion.
  if (print_job_.get() &&
      print_job_->document() &&
      !print_job_->document()->IsComplete()) {
    DCHECK(!result);
    // That failed.
    TerminatePrintJob(true);
  } else {
    // DO NOT wait for the job to finish.
    ReleasePrintJob();
  }
#if !defined(OS_MACOSX)
  expecting_first_page_ = true;
#endif
}

void CefPrintViewManagerBase::TerminatePrintJob(bool cancel) {
  if (!print_job_.get())
    return;

  if (cancel) {
    // We don't need the metafile data anymore because the printing is canceled.
    print_job_->Cancel();
    inside_inner_message_loop_ = false;
  } else {
    DCHECK(!inside_inner_message_loop_);
    DCHECK(!print_job_->document() || print_job_->document()->IsComplete());

    // WebContents is either dying or navigating elsewhere. We need to render
    // all the pages in an hurry if a print job is still pending. This does the
    // trick since it runs a blocking message loop:
    print_job_->Stop();
  }
  ReleasePrintJob();
}

void CefPrintViewManagerBase::ReleasePrintJob() {
  content::RenderFrameHost* rfh = printing_rfh_;
  printing_rfh_ = nullptr;

  if (!print_job_.get())
    return;

  if (rfh) {
    auto msg = base::MakeUnique<PrintMsg_PrintingDone>(rfh->GetRoutingID(),
                                                       printing_succeeded_);
    rfh->Send(msg.release());
  }

  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                    content::Source<PrintJob>(print_job_.get()));
  print_job_->DisconnectSource();
  // Don't close the worker thread.
  print_job_ = nullptr;
}

bool CefPrintViewManagerBase::RunInnerMessageLoop() {
  // This value may actually be too low:
  //
  // - If we're looping because of printer settings initialization, the premise
  // here is that some poor users have their print server away on a VPN over a
  // slow connection. In this situation, the simple fact of opening the printer
  // can be dead slow. On the other side, we don't want to die infinitely for a
  // real network error. Give the printer 60 seconds to comply.
  //
  // - If we're looping because of renderer page generation, the renderer could
  // be CPU bound, the page overly complex/large or the system just
  // memory-bound.
  static const int kPrinterSettingsTimeout = 60000;
  base::OneShotTimer quit_timer;
  quit_timer.Start(
      FROM_HERE, TimeDelta::FromMilliseconds(kPrinterSettingsTimeout),
      base::MessageLoop::current(), &base::MessageLoop::QuitWhenIdle);

  inside_inner_message_loop_ = true;

  // Need to enable recursive task.
  {
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    base::RunLoop().Run();
  }

  bool success = true;
  if (inside_inner_message_loop_) {
    // Ok we timed out. That's sad.
    inside_inner_message_loop_ = false;
    success = false;
  }

  return success;
}

bool CefPrintViewManagerBase::OpportunisticallyCreatePrintJob(int cookie) {
  if (print_job_.get())
    return true;

  if (!cookie) {
    // Out of sync. It may happens since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return false;
  }

  // The job was initiated by a script. Time to get the corresponding worker
  // thread.
  scoped_refptr<PrinterQuery> queued_query = queue_->PopPrinterQuery(cookie);
  if (!queued_query.get()) {
    NOTREACHED();
    return false;
  }

  if (!CreateNewPrintJob(queued_query.get())) {
    // Don't kill anything.
    return false;
  }

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();
  return true;
}

bool CefPrintViewManagerBase::PrintNowInternal(
    content::RenderFrameHost* rfh,
    std::unique_ptr<IPC::Message> message) {
  // Don't print / print preview interstitials or crashed tabs.
  if (web_contents()->ShowingInterstitialPage() || web_contents()->IsCrashed())
    return false;
  return rfh->Send(message.release());
}

void CefPrintViewManagerBase::SetPrintingRFH(content::RenderFrameHost* rfh) {
  DCHECK(!printing_rfh_);
  printing_rfh_ = rfh;
}

void CefPrintViewManagerBase::ReleasePrinterQuery() {
  if (!cookie_)
    return;

  int cookie = cookie_;
  cookie_ = 0;

  PrintJobManager* print_job_manager = g_browser_process->print_job_manager();
  // May be NULL in tests.
  if (!print_job_manager)
    return;

  scoped_refptr<PrinterQuery> printer_query;
  printer_query = queue_->PopPrinterQuery(cookie);
  if (!printer_query.get())
    return;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrinterQuery::StopWorker, printer_query));
}

void CefPrintViewManagerBase::SendPrintingEnabled(
    bool enabled,
    content::RenderFrameHost* rfh) {
  rfh->Send(new PrintMsg_SetPrintingEnabled(rfh->GetRoutingID(), enabled));
}

}  // namespace printing
