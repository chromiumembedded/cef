// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win_printing_context.h"

#include <winspool.h>

#include "base/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "skia/ext/platform_device_win.h"

#include "printing/units.h"

using base::Time;

namespace {

// Retrieves the content of a GetPrinter call.
void GetPrinterHelper(HANDLE printer, int level, scoped_array<uint8>* buffer) {
  DWORD buf_size = 0;
  GetPrinter(printer, level, NULL, 0, &buf_size);
  if (buf_size) {
    buffer->reset(new uint8[buf_size]);
    memset(buffer->get(), 0, buf_size);
    if (!GetPrinter(printer, level, buffer->get(), buf_size, &buf_size)) {
      buffer->reset();
    }
  }
}

}  // namespace

namespace printing {

PrintingContext::PrintingContext()
    : hdc_(NULL),
#ifndef NDEBUG
      page_number_(-1),
#endif
      dialog_box_(NULL),
      dialog_box_dismissed_(false),
      abort_printing_(false),
      in_print_job_(false) {
}

PrintingContext::~PrintingContext() {
  ResetSettings();
}

PrintingContext::Result PrintingContext::AskUserForSettings(
    HWND window,
    int max_pages,
    bool has_selection) {
  DCHECK(window);
  DCHECK(!in_print_job_);
  dialog_box_dismissed_ = false;
  // Show the OS-dependent dialog box.
  // If the user press
  // - OK, the settings are reset and reinitialized with the new settings. OK is
  //   returned.
  // - Apply then Cancel, the settings are reset and reinitialized with the new
  //   settings. CANCEL is returned.
  // - Cancel, the settings are not changed, the previous setting, if it was
  //   initialized before, are kept. CANCEL is returned.
  // On failure, the settings are reset and FAILED is returned.
  PRINTDLGEX dialog_options = { sizeof(PRINTDLGEX) };
  dialog_options.hwndOwner = window;
  // Disable options we don't support currently.
  // TODO(maruel):  Reuse the previously loaded settings!
  dialog_options.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE  |
                         PD_NOCURRENTPAGE;
  if (!has_selection)
    dialog_options.Flags |= PD_NOSELECTION;

  PRINTPAGERANGE ranges[32];
  dialog_options.nStartPage = START_PAGE_GENERAL;
  if (max_pages) {
    // Default initialize to print all the pages.
    memset(ranges, 0, sizeof(ranges));
    ranges[0].nFromPage = 1;
    ranges[0].nToPage = max_pages;
    dialog_options.nPageRanges = 1;
    dialog_options.nMaxPageRanges = arraysize(ranges);
    dialog_options.nMinPage = 1;
    dialog_options.nMaxPage = max_pages;
    dialog_options.lpPageRanges = ranges;
  } else {
    // No need to bother, we don't know how many pages are available.
    dialog_options.Flags |= PD_NOPAGENUMS;
  }

  // Adjust the default dev mode for the printdlg settings.
  DEVMODE dev_mode;
  memset(&dev_mode,0,sizeof(dev_mode));
  dev_mode.dmSpecVersion = DM_SPECVERSION;
  dev_mode.dmSize = sizeof(DEVMODE);
  AdjustDevMode(dev_mode);

  dialog_options.hDevMode = GlobalAlloc(GMEM_MOVEABLE, sizeof(DEVMODE));
  DEVMODE* locked_dev_mode = reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
  memcpy(locked_dev_mode, &dev_mode, sizeof(DEVMODE));
  GlobalUnlock(dialog_options.hDevMode);
  {
    if (PrintDlgEx(&dialog_options) != S_OK) {
      ResetSettings();
      return FAILED;
    }
  }
  return ParseDialogResultEx(dialog_options);
}

PrintingContext::Result PrintingContext::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  PRINTDLG dialog_options = { sizeof(PRINTDLG) };
  dialog_options.Flags = PD_RETURNDC | PD_RETURNDEFAULT;
  if (PrintDlg(&dialog_options) == 0) {
    ResetSettings();
    return FAILED;
  }
  return ParseDialogResult(dialog_options);
}

