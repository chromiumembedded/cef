// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/print_view_manager.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/thread_util.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "components/printing/common/print.mojom.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/metafile_skia.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if BUILDFLAG(IS_LINUX)
#include "libcef/browser/printing/print_dialog_linux.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

const int PREVIEW_UIID = 12345678;

// Convert CefPdfPrintSettings into base::Value::Dict.
void FillInDictionaryFromPdfPrintSettings(
    const CefPdfPrintSettings& pdf_settings,
    int request_id,
    base::Value::Dict& print_settings) {
  // Fixed settings.
  print_settings.Set(kSettingPrinterType,
                     static_cast<int>(mojom::PrinterType::kPdf));
  print_settings.Set(kSettingColor, static_cast<int>(mojom::ColorModel::kGray));
  print_settings.Set(kSettingDuplexMode,
                     static_cast<int>(mojom::DuplexMode::kSimplex));
  print_settings.Set(kSettingCopies, 1);
  print_settings.Set(kSettingCollate, false);
  print_settings.Set(kSettingDeviceName, "");
  print_settings.Set(kSettingRasterizePdf, false);
  print_settings.Set(kSettingPreviewModifiable, false);
  print_settings.Set(kSettingDpiHorizontal, 0);
  print_settings.Set(kSettingDpiVertical, 0);
  print_settings.Set(kSettingPagesPerSheet, 1);

  // User defined settings.
  print_settings.Set(kSettingLandscape, !!pdf_settings.landscape);
  print_settings.Set(kSettingShouldPrintSelectionOnly,
                     !!pdf_settings.selection_only);
  print_settings.Set(kSettingShouldPrintBackgrounds,
                     !!pdf_settings.backgrounds_enabled);
  print_settings.Set(kSettingHeaderFooterEnabled,
                     !!pdf_settings.header_footer_enabled);
  print_settings.Set(kSettingScaleFactor, pdf_settings.scale_factor > 0
                                              ? pdf_settings.scale_factor
                                              : 100);

  if (pdf_settings.header_footer_enabled) {
    print_settings.Set(
        kSettingHeaderFooterTitle,
        CefString(&pdf_settings.header_footer_title).ToString16());
    print_settings.Set(kSettingHeaderFooterURL,
                       CefString(&pdf_settings.header_footer_url).ToString16());
  }

  if (pdf_settings.page_width > 0 && pdf_settings.page_height > 0) {
    base::Value::Dict dict;
    dict.Set(kSettingMediaSizeWidthMicrons, pdf_settings.page_width);
    dict.Set(kSettingMediaSizeHeightMicrons, pdf_settings.page_height);
    print_settings.Set(kSettingMediaSize, std::move(dict));
  }

  auto margin_type = printing::mojom::MarginType::kDefaultMargins;
  switch (pdf_settings.margin_type) {
    case PDF_PRINT_MARGIN_NONE:
      margin_type = printing::mojom::MarginType::kNoMargins;
      break;
    case PDF_PRINT_MARGIN_MINIMUM:
      margin_type = printing::mojom::MarginType::kPrintableAreaMargins;
      break;
    case PDF_PRINT_MARGIN_CUSTOM:
      margin_type = printing::mojom::MarginType::kCustomMargins;
      break;
    default:
      break;
  }

  print_settings.Set(kSettingMarginsType, static_cast<int>(margin_type));
  if (margin_type == printing::mojom::MarginType::kCustomMargins) {
    base::Value::Dict dict;
    dict.Set(kSettingMarginTop, pdf_settings.margin_top);
    dict.Set(kSettingMarginRight, pdf_settings.margin_right);
    dict.Set(kSettingMarginBottom, pdf_settings.margin_bottom);
    dict.Set(kSettingMarginLeft, pdf_settings.margin_left);
    print_settings.Set(kSettingMarginsCustom, std::move(dict));
  }

  // Service settings.
  print_settings.Set(kPreviewUIID, PREVIEW_UIID);
  print_settings.Set(kPreviewRequestID, request_id);
  print_settings.Set(kIsFirstRequest, request_id != 0);
}

void StopWorker(int document_cookie) {
  if (document_cookie <= 0)
    return;
  scoped_refptr<PrintQueriesQueue> queue =
      g_browser_process->print_job_manager()->queue();
  std::unique_ptr<PrinterQuery> printer_query =
      queue->PopPrinterQuery(document_cookie);
  if (printer_query.get()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PrinterQuery::StopWorker, std::move(printer_query)));
  }
}

// Write the PDF file to disk.
void SavePdfFile(scoped_refptr<base::RefCountedSharedMemoryMapping> data,
                 const base::FilePath& path,
                 CefPrintViewManager::PdfPrintCallback callback) {
  CEF_REQUIRE_BLOCKING();
  DCHECK_GT(data->size(), 0U);

  MetafileSkia metafile;
  metafile.InitFromData(*data);

  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  bool ok = file.IsValid() && metafile.SaveTo(&file);

  if (!callback.is_null()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ok));
  }
}

}  // namespace

struct CefPrintViewManager::PdfPrintState {
  content::RenderFrameHost* printing_rfh_ = nullptr;
  base::FilePath output_path_;
  base::Value::Dict settings_;
  PdfPrintCallback callback_;
};

CefPrintViewManager::CefPrintViewManager(content::WebContents* web_contents)
    : PrintViewManager(web_contents) {}

CefPrintViewManager::~CefPrintViewManager() {
  TerminatePdfPrintJob();
}

