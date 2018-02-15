// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASE_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/prefs/pref_member.h"
#include "components/printing/browser/print_manager.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/features/features.h"

struct PrintHostMsg_DidPrintDocument_Params;

namespace base {
class RefCountedBytes;
}

namespace content {
class RenderFrameHost;
}

namespace printing {

class JobEventDetails;
class PrintJob;
class PrintJobWorkerOwner;
class PrintQueriesQueue;
class PrintedDocument;
class PrinterQuery;

// Base class for managing the print commands for a WebContents.
class CefPrintViewManagerBase : public content::NotificationObserver,
                                public PrintManager {
 public:
  ~CefPrintViewManagerBase() override;

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  virtual bool PrintNow(content::RenderFrameHost* rfh);
#endif  // ENABLE_BASIC_PRINTING

  // Whether printing is enabled or not.
  void UpdatePrintingEnabled();

  base::string16 RenderSourceName();

 protected:
  explicit CefPrintViewManagerBase(content::WebContents* web_contents);

  // Helper method for Print*Now().
  bool PrintNowInternal(content::RenderFrameHost* rfh,
                        std::unique_ptr<IPC::Message> message);

  void SetPrintingRFH(content::RenderFrameHost* rfh);

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // Cancels the print job.
  void NavigationStopped() override;

 private:
  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::WebContentsObserver implementation.
  void DidStartLoading() override;

  // IPC Message handlers.
  void OnDidGetPrintedPagesCount(int cookie, int number_pages) override;
  void OnPrintingFailed(int cookie) override;
  void OnShowInvalidPrinterSettingsError();
  void OnDidPrintDocument(const PrintHostMsg_DidPrintDocument_Params& params);

  // IPC message handlers for service.
  void OnComposePdfDone(const PrintHostMsg_DidPrintDocument_Params& params,
                        mojom::PdfCompositor::Status status,
                        mojo::ScopedSharedBufferHandle handle);

  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnNotifyPrintJobEvent(const JobEventDetails& event_details);

  // Requests the RenderView to render all the missing pages for the print job.
  // No-op if no print job is pending. Returns true if at least one page has
  // been requested to the renderer.
  bool RenderAllMissingPagesNow();

  // Checks that synchronization is correct and a print query exists for
  // |cookie|. If so, returns the document associated with the cookie.
  PrintedDocument* GetDocument(int cookie);

  // Starts printing |document| with the given |print_data|. This method assumes
  // |print_data| contains valid data.
  void PrintDocument(PrintedDocument* document,
                     const scoped_refptr<base::RefCountedBytes>& print_data,
                     const gfx::Size& page_size,
                     const gfx::Rect& content_area,
                     const gfx::Point& offsets);

  // Quits the current message loop if these conditions hold true: a document is
  // loaded and is complete and waiting_for_pages_to_be_rendered_ is true. This
  // function is called in DidPrintDocument() or on ALL_PAGES_REQUESTED
  // notification. The inner message loop is created was created by
  // RenderAllMissingPagesNow().
  void ShouldQuitFromInnerMessageLoop();

  // Creates a new empty print job. It has no settings loaded. If there is
  // currently a print job, safely disconnect from it. Returns false if it is
  // impossible to safely disconnect from the current print job or it is
  // impossible to create a new print job.
  bool CreateNewPrintJob(PrintJobWorkerOwner* job);

  // Makes sure the current print_job_ has all its data before continuing, and
  // disconnect from it.
  void DisconnectFromCurrentPrintJob();

  // Terminates the print job. No-op if no print job has been created. If
  // |cancel| is true, cancel it instead of waiting for the job to finish. Will
  // call ReleasePrintJob().
  void TerminatePrintJob(bool cancel);

  // Releases print_job_. Correctly deregisters from notifications. No-op if
  // no print job has been created.
  void ReleasePrintJob();

  // Runs an inner message loop. It will set inside_inner_message_loop_ to true
  // while the blocking inner message loop is running. This is useful in cases
  // where the RenderView is about to be destroyed while a printing job isn't
  // finished.
  bool RunInnerMessageLoop();

  // In the case of Scripted Printing, where the renderer is controlling the
  // control flow, print_job_ is initialized whenever possible. No-op is
  // print_job_ is initialized.
  bool OpportunisticallyCreatePrintJob(int cookie);

  // Release the PrinterQuery associated with our |cookie_|.
  void ReleasePrinterQuery();

  // Helper method for UpdatePrintingEnabled().
  void SendPrintingEnabled(bool enabled, content::RenderFrameHost* rfh);

  content::NotificationRegistrar registrar_;

  // The current RFH that is printing with a system printing dialog.
  content::RenderFrameHost* printing_rfh_;

  // Manages the low-level talk to the printer.
  scoped_refptr<PrintJob> print_job_;

  // Indication of success of the print job.
  bool printing_succeeded_;

  // Running an inner message loop inside RenderAllMissingPagesNow(). This means
  // we are _blocking_ until all the necessary pages have been rendered or the
  // print settings are being loaded.
  bool inside_inner_message_loop_;

  // Whether printing is enabled.
  BooleanPrefMember printing_enabled_;

  scoped_refptr<printing::PrintQueriesQueue> queue_;

  base::WeakPtrFactory<CefPrintViewManagerBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefPrintViewManagerBase);
};

}  // namespace printing

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASE_H_
