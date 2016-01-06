// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "libcef/browser/printing/print_view_manager_base.h"

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"

struct PrintHostMsg_DidPreviewDocument_Params;
struct PrintHostMsg_RequestPrintPreview_Params;

namespace content {
class RenderProcessHost;
}

namespace printing {

// Manages the print commands for a WebContents.
class PrintViewManager : public PrintViewManagerBase,
                         public content::WebContentsUserData<PrintViewManager> {
 public:
  ~PrintViewManager() override;

#if defined(ENABLE_BASIC_PRINTING)
  // Same as PrintNow(), but for the case where a user prints with the system
  // dialog from print preview.
  bool PrintForSystemDialogNow();
#endif  // ENABLE_BASIC_PRINTING

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // content::WebContentsObserver implementation.
  // Cancels the print job.
  void NavigationStopped() override;
  // Terminates or cancels the print job if one was pending.
  void RenderProcessGone(base::TerminationStatus status) override;

  // Callback executed on PDF printing completion.
  typedef base::Callback<void(bool /*ok*/)> PdfPrintCallback;

  // Print the current document to a PDF file. Execute |callback| on completion.
  void PrintToPDF(const base::FilePath& path,
                  const CefPdfPrintSettings& settings,
                  const PdfPrintCallback& callback);

 private:
  explicit PrintViewManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrintViewManager>;

  // IPC Message handlers.
  void OnDidShowPrintDialog();
  void OnRequestPrintPreview(const PrintHostMsg_RequestPrintPreview_Params&);
  void OnMetafileReadyForPrinting(
      const PrintHostMsg_DidPreviewDocument_Params&);

  void TerminatePdfPrintJob();

  // Used for printing to PDF. Only accessed on the browser process UI thread.
  int next_pdf_request_id_ = -1;
  base::FilePath pdf_output_path_;
  scoped_ptr<base::DictionaryValue> pdf_print_settings_;
  PdfPrintCallback pdf_print_callback_;

  DISALLOW_COPY_AND_ASSIGN(PrintViewManager);
};

}  // namespace printing

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
