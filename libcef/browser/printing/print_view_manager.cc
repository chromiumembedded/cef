// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/print_view_manager.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "components/printing/common/print_messages.h"
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
#include "printing/metafile_skia.h"

#include "libcef/browser/thread_util.h"

using content::BrowserThread;

namespace printing {

namespace {

const int PREVIEW_UIID = 12345678;

// Convert CefPdfPrintSettings into base::DictionaryValue.
void FillInDictionaryFromPdfPrintSettings(
    const CefPdfPrintSettings& pdf_settings,
    int request_id,
    base::DictionaryValue& print_settings) {
  // Fixed settings.
  print_settings.SetBoolean(kSettingPrintToPDF, true);
  print_settings.SetBoolean(kSettingCloudPrintDialog, false);
  print_settings.SetBoolean(kSettingPrintWithPrivet, false);
  print_settings.SetBoolean(kSettingPrintWithExtension, false);
  print_settings.SetInteger(kSettingColor, GRAY);
  print_settings.SetInteger(kSettingDuplexMode, SIMPLEX);
  print_settings.SetInteger(kSettingCopies, 1);
  print_settings.SetBoolean(kSettingCollate, false);
  print_settings.SetString(kSettingDeviceName, "");
  print_settings.SetBoolean(kSettingRasterizePdf, false);
  print_settings.SetBoolean(kSettingPreviewModifiable, false);
  print_settings.SetInteger(kSettingDpiHorizontal, 0);
  print_settings.SetInteger(kSettingDpiVertical, 0);
  print_settings.SetInteger(kSettingPagesPerSheet, 1);

  // User defined settings.
  print_settings.SetBoolean(kSettingLandscape, !!pdf_settings.landscape);
  print_settings.SetBoolean(kSettingShouldPrintSelectionOnly,
                            !!pdf_settings.selection_only);
  print_settings.SetBoolean(kSettingShouldPrintBackgrounds,
                            !!pdf_settings.backgrounds_enabled);
  print_settings.SetBoolean(kSettingHeaderFooterEnabled,
                            !!pdf_settings.header_footer_enabled);
  print_settings.SetInteger(kSettingScaleFactor, pdf_settings.scale_factor > 0
                                                     ? pdf_settings.scale_factor
                                                     : 100);

  if (pdf_settings.header_footer_enabled) {
    print_settings.SetString(
        kSettingHeaderFooterTitle,
        CefString(&pdf_settings.header_footer_title).ToString16());
    print_settings.SetString(
        kSettingHeaderFooterURL,
        CefString(&pdf_settings.header_footer_url).ToString16());
  }

  if (pdf_settings.page_width > 0 && pdf_settings.page_height > 0) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetInteger(kSettingMediaSizeWidthMicrons, pdf_settings.page_width);
    dict->SetInteger(kSettingMediaSizeHeightMicrons, pdf_settings.page_height);
    print_settings.Set(kSettingMediaSize, std::move(dict));
  }

  int margin_type = DEFAULT_MARGINS;
  switch (pdf_settings.margin_type) {
    case PDF_PRINT_MARGIN_NONE:
      margin_type = NO_MARGINS;
      break;
    case PDF_PRINT_MARGIN_MINIMUM:
      margin_type = PRINTABLE_AREA_MARGINS;
      break;
    case PDF_PRINT_MARGIN_CUSTOM:
      margin_type = CUSTOM_MARGINS;
      break;
    default:
      break;
  }

  print_settings.SetInteger(kSettingMarginsType, margin_type);
  if (margin_type == CUSTOM_MARGINS) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetDouble(kSettingMarginTop, pdf_settings.margin_top);
    dict->SetDouble(kSettingMarginRight, pdf_settings.margin_right);
    dict->SetDouble(kSettingMarginBottom, pdf_settings.margin_bottom);
    dict->SetDouble(kSettingMarginLeft, pdf_settings.margin_left);
    print_settings.Set(kSettingMarginsCustom, std::move(dict));
  }

  // Service settings.
  print_settings.SetInteger(kPreviewUIID, PREVIEW_UIID);
  print_settings.SetInteger(kPreviewRequestID, request_id);
  print_settings.SetBoolean(kIsFirstRequest, request_id != 0);
}