void PrintingContext::AdjustDevMode(DEVMODE& dev_mode)
{
  dev_mode.dmFields |= DM_ORIENTATION;
  dev_mode.dmOrientation = (settings_.landscape) ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;

  dev_mode.dmFields |= DM_PAPERSIZE;
  switch(settings_.page_measurements.page_type) {
    case PT_LETTER:
      dev_mode.dmPaperSize = DMPAPER_LETTER;
      break;
    case PT_LEGAL:
      dev_mode.dmPaperSize = DMPAPER_LEGAL;
      break;
    case PT_EXECUTIVE:
      dev_mode.dmPaperSize = DMPAPER_EXECUTIVE;
      break;
    case PT_A3:
      dev_mode.dmPaperSize = DMPAPER_A3;
      break;
    case PT_A4:
      dev_mode.dmPaperSize = DMPAPER_A4;
      break;
    case PT_CUSTOM:
      {
        dev_mode.dmPaperSize = DMPAPER_USER;
        dev_mode.dmFields |= DM_PAPERLENGTH | DM_PAPERWIDTH;
        DCHECK_GT(settings_.page_measurements.page_length, 0);
        DCHECK_GT(settings_.page_measurements.page_width, 0);
        // Convert from desired_dpi to tenths of a mm.
        dev_mode.dmPaperLength = static_cast<short>(
                                 ConvertUnitDouble(abs(settings_.page_measurements.page_length), 
                                                   10.0 * settings_.desired_dpi, 
                                                   static_cast<double>(kHundrethsMMPerInch)) + 0.5);
        dev_mode.dmPaperWidth = static_cast<short>(
                                ConvertUnitDouble(abs(settings_.page_measurements.page_width), 
                                                  10.0 * settings_.desired_dpi, 
                                                  static_cast<double>(kHundrethsMMPerInch)) + 0.5);
        break;
      }
    default:
      //we shouldn't ever hit this case.
      DCHECK(false);
      dev_mode.dmPaperSize = DMPAPER_LETTER;
      break;
  }
}

PrintingContext::Result PrintingContext::Init() {
  DCHECK(!in_print_job_);
  TCHAR printername[512];
  DWORD size = sizeof(printername)-1;
  if(GetDefaultPrinter(printername, &size)) {
    return Init(CefString(printername), false);
  }
  return FAILED;
}

PrintingContext::Result PrintingContext::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);
  settings_ = settings;

  return Init(settings_.device_name(), true);
}

PrintingContext::Result PrintingContext::Init(const CefString& device_name, 
                                              bool adjust_dev_mode) {
  HANDLE printer;
  std::wstring deviceNameStr = device_name;
  if (!OpenPrinter(const_cast<wchar_t*>(deviceNameStr.c_str()),
                   &printer,
                   NULL))
    return FAILED;

  Result status = OK;

  if (!GetPrinterSettings(printer, device_name, adjust_dev_mode))
    status = FAILED;

  // Close the printer after retrieving the context.
  ClosePrinter(printer);

  if (status != OK)
    ResetSettings();
  return status;
}

void PrintingContext::ResetSettings() {
  if (hdc_ != NULL) {
    DeleteDC(hdc_);
    hdc_ = NULL;
  }
  settings_.Clear();
  in_print_job_ = false;

#ifndef NDEBUG
  page_number_ = -1;
#endif
}

PrintingContext::Result PrintingContext::NewDocument(
    const CefString& document_name) {
  DCHECK(!in_print_job_);
  if (!hdc_)
    return OnError();

  // Set the flag used by the AbortPrintJob dialog procedure.
  abort_printing_ = false;

  in_print_job_ = true;

  // Register the application's AbortProc function with GDI.
  if (SP_ERROR == SetAbortProc(hdc_, &AbortProc))
    return OnError();

  DOCINFO di = { sizeof(DOCINFO) };
  std::wstring documentNameStr = document_name;
  di.lpszDocName = documentNameStr.c_str();

  wchar_t szFileName[MAX_PATH] = L"";
  if (settings_.to_file) {
    // Prompt for the file name to use for the printed output.
    OPENFILENAME ofn = { sizeof(ofn) };

    ofn.lpstrFilter = L"PostScript Files (*.ps)\0*.ps\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY |
                OFN_NOREADONLYRETURN | OFN_ENABLESIZING;
    ofn.lpstrDefExt = L"ps";

    if(GetSaveFileName(&ofn))
      di.lpszOutput = szFileName;
    else
      return OnError();
  }

  DCHECK_EQ(MessageLoop::current()->NestableTasksAllowed(), false);
  // Begin a print job by calling the StartDoc function.
  // NOTE: StartDoc() starts a message loop. That causes a lot of problems with
  // IPC. Make sure recursive task processing is disabled.
  if (StartDoc(hdc_, &di) <= 0)
    return OnError();

#ifndef NDEBUG
  page_number_ = 0;
#endif
  return OK;
}

