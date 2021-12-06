// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCEF_BROWSER_PRINTING_PRINT_DIALOG_LINUX_H_
#define LIBCEF_BROWSER_PRINTING_PRINT_DIALOG_LINUX_H_

#include "include/cef_print_handler.h"
#include "libcef/browser/browser_host_base.h"

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "printing/print_dialog_gtk_interface.h"
#include "printing/printing_context_linux.h"

namespace printing {
class MetafilePlayer;
class PrintSettings;
}  // namespace printing

using printing::PrintingContextLinux;

// Needs to be freed on the UI thread to clean up its member variables.
class CefPrintDialogLinux : public printing::PrintDialogGtkInterface,
                            public base::RefCountedThreadSafe<
                                CefPrintDialogLinux,
                                content::BrowserThread::DeleteOnUIThread> {
 public:
  CefPrintDialogLinux(const CefPrintDialogLinux&) = delete;
  CefPrintDialogLinux& operator=(const CefPrintDialogLinux&) = delete;

  // Creates and returns a print dialog.
  static printing::PrintDialogGtkInterface* CreatePrintDialog(
      PrintingContextLinux* context);

  // Returns the paper size in device units.
  static gfx::Size GetPdfPaperSize(printing::PrintingContextLinux* context);

  // Notify the client when printing has started.
  static void OnPrintStart(CefRefPtr<CefBrowserHostBase> browser);

  // PrintDialogGtkInterface implementation.
  void UseDefaultSettings() override;
  void UpdateSettings(
      std::unique_ptr<printing::PrintSettings> settings) override;
  void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      PrintingContextLinux::PrintSettingsCallback callback) override;
  void PrintDocument(const printing::MetafilePlayer& metafile,
                     const std::u16string& document_name) override;
  void AddRefToDialog() override;
  void ReleaseDialog() override;

 private:
  friend class base::DeleteHelper<CefPrintDialogLinux>;
  friend class base::RefCountedThreadSafe<
      CefPrintDialogLinux,
      content::BrowserThread::DeleteOnUIThread>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class CefPrintDialogCallbackImpl;
  friend class CefPrintJobCallbackImpl;

  explicit CefPrintDialogLinux(PrintingContextLinux* context);
  ~CefPrintDialogLinux() override;

  void SetHandler();
  void ReleaseHandler();

  bool UpdateSettings(std::unique_ptr<printing::PrintSettings> settings,
                      bool get_defaults);

  // Prints document named |document_name|.
  void SendDocumentToPrinter(const std::u16string& document_name);

  // Handles print dialog response.
  void OnPrintContinue(CefRefPtr<CefPrintSettings> settings);
  void OnPrintCancel();

  // Handles print job response.
  void OnJobCompleted();

  CefRefPtr<CefPrintHandler> handler_;

  // Printing dialog callback.
  PrintingContextLinux::PrintSettingsCallback callback_;
  PrintingContextLinux* context_;
  CefRefPtr<CefBrowserHostBase> browser_;

  base::FilePath path_to_pdf_;
};

#endif  // LIBCEF_BROWSER_PRINTING_PRINT_DIALOG_LINUX_H_
