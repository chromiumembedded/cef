// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "libcef/browser/printing/print_view_manager_base.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderProcessHost;
}

namespace printing {

// Manages the print commands for a WebContents.
class PrintViewManager : public PrintViewManagerBase,
                         public content::WebContentsUserData<PrintViewManager> {
 public:
  virtual ~PrintViewManager();

  // Same as PrintNow(), but for the case where a user prints with the system
  // dialog from print preview.
  bool PrintForSystemDialogNow();

  // Same as PrintNow(), but for the case where we want to send the result to
  // another destination.
  // TODO(mad) Add an argument so we can pass the destination interface.
  bool PrintToDestination();

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::WebContentsObserver implementation.
  // Terminates or cancels the print job if one was pending.
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;

 private:
  explicit PrintViewManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrintViewManager>;

  // IPC Message handlers.
  void OnDidShowPrintDialog();

  DISALLOW_COPY_AND_ASSIGN(PrintViewManager);
};

}  // namespace printing

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
