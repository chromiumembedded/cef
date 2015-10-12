// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCEF_BROWSER_PRINTING_PRINT_DIALOG_LINUX_H_
#define LIBCEF_BROWSER_PRINTING_PRINT_DIALOG_LINUX_H_

#include "include/cef_print_handler.h"

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "printing/print_dialog_gtk_interface.h"
#include "printing/printing_context_linux.h"

namespace printing {
class MetafilePlayer;
class PrintSettings;
}

using printing::PrintingContextLinux;

// Needs to be freed on the UI thread to clean up its member variables.
class CefPrintDialogLinux
    : public printing::PrintDialogGtkInterface,
      public base::RefCountedThreadSafe<
          CefPrintDialogLinux, content::BrowserThread::DeleteOnUIThread> {
 public:
  // Creates and returns a print dialog.
  static printing::PrintDialogGtkInterface* CreatePrintDialog(
      PrintingContextLinux* context);

  // Returns the paper size in device units.
  static gfx::Size GetPdfPaperSize(
    printing::PrintingContextLinux* context);

  // Notify the client when printing has started.
  static void OnPrintStart(int render_process_id,
                           int render_routing_id);

  // printing::CefPrintDialogLinuxInterface implementation.
  void UseDefaultSettings() override;
  bool UpdateSettings(printing::PrintSettings* settings) override;
  void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      const PrintingContextLinux::PrintSettingsCallback& callback) override;
  void PrintDocument(const printing::MetafilePlayer& metafile,
                     const base::string16& document_name) override;
  void AddRefToDialog() override;
  void ReleaseDialog() override;

 private:
  friend class base::DeleteHelper<CefPrintDialogLinux>;
  friend class base::RefCountedThreadSafe<
      CefPrintDialogLinux, content::BrowserThread::DeleteOnUIThread>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class CefPrintDialogCallbackImpl;
  friend class CefPrintJobCallbackImpl;

  explicit CefPrintDialogLinux(PrintingContextLinux* context);
  ~CefPrintDialogLinux() override;

  void SetHandler();
  void ReleaseHandler();

  bool UpdateSettings(printing::PrintSettings* settings,
                      bool get_defaults);

  // Prints document named |document_name|.
  void SendDocumentToPrinter(const base::string16& document_name);

  // Handles print dialog response.
  void OnPrintContinue(CefRefPtr<CefPrintSettings> settings);
  void OnPrintCancel();

  // Handles print job response.
  void OnJobCompleted();

  CefRefPtr<CefPrintHandler> handler_;

  // Printing dialog callback.
  PrintingContextLinux::PrintSettingsCallback callback_;
  PrintingContextLinux* context_;

  base::FilePath path_to_pdf_;

  DISALLOW_COPY_AND_ASSIGN(CefPrintDialogLinux);
};

#endif  // LIBCEF_BROWSER_PRINTING_PRINT_DIALOG_LINUX_H_