void StopWorker(int document_cookie) {
  if (document_cookie <= 0)
    return;
  scoped_refptr<PrintQueriesQueue> queue =
      g_browser_process->print_job_manager()->queue();
  std::unique_ptr<PrinterQuery> printer_query =
      queue->PopPrinterQuery(document_cookie);
  if (printer_query.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&PrinterQuery::StopWorker, std::move(printer_query)));
  }
}

// Write the PDF file to disk.
void SavePdfFile(scoped_refptr<base::RefCountedSharedMemoryMapping> data,
                 const base::FilePath& path,
                 const CefPrintViewManager::PdfPrintCallback& callback) {
  CEF_REQUIRE_BLOCKING();
  DCHECK_GT(data->size(), 0U);

  MetafileSkia metafile;
  metafile.InitFromData(static_cast<const void*>(data->front()), data->size());

  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  bool ok = file.IsValid() && metafile.SaveTo(&file);

  if (!callback.is_null()) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::Bind(callback, ok));
  }
}

}  // namespace

struct CefPrintViewManager::PdfPrintState {
  content::RenderFrameHost* printing_rfh_ = nullptr;
  base::FilePath output_path_;
  base::DictionaryValue settings_;
  PdfPrintCallback callback_;
};

CefPrintViewManager::CefPrintViewManager(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  PrintViewManager::CreateForWebContents(web_contents);
  PrintPreviewMessageHandler::CreateForWebContents(web_contents);
}

CefPrintViewManager::~CefPrintViewManager() {
  TerminatePdfPrintJob();
}

bool CefPrintViewManager::PrintToPDF(content::RenderFrameHost* rfh,
                                     const base::FilePath& path,
                                     const CefPdfPrintSettings& settings,
                                     const PdfPrintCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't start print again while printing is currently in progress.
  if (pdf_print_state_)
    return false;

  // Don't print interstitials or crashed tabs.
  if (!web_contents() || web_contents()->ShowingInterstitialPage() ||
      web_contents()->IsCrashed()) {
    return false;
  }

  pdf_print_state_.reset(new PdfPrintState);
  pdf_print_state_->printing_rfh_ = rfh;
  pdf_print_state_->output_path_ = path;
  pdf_print_state_->callback_ = callback;

  FillInDictionaryFromPdfPrintSettings(settings, ++next_pdf_request_id_,
                                       pdf_print_state_->settings_);

  rfh->Send(new PrintMsg_InitiatePrintPreview(rfh->GetRoutingID(),
                                              !!settings.selection_only));

  return true;
}

bool CefPrintViewManager::PrintPreviewNow(content::RenderFrameHost* rfh,
                                          bool has_selection) {
  return PrintViewManager::FromWebContents(web_contents())
      ->PrintPreviewNow(rfh, has_selection);
}

void CefPrintViewManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (pdf_print_state_ &&
      render_frame_host == pdf_print_state_->printing_rfh_) {
    TerminatePdfPrintJob();
  }
}

void CefPrintViewManager::NavigationStopped() {
  TerminatePdfPrintJob();
}

void CefPrintViewManager::RenderProcessGone(base::TerminationStatus status) {
  TerminatePdfPrintJob();
}

bool CefPrintViewManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  if (!pdf_print_state_) {
    IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(CefPrintViewManager, message,
                                     render_frame_host)
      IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                          OnRequestPrintPreview)
      IPC_MESSAGE_HANDLER(PrintHostMsg_ShowScriptedPrintPreview,
                          OnShowScriptedPrintPreview)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return false;
  }

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(CefPrintViewManager, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidShowPrintDialog,
                        OnDidShowPrintDialog_PrintToPdf)
    IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                        OnRequestPrintPreview_PrintToPdf)
    IPC_MESSAGE_HANDLER(PrintHostMsg_MetafileReadyForPrinting,
                        OnMetafileReadyForPrinting_PrintToPdf)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}
void CefPrintViewManager::OnRequestPrintPreview(
    content::RenderFrameHost* rfh,
    const PrintHostMsg_RequestPrintPreview_Params&) {
  InitializePrintPreview(rfh->GetFrameTreeNodeId());
}