PrintingContext::Result PrintingContext::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Inform the driver that the application is about to begin sending data.
  if (StartPage(hdc_) <= 0)
    return OnError();

#ifndef NDEBUG
  ++page_number_;
#endif

  return OK;
}

PrintingContext::Result PrintingContext::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  if (EndPage(hdc_) <= 0)
    return OnError();
  return OK;
}

PrintingContext::Result PrintingContext::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Inform the driver that document has ended.
  if (EndDoc(hdc_) <= 0)
    return OnError();

  ResetSettings();
  return OK;
}

void PrintingContext::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  if (hdc_)
    CancelDC(hdc_);
  DismissDialog();
}

void PrintingContext::DismissDialog() {
  if (dialog_box_) {
    DestroyWindow(dialog_box_);
    dialog_box_dismissed_ = true;
  }
}

PrintingContext::Result PrintingContext::OnError() {
  // This will close hdc_ and clear settings_.
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

// static
BOOL PrintingContext::AbortProc(HDC hdc, int nCode) {
  if (nCode) {
    // TODO(maruel):  Need a way to find the right instance to set. Should
    // leverage PrintJobManager here?
    // abort_printing_ = true;
  }
  return true;
}

bool PrintingContext::InitializeSettings(const DEVMODE& dev_mode,
                                         const CefString& new_device_name,
                                         const PRINTPAGERANGE* ranges,
                                         int number_ranges,
                                         bool selection_only,
                                         bool to_file) {
  skia::InitializeDC(hdc_);
  DCHECK(GetDeviceCaps(hdc_, CLIPCAPS));
  DCHECK(GetDeviceCaps(hdc_, RASTERCAPS) & RC_STRETCHDIB);
  DCHECK(GetDeviceCaps(hdc_, RASTERCAPS) & RC_BITMAP64);
  // Some printers don't advertise these.
  // DCHECK(GetDeviceCaps(hdc_, RASTERCAPS) & RC_SCALING);
  // DCHECK(GetDeviceCaps(hdc_, SHADEBLENDCAPS) & SB_CONST_ALPHA);
  // DCHECK(GetDeviceCaps(hdc_, SHADEBLENDCAPS) & SB_PIXEL_ALPHA);

  // StretchDIBits() support is needed for printing.
  if (!(GetDeviceCaps(hdc_, RASTERCAPS) & RC_STRETCHDIB) ||
      !(GetDeviceCaps(hdc_, RASTERCAPS) & RC_BITMAP64)) {
    NOTREACHED();
    ResetSettings();
    return false;
  }

  DCHECK(!in_print_job_);
  DCHECK(hdc_);
  PageRanges ranges_vector;
  if (!selection_only) {
    // Convert the PRINTPAGERANGE array to a PrintSettings::PageRanges vector.
    ranges_vector.reserve(number_ranges);
    for (int i = 0; i < number_ranges; ++i) {
      PageRange range;
      // Transfer from 1-based to 0-based.
      range.from = ranges[i].nFromPage - 1;
      range.to = ranges[i].nToPage - 1;
      ranges_vector.push_back(range);
    }
  }
  settings_.Init(hdc_,
                 dev_mode,
                 ranges_vector,
                 new_device_name,
                 selection_only,
                 to_file);
  return true;
}

bool PrintingContext::GetPrinterSettings(HANDLE printer,
                                         const CefString& device_name,
                                         bool adjust_dev_mode) {
  DCHECK(!in_print_job_);
  scoped_array<uint8> buffer;

  // A PRINTER_INFO_9 structure specifying the per-user default printer
  // settings.
  GetPrinterHelper(printer, 9, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_9* info_9 = reinterpret_cast<PRINTER_INFO_9*>(buffer.get());
    if (info_9->pDevMode != NULL) {
      if (adjust_dev_mode)
        AdjustDevMode(*info_9->pDevMode);
      if (!AllocateContext(device_name, info_9->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_9->pDevMode, device_name, NULL, 0, false,
          false);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_8 structure specifying the global default printer settings.
  GetPrinterHelper(printer, 8, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_8* info_8 = reinterpret_cast<PRINTER_INFO_8*>(buffer.get());
    if (info_8->pDevMode != NULL) {
      if (adjust_dev_mode)
        AdjustDevMode(*info_8->pDevMode);
      if (!AllocateContext(device_name, info_8->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_8->pDevMode, device_name, NULL, 0, false,
          false);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_2 structure specifying the driver's default printer
  // settings.
  GetPrinterHelper(printer, 2, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_2* info_2 = reinterpret_cast<PRINTER_INFO_2*>(buffer.get());
    if (info_2->pDevMode != NULL) {
      if (adjust_dev_mode)
        AdjustDevMode(*info_2->pDevMode);
      if (!AllocateContext(device_name, info_2->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_2->pDevMode, device_name, NULL, 0, false,
          false);
    }
    buffer.reset();
  }
  // Failed to retrieve the printer settings.
  ResetSettings();
  return false;
}

bool PrintingContext::AllocateContext(const CefString& printer_name,
                                      const DEVMODE* dev_mode) {
  std::wstring printerNameStr = printer_name;
  hdc_ = CreateDC(L"WINSPOOL", printerNameStr.c_str(), NULL, dev_mode);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

PrintingContext::Result PrintingContext::ParseDialogResultEx(
    const PRINTDLGEX& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  if (dialog_options.dwResultAction != PD_RESULT_CANCEL) {
    PageMargins requested_margins = settings_.requested_margins;
    // Start fresh except for page margins since that isn't controlled by this dialog.
    ResetSettings();
    settings_.requested_margins = requested_margins;

    DEVMODE* dev_mode = NULL;
    if (dialog_options.hDevMode) {
      dev_mode =
          reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
      DCHECK(dev_mode);
    }

    std::wstring device_name;
    if (dialog_options.hDevNames) {
      DEVNAMES* dev_names =
          reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
      DCHECK(dev_names);
      if (dev_names) {
        device_name =
            reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const wchar_t*>(dev_names) +
                    dev_names->wDeviceOffset);
        GlobalUnlock(dialog_options.hDevNames);
      }
    }

    bool success = false;
    if (dev_mode && !device_name.empty()) {
      hdc_ = dialog_options.hDC;
      PRINTPAGERANGE* page_ranges = NULL;
      DWORD num_page_ranges = 0;
      bool print_selection_only = false;
      bool print_to_file = false;
      if (dialog_options.Flags & PD_PAGENUMS) {
        page_ranges = dialog_options.lpPageRanges;
        num_page_ranges = dialog_options.nPageRanges;
      }
      if (dialog_options.Flags & PD_SELECTION) {
        print_selection_only = true;
      }
      if (dialog_options.Flags & PD_PRINTTOFILE) {
        print_to_file = true;
      }
      success = InitializeSettings(*dev_mode,
                                   device_name,
                                   page_ranges,
                                   num_page_ranges,
                                   print_selection_only,
                                   print_to_file);
    }

    if (!success && dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
      hdc_ = NULL;
    }

    if (dev_mode) {
      GlobalUnlock(dialog_options.hDevMode);
    }
  } else {
    if (dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
    }
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  switch (dialog_options.dwResultAction) {
    case PD_RESULT_PRINT:
      return hdc_ ? OK : FAILED;
    case PD_RESULT_APPLY:
      return hdc_ ? CANCEL : FAILED;
    case PD_RESULT_CANCEL:
      return CANCEL;
    default:
      return FAILED;
  }
}

PrintingContext::Result PrintingContext::ParseDialogResult(
    const PRINTDLG& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  // Start fresh.
  ResetSettings();

  DEVMODE* dev_mode = NULL;
  if (dialog_options.hDevMode) {
    dev_mode =
        reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
    DCHECK(dev_mode);
  }

  std::wstring device_name;
  if (dialog_options.hDevNames) {
    DEVNAMES* dev_names =
        reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
    DCHECK(dev_names);
    if (dev_names) {
      device_name =
          reinterpret_cast<const wchar_t*>(
              reinterpret_cast<const wchar_t*>(dev_names) +
                  dev_names->wDeviceOffset);
      GlobalUnlock(dialog_options.hDevNames);
    }
  }

  bool success = false;
  if (dev_mode && !device_name.empty()) {
    hdc_ = dialog_options.hDC;
    success = InitializeSettings(*dev_mode, device_name, NULL, 0, false,
        false);
  }

  if (!success && dialog_options.hDC) {
    DeleteDC(dialog_options.hDC);
    hdc_ = NULL;
  }

  if (dev_mode) {
    GlobalUnlock(dialog_options.hDevMode);
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  return hdc_ ? OK : FAILED;
}

}  // namespace printing

