// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "win_printing_context.h"

#include <winspool.h>

#include "base/file_util.h"
#include "base/gfx/platform_canvas.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/time_format.h"

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

PrintingContext::Result PrintingContext::AskUserForSettings(HWND window,
                                                            int max_pages) {
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
  // Disables the Current Page and Selection radio buttons since WebKit can't
  // print a part of the webpage and we don't know which page is the current
  // one.
  // TODO(maruel):  Reuse the previously loaded settings!
  dialog_options.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE  |
                         PD_NOSELECTION | PD_NOCURRENTPAGE | PD_HIDEPRINTTOFILE;
  PRINTPAGERANGE ranges[32];
  dialog_options.nStartPage = START_PAGE_GENERAL;
  if (max_pages) {
    // Default initialize to print all the pages.
    memset(ranges, 0, sizeof(ranges));
    ranges[0].nFromPage = 1;
    ranges[0].nToPage = max_pages;
    dialog_options.nPageRanges = 1;
    dialog_options.nMaxPageRanges = arraysize(ranges);
    dialog_options.nMaxPage = max_pages;
    dialog_options.lpPageRanges = ranges;
  } else {
    // No need to bother, we don't know how many pages are available.
    dialog_options.Flags |= PD_NOPAGENUMS;
  }

  {
    if (PrintDlgEx(&dialog_options) != S_OK) {
      ResetSettings();
      return FAILED;
    }
  }
  // TODO(maruel):  Support PD_PRINTTOFILE.
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

PrintingContext::Result PrintingContext::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);
  settings_ = settings;
  // TODO(maruel): settings_->ToDEVMODE()
  HANDLE printer;
  if (!OpenPrinter(const_cast<wchar_t*>(settings_.device_name().c_str()),
                   &printer,
                   NULL))
    return FAILED;

  Result status = OK;

  if (!GetPrinterSettings(printer, settings_.device_name()))
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
    const std::wstring& document_name) {
  DCHECK(!in_print_job_);
  if (!hdc_)
    return OnErrror();

  // Set the flag used by the AbortPrintJob dialog procedure.
  abort_printing_ = false;

  in_print_job_ = true;

  // Register the application's AbortProc function with GDI.
  if (SP_ERROR == SetAbortProc(hdc_, &AbortProc))
    return OnErrror();

  DOCINFO di = { sizeof(DOCINFO) };
  di.lpszDocName = document_name.c_str();

  DCHECK_EQ(MessageLoop::current()->NestableTasksAllowed(), false);
  // Begin a print job by calling the StartDoc function.
  // NOTE: StartDoc() starts a message loop. That causes a lot of problems with
  // IPC. Make sure recursive task processing is disabled.
  if (StartDoc(hdc_, &di) <= 0)
    return OnErrror();

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
    return OnErrror();

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
    return OnErrror();
  return OK;
}

PrintingContext::Result PrintingContext::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Inform the driver that document has ended.
  if (EndDoc(hdc_) <= 0)
    return OnErrror();

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

PrintingContext::Result PrintingContext::OnErrror() {
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
                                         const std::wstring& new_device_name,
                                         const PRINTPAGERANGE* ranges,
                                         int number_ranges) {
  gfx::PlatformDeviceWin::InitializeDC(hdc_);
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
  // Convert the PRINTPAGERANGE array to a PrintSettings::PageRanges vector.
  PageRanges ranges_vector;
  ranges_vector.reserve(number_ranges);
  for (int i = 0; i < number_ranges; ++i) {
    PageRange range;
    // Transfert from 1-based to 0-based.
    range.from = ranges[i].nFromPage - 1;
    range.to = ranges[i].nToPage - 1;
    ranges_vector.push_back(range);
  }
  settings_.Init(hdc_, dev_mode, ranges_vector, new_device_name);
  PageMargins margins;
  margins.header = 500;
  margins.footer = 500;
  margins.left = 500;
  margins.top = 500;
  margins.right = 500;
  margins.bottom = 500;
  settings_.UpdateMarginsMilliInch(margins);
  return true;
}

bool PrintingContext::GetPrinterSettings(HANDLE printer,
                                         const std::wstring& device_name) {
  DCHECK(!in_print_job_);
  scoped_array<uint8> buffer;

  // A PRINTER_INFO_9 structure specifying the per-user default printer
  // settings.
  GetPrinterHelper(printer, 9, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_9* info_9 = reinterpret_cast<PRINTER_INFO_9*>(buffer.get());
    if (info_9->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_9->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_9->pDevMode, device_name, NULL, 0);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_8 structure specifying the global default printer settings.
  GetPrinterHelper(printer, 8, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_8* info_8 = reinterpret_cast<PRINTER_INFO_8*>(buffer.get());
    if (info_8->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_8->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_8->pDevMode, device_name, NULL, 0);
    }
    buffer.reset();
  }

  // A PRINTER_INFO_2 structure specifying the driver's default printer
  // settings.
  GetPrinterHelper(printer, 2, &buffer);
  if (buffer.get()) {
    PRINTER_INFO_2* info_2 = reinterpret_cast<PRINTER_INFO_2*>(buffer.get());
    if (info_2->pDevMode != NULL) {
      if (!AllocateContext(device_name, info_2->pDevMode)) {
        ResetSettings();
        return false;
      }
      return InitializeSettings(*info_2->pDevMode, device_name, NULL, 0);
    }
    buffer.reset();
  }
  // Failed to retrieve the printer settings.
  ResetSettings();
  return false;
}

bool PrintingContext::AllocateContext(const std::wstring& printer_name,
                                      const DEVMODE* dev_mode) {
  hdc_ = CreateDC(L"WINSPOOL", printer_name.c_str(), NULL, dev_mode);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

PrintingContext::Result PrintingContext::ParseDialogResultEx(
    const PRINTDLGEX& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  if (dialog_options.dwResultAction != PD_RESULT_CANCEL) {
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
      if (dialog_options.Flags & PD_PAGENUMS) {
        success = InitializeSettings(*dev_mode,
                                     device_name,
                                     dialog_options.lpPageRanges,
                                     dialog_options.nPageRanges);
      } else {
        success = InitializeSettings(*dev_mode, device_name, NULL, 0);
      }
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
    success = InitializeSettings(*dev_mode, device_name, NULL, 0);
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

