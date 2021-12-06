// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "include/internal/cef_types_wrappers.h"

#include "base/callback_forward.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/mojom/print.mojom.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
class WebContentsObserver;
}  // namespace content

class CefBrowserInfo;

namespace printing {

// CEF handler for print commands.
class CefPrintViewManager : public PrintViewManager,
                            public mojom::PrintPreviewUI {
 public:
  CefPrintViewManager(const CefPrintViewManager&) = delete;
  CefPrintViewManager& operator=(const CefPrintViewManager&) = delete;

  ~CefPrintViewManager() override;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
      content::RenderFrameHost* rfh);

  // Callback executed on PDF printing completion.
  using PdfPrintCallback = base::OnceCallback<void(bool /*ok*/)>;

  // Print the current document to a PDF file. Execute |callback| on completion.
  bool PrintToPDF(content::RenderFrameHost* rfh,
                  const base::FilePath& path,
                  const CefPdfPrintSettings& settings,
                  PdfPrintCallback callback);

  // mojom::PrintManagerHost methods:
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
  void DidShowPrintDialog() override;
  void RequestPrintPreview(mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;

  // printing::mojo::PrintPreviewUI methods:
  void SetOptionsFromDocument(const mojom::OptionsFromDocumentParamsPtr params,
                              int32_t request_id) override {}
  void DidPrepareDocumentForPreview(int32_t document_cookie,
                                    int32_t request_id) override {}
  void DidPreviewPage(mojom::DidPreviewPageParamsPtr params,
                      int32_t request_id) override {}
  void MetafileReadyForPrinting(mojom::DidPreviewDocumentParamsPtr params,
                                int32_t request_id) override;
  void PrintPreviewFailed(int32_t document_cookie, int32_t request_id) override;
  void PrintPreviewCancelled(int32_t document_cookie,
                             int32_t request_id) override;
  void PrinterSettingsInvalid(int32_t document_cookie,
                              int32_t request_id) override {}
  void DidGetDefaultPageLayout(mojom::PageSizeMarginsPtr page_layout_in_points,
                               const gfx::Rect& printable_area_in_points,
                               bool has_custom_page_size_style,
                               int32_t request_id) override {}
  void DidStartPreview(mojom::DidStartPreviewParamsPtr params,
                       int32_t request_id) override {}

  // content::WebContentsObserver methods:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void NavigationStopped() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // Inline versions of the content::WebContentsUserData methods to avoid
  // ambiguous warnings due to the PrintViewManager base class also extending
  // WebContentsUserData.
  static void CreateForWebContents(content::WebContents* contents);
  static CefPrintViewManager* FromWebContents(content::WebContents* contents);
  static const CefPrintViewManager* FromWebContents(
      const content::WebContents* contents);

 private:
  explicit CefPrintViewManager(content::WebContents* web_contents);

  void TerminatePdfPrintJob();

  // Used for printing to PDF. Only accessed on the browser process UI thread.
  int next_pdf_request_id_ = content::RenderFrameHost::kNoFrameTreeNodeId;
  struct PdfPrintState;
  std::unique_ptr<PdfPrintState> pdf_print_state_;
  mojo::AssociatedReceiver<mojom::PrintPreviewUI> pdf_print_receiver_{this};
};

}  // namespace printing

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_