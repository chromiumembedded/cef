// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/printing/print_view_manager.h"

#include <stdint.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "printing/pdf_metafile_skia.h"

using content::BrowserThread;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(printing::PrintViewManager);

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
  print_settings.SetBoolean(kSettingGenerateDraftData, false);
  print_settings.SetBoolean(kSettingPreviewModifiable, false);

  // User defined settings.
  print_settings.SetBoolean(kSettingLandscape, !!pdf_settings.landscape);
  print_settings.SetBoolean(kSettingShouldPrintSelectionOnly,
                            !!pdf_settings.selection_only);
  print_settings.SetBoolean(kSettingShouldPrintBackgrounds,
                            !!pdf_settings.backgrounds_enabled);
  print_settings.SetBoolean(kSettingHeaderFooterEnabled,
                            !!pdf_settings.header_footer_enabled);

  if (pdf_settings.header_footer_enabled) {
    print_settings.SetString(kSettingHeaderFooterTitle,
        CefString(&pdf_settings.header_footer_title).ToString16());
    print_settings.SetString(kSettingHeaderFooterURL,
        CefString(&pdf_settings.header_footer_url).ToString16());
  }

  if (pdf_settings.page_width > 0 && pdf_settings.page_height > 0) {
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
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
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
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
  scoped_refptr<PrinterQuery> printer_query =
      queue->PopPrinterQuery(document_cookie);
  if (printer_query.get()) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&PrinterQuery::StopWorker,
                                       printer_query));
  }
}

scoped_refptr<base::RefCountedBytes>
GetDataFromHandle(base::SharedMemoryHandle handle, uint32_t data_size) {
  scoped_ptr<base::SharedMemory> shared_buf(
      new base::SharedMemory(handle, true));

  if (!shared_buf->Map(data_size)) {
    NOTREACHED();
    return NULL;
  }

  unsigned char* data = static_cast<unsigned char*>(shared_buf->memory());
  std::vector<unsigned char> dataVector(data, data + data_size);
  return base::RefCountedBytes::TakeVector(&dataVector);
}

// Write the PDF file to disk.
void SavePdfFile(scoped_refptr<base::RefCountedBytes> data,
                 const base::FilePath& path,
                 const PrintViewManager::PdfPrintCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK_GT(data->size(), 0U);

  PdfMetafileSkia metafile;
  metafile.InitFromData(static_cast<const void*>(data->front()), data->size());

  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  bool ok = file.IsValid() && metafile.SaveTo(&file);

  if (!callback.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, ok));
  }
}

}  // namespace

PrintViewManager::PrintViewManager(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents) {
}

PrintViewManager::~PrintViewManager() {
  TerminatePdfPrintJob();
}

#if defined(ENABLE_BASIC_PRINTING)
bool PrintViewManager::PrintForSystemDialogNow() {
  return PrintNowInternal(new PrintMsg_PrintForSystemDialog(routing_id()));
}
#endif  // ENABLE_BASIC_PRINTING

bool PrintViewManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintViewManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidShowPrintDialog, OnDidShowPrintDialog)
    IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                        OnRequestPrintPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_MetafileReadyForPrinting,
                        OnMetafileReadyForPrinting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled ? true : PrintViewManagerBase::OnMessageReceived(message);
}

void PrintViewManager::NavigationStopped() {
  PrintViewManagerBase::NavigationStopped();
  TerminatePdfPrintJob();
}

void PrintViewManager::RenderProcessGone(base::TerminationStatus status) {
  PrintViewManagerBase::RenderProcessGone(status);
  TerminatePdfPrintJob();
}

void PrintViewManager::PrintToPDF(const base::FilePath& path,
                                  const CefPdfPrintSettings& settings,
                                  const PdfPrintCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents() || pdf_print_settings_)
    return;

  pdf_output_path_ = path;
  pdf_print_callback_ = callback;

  pdf_print_settings_.reset(new base::DictionaryValue);
  FillInDictionaryFromPdfPrintSettings(settings,
                                       ++next_pdf_request_id_,
                                       *pdf_print_settings_);

  Send(new PrintMsg_InitiatePrintPreview(routing_id(),
                                         !!settings.selection_only));
}

void PrintViewManager::OnDidShowPrintDialog() {
}

void PrintViewManager::OnRequestPrintPreview(
    const PrintHostMsg_RequestPrintPreview_Params&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents() || !pdf_print_settings_)
    return;

  Send(new PrintMsg_PrintPreview(routing_id(), *pdf_print_settings_));
}

void PrintViewManager::OnMetafileReadyForPrinting(
    const PrintHostMsg_DidPreviewDocument_Params& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWorker(params.document_cookie);

  scoped_refptr<base::RefCountedBytes> data_bytes =
      GetDataFromHandle(params.metafile_data_handle, params.data_size);
  if (!data_bytes || !data_bytes->size()) {
    TerminatePdfPrintJob();
    return;
  }

  base::FilePath pdf_output_path = pdf_output_path_;
  PdfPrintCallback pdf_print_callback = pdf_print_callback_;

  // Reset state information.
  pdf_output_path_.clear();
  pdf_print_callback_.Reset();
  pdf_print_settings_.reset();

  // Save the PDF file to disk and then execute the callback.
  BrowserThread::PostTask(BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SavePdfFile, data_bytes, pdf_output_path,
                 pdf_print_callback));
}

void PrintViewManager::TerminatePdfPrintJob() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!pdf_print_settings_.get())
    return;

  if (!pdf_print_callback_.is_null()) {
    // Execute the callback.
    BrowserThread::PostTask(BrowserThread::UI,
        FROM_HERE,
        base::Bind(pdf_print_callback_, false));
  }

  // Reset state information.
  pdf_output_path_.clear();
  pdf_print_callback_.Reset();
  pdf_print_settings_.reset();
}

}  // namespace printing