// static
void CefPrintViewManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = CefPrintViewManager::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

bool CefPrintViewManager::PrintToPDF(content::RenderFrameHost* rfh,
                                     const base::FilePath& path,
                                     const CefPdfPrintSettings& settings,
                                     PdfPrintCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't start print again while printing is currently in progress.
  if (pdf_print_state_)
    return false;

  // Don't print crashed tabs.
  if (!web_contents() || web_contents()->IsCrashed() ||
      !rfh->IsRenderFrameLive()) {
    return false;
  }

  pdf_print_state_.reset(new PdfPrintState);
  pdf_print_state_->printing_rfh_ = rfh;
  pdf_print_state_->output_path_ = path;
  pdf_print_state_->callback_ = std::move(callback);

  FillInDictionaryFromPdfPrintSettings(settings, ++next_pdf_request_id_,
                                       pdf_print_state_->settings_);

  auto& print_render_frame = GetPrintRenderFrame(rfh);
  if (!pdf_print_receiver_.is_bound()) {
    print_render_frame->SetPrintPreviewUI(
        pdf_print_receiver_.BindNewEndpointAndPassRemote());
  }

  print_render_frame->InitiatePrintPreview({}, !!settings.selection_only);

  return true;
}

void CefPrintViewManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
#if BUILDFLAG(IS_LINUX)
  // Send notification to the client.
  auto browser = CefBrowserHostBase::GetBrowserForContents(web_contents());
  if (browser) {
    CefPrintDialogLinux::OnPrintStart(browser);
  }
#endif
  PrintViewManager::GetDefaultPrintSettings(std::move(callback));
}

void CefPrintViewManager::DidShowPrintDialog() {
  if (pdf_print_state_)
    return;
  PrintViewManager::DidShowPrintDialog();
}

void CefPrintViewManager::RequestPrintPreview(
    mojom::RequestPrintPreviewParamsPtr params) {
  if (!pdf_print_state_) {
    PrintViewManager::RequestPrintPreview(std::move(params));
    return;
  }

  GetPrintRenderFrame(pdf_print_state_->printing_rfh_)
      ->PrintPreview(pdf_print_state_->settings_.Clone());
}

void CefPrintViewManager::CheckForCancel(int32_t preview_ui_id,
                                         int32_t request_id,
                                         CheckForCancelCallback callback) {
  if (!pdf_print_state_) {
    return PrintViewManager::CheckForCancel(preview_ui_id, request_id,
                                            std::move(callback));
  }

  std::move(callback).Run(/*cancel=*/false);
}

void CefPrintViewManager::MetafileReadyForPrinting(
    mojom::DidPreviewDocumentParamsPtr params,
    int32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWorker(params->document_cookie);

  if (!pdf_print_state_)
    return;

  GetPrintRenderFrame(pdf_print_state_->printing_rfh_)
      ->OnPrintPreviewDialogClosed();

  auto shared_buf = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      params->content->metafile_data_region);
  if (!shared_buf) {
    TerminatePdfPrintJob();
    return;
  }

  const base::FilePath output_path = pdf_print_state_->output_path_;
  PdfPrintCallback print_callback = std::move(pdf_print_state_->callback_);

  // Reset state information.
  pdf_print_state_.reset();
  pdf_print_receiver_.reset();

  // Save the PDF file to disk and then execute the callback.
  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(
      &SavePdfFile, shared_buf, output_path, std::move(print_callback)));
}

void CefPrintViewManager::PrintPreviewFailed(int32_t document_cookie,
                                             int32_t request_id) {
  TerminatePdfPrintJob();
}

void CefPrintViewManager::PrintPreviewCancelled(int32_t document_cookie,
                                                int32_t request_id) {
  // Should never be canceled by CheckForCancel().
  NOTREACHED();
}

void CefPrintViewManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (pdf_print_state_ &&
      render_frame_host == pdf_print_state_->printing_rfh_) {
    TerminatePdfPrintJob();
  }
  PrintViewManager::RenderFrameDeleted(render_frame_host);
}

void CefPrintViewManager::NavigationStopped() {
  TerminatePdfPrintJob();
  PrintViewManager::NavigationStopped();
}

void CefPrintViewManager::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  TerminatePdfPrintJob();
  PrintViewManager::PrimaryMainFrameRenderProcessGone(status);
}

// static
void CefPrintViewManager::CreateForWebContents(content::WebContents* contents) {
  DCHECK(contents);
  if (!FromWebContents(contents)) {
    contents->SetUserData(PrintViewManager::UserDataKey(),
                          base::WrapUnique(new CefPrintViewManager(contents)));
  }
}

// static
CefPrintViewManager* CefPrintViewManager::FromWebContents(
    content::WebContents* contents) {
  DCHECK(contents);
  return static_cast<CefPrintViewManager*>(
      contents->GetUserData(PrintViewManager::UserDataKey()));
}

// static
const CefPrintViewManager* CefPrintViewManager::FromWebContents(
    const content::WebContents* contents) {
  DCHECK(contents);
  return static_cast<const CefPrintViewManager*>(
      contents->GetUserData(PrintViewManager::UserDataKey()));
}

void CefPrintViewManager::TerminatePdfPrintJob() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!pdf_print_state_)
    return;

  if (!pdf_print_state_->callback_.is_null()) {
    // Execute the callback.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pdf_print_state_->callback_), false));
  }

  // Reset state information.
  pdf_print_state_.reset();
  pdf_print_receiver_.reset();
}

}  // namespace printing