void CefPrintViewManager::OnShowScriptedPrintPreview(
    content::RenderFrameHost* rfh,
    bool source_is_modifiable) {
  InitializePrintPreview(rfh->GetFrameTreeNodeId());
}

void CefPrintViewManager::OnDidShowPrintDialog_PrintToPdf(
    content::RenderFrameHost* rfh) {}

void CefPrintViewManager::OnRequestPrintPreview_PrintToPdf(
    content::RenderFrameHost* rfh,
    const PrintHostMsg_RequestPrintPreview_Params&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!pdf_print_state_)
    return;

  DCHECK_EQ(pdf_print_state_->printing_rfh_, rfh);

  rfh->Send(new PrintMsg_PrintPreview(rfh->GetRoutingID(),
                                      pdf_print_state_->settings_));
}

void CefPrintViewManager::OnMetafileReadyForPrinting_PrintToPdf(
    content::RenderFrameHost* rfh,
    const PrintHostMsg_DidPreviewDocument_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWorker(params.document_cookie);

  if (!pdf_print_state_)
    return;

  DCHECK_EQ(pdf_print_state_->printing_rfh_, rfh);

  rfh->Send(new PrintMsg_ClosePrintPreviewDialog(rfh->GetRoutingID()));

  auto shared_buf = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      params.content.metafile_data_region);
  if (!shared_buf) {
    TerminatePdfPrintJob();
    return;
  }

  const base::FilePath output_path = pdf_print_state_->output_path_;
  const PdfPrintCallback print_callback = pdf_print_state_->callback_;

  // Reset state information.
  pdf_print_state_.reset();

  // Save the PDF file to disk and then execute the callback.
  CEF_POST_USER_VISIBLE_TASK(
      base::Bind(&SavePdfFile, shared_buf, output_path, print_callback));
}

void CefPrintViewManager::TerminatePdfPrintJob() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!pdf_print_state_)
    return;

  if (!pdf_print_state_->callback_.is_null()) {
    // Execute the callback.
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::Bind(pdf_print_state_->callback_, false));
  }

  // Reset state information.
  pdf_print_state_.reset();
}

void CefPrintViewManager::InitializePrintPreview(int frame_tree_node_id) {
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return;

  dialog_controller->PrintPreview(web_contents());

  content::WebContents* preview_contents =
      dialog_controller->GetPrintPreviewForContents(web_contents());

  extensions::CefExtensionWebContentsObserver::CreateForWebContents(
      preview_contents);

  PrintPreviewHelper::CreateForWebContents(preview_contents);
  PrintPreviewHelper::FromWebContents(preview_contents)
      ->Initialize(frame_tree_node_id);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CefPrintViewManager)

// CefPrintViewManager::PrintPreviewHelper

CefPrintViewManager::PrintPreviewHelper::PrintPreviewHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

void CefPrintViewManager::PrintPreviewHelper::Initialize(
    int parent_frame_tree_node_id) {
  DCHECK_EQ(parent_frame_tree_node_id_,
            content::RenderFrameHost::kNoFrameTreeNodeId);
  DCHECK_NE(parent_frame_tree_node_id,
            content::RenderFrameHost::kNoFrameTreeNodeId);
  parent_frame_tree_node_id_ = parent_frame_tree_node_id;

  auto context = web_contents()->GetBrowserContext();
  auto manager = content::BrowserContext::GetDownloadManager(context);

  if (!context->GetDownloadManagerDelegate()) {
    manager->SetDelegate(new CefDownloadManagerDelegate(manager));
  }

  auto browser_info =
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoForFrameTreeNode(
          parent_frame_tree_node_id_);
  DCHECK(browser_info);
  if (!browser_info)
    return;

  // Associate guest state information with the owner browser.
  browser_info->MaybeCreateFrame(web_contents()->GetMainFrame(),
                                 true /* is_guest_view */);
}

void CefPrintViewManager::PrintPreviewHelper::WebContentsDestroyed() {
  auto browser_info =
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoForFrameTreeNode(
          parent_frame_tree_node_id_);
  DCHECK(browser_info);
  if (!browser_info)
    return;

  // Disassociate guest state information with the owner browser.
  browser_info->RemoveFrame(web_contents()->GetMainFrame());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CefPrintViewManager::PrintPreviewHelper)

}  // namespace printing
