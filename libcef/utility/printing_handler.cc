// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/printing_handler.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "content/public/utility/utility_thread.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"

#if defined(OS_WIN)
#include "base/win/iat_patch_function.h"
#include "printing/emf_win.h"
#include "ui/gfx/gdi_util.h"
#endif

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

class PdfFunctionsBase {
 public:
  PdfFunctionsBase() : render_pdf_to_bitmap_func_(NULL),
                       get_pdf_doc_info_func_(NULL) {}

  bool Init() {
    base::FilePath pdf_module_path;
    if (!PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_module_path) ||
        !base::PathExists(pdf_module_path)) {
      return false;
    }

    pdf_lib_.Reset(base::LoadNativeLibrary(pdf_module_path, NULL));
    if (!pdf_lib_.is_valid()) {
      LOG(WARNING) << "Couldn't load PDF plugin";
      return false;
    }

    render_pdf_to_bitmap_func_ =
        reinterpret_cast<RenderPDFPageToBitmapProc>(
            pdf_lib_.GetFunctionPointer("RenderPDFPageToBitmap"));
    LOG_IF(WARNING, !render_pdf_to_bitmap_func_) <<
        "Missing RenderPDFPageToBitmap";

    get_pdf_doc_info_func_ =
        reinterpret_cast<GetPDFDocInfoProc>(
            pdf_lib_.GetFunctionPointer("GetPDFDocInfo"));
    LOG_IF(WARNING, !get_pdf_doc_info_func_) << "Missing GetPDFDocInfo";

    if (!render_pdf_to_bitmap_func_ || !get_pdf_doc_info_func_ ||
        !PlatformInit(pdf_module_path, pdf_lib_)) {
      Reset();
    }

    return IsValid();
  }

  bool IsValid() const {
    return pdf_lib_.is_valid();
  }

  void Reset() {
    pdf_lib_.Reset(NULL);
  }

  bool RenderPDFPageToBitmap(const void* pdf_buffer,
                             int pdf_buffer_size,
                             int page_number,
                             void* bitmap_buffer,
                             int bitmap_width,
                             int bitmap_height,
                             int dpi_x,
                             int dpi_y,
                             bool autorotate) {
    if (!render_pdf_to_bitmap_func_)
      return false;
    return render_pdf_to_bitmap_func_(pdf_buffer, pdf_buffer_size, page_number,
                                      bitmap_buffer, bitmap_width,
                                      bitmap_height, dpi_x, dpi_y, autorotate);
  }

  bool GetPDFDocInfo(const void* pdf_buffer,
                     int buffer_size,
                     int* page_count,
                     double* max_page_width) {
    if (!get_pdf_doc_info_func_)
      return false;
    return get_pdf_doc_info_func_(pdf_buffer, buffer_size, page_count,
                                  max_page_width);
  }

 protected:
  virtual bool PlatformInit(
      const base::FilePath& pdf_module_path,
      const base::ScopedNativeLibrary& pdf_lib) {
    return true;
  }

 private:
  // Exported by PDF plugin.
  typedef bool (*RenderPDFPageToBitmapProc)(const void* pdf_buffer,
                                            int pdf_buffer_size,
                                            int page_number,
                                            void* bitmap_buffer,
                                            int bitmap_width,
                                            int bitmap_height,
                                            int dpi_x,
                                            int dpi_y,
                                            bool autorotate);
  typedef bool (*GetPDFDocInfoProc)(const void* pdf_buffer,
                                    int buffer_size, int* page_count,
                                    double* max_page_width);

  RenderPDFPageToBitmapProc render_pdf_to_bitmap_func_;
  GetPDFDocInfoProc get_pdf_doc_info_func_;

  base::ScopedNativeLibrary pdf_lib_;
  DISALLOW_COPY_AND_ASSIGN(PdfFunctionsBase);
};

#if defined(OS_WIN)
// The 2 below IAT patch functions are almost identical to the code in
// render_process_impl.cc. This is needed to work around specific Windows APIs
// used by the Chrome PDF plugin that will fail in the sandbox.
static base::win::IATPatchFunction g_iat_patch_createdca;
HDC WINAPI UtilityProcess_CreateDCAPatch(LPCSTR driver_name,
                                         LPCSTR device_name,
                                         LPCSTR output,
                                         const DEVMODEA* init_data) {
  if (driver_name && (std::string("DISPLAY") == driver_name)) {
    // CreateDC fails behind the sandbox, but not CreateCompatibleDC.
    return CreateCompatibleDC(NULL);
  }

  NOTREACHED();
  return CreateDCA(driver_name, device_name, output, init_data);
}

static base::win::IATPatchFunction g_iat_patch_get_font_data;
DWORD WINAPI UtilityProcess_GetFontDataPatch(
    HDC hdc, DWORD table, DWORD offset, LPVOID buffer, DWORD length) {
  int rv = GetFontData(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      content::UtilityThread::Get()->PreCacheFont(logfont);
      rv = GetFontData(hdc, table, offset, buffer, length);
      content::UtilityThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}

class PdfFunctionsWin : public PdfFunctionsBase {
 public:
  PdfFunctionsWin() : render_pdf_to_dc_func_(NULL) {
  }

  bool PlatformInit(
      const base::FilePath& pdf_module_path,
      const base::ScopedNativeLibrary& pdf_lib) override {
    // Patch the IAT for handling specific APIs known to fail in the sandbox.
    if (!g_iat_patch_createdca.is_patched()) {
      g_iat_patch_createdca.Patch(pdf_module_path.value().c_str(),
                                  "gdi32.dll", "CreateDCA",
                                  UtilityProcess_CreateDCAPatch);
    }

    if (!g_iat_patch_get_font_data.is_patched()) {
      g_iat_patch_get_font_data.Patch(pdf_module_path.value().c_str(),
                                      "gdi32.dll", "GetFontData",
                                      UtilityProcess_GetFontDataPatch);
    }
    render_pdf_to_dc_func_ =
      reinterpret_cast<RenderPDFPageToDCProc>(
          pdf_lib.GetFunctionPointer("RenderPDFPageToDC"));
    LOG_IF(WARNING, !render_pdf_to_dc_func_) << "Missing RenderPDFPageToDC";

    return render_pdf_to_dc_func_ != NULL;
  }

  bool RenderPDFPageToDC(const void* pdf_buffer,
                         int buffer_size,
                         int page_number,
                         HDC dc,
                         int dpi_x,
                         int dpi_y,
                         int bounds_origin_x,
                         int bounds_origin_y,
                         int bounds_width,
                         int bounds_height,
                         bool fit_to_bounds,
                         bool stretch_to_bounds,
                         bool keep_aspect_ratio,
                         bool center_in_bounds,
                         bool autorotate) {
    if (!render_pdf_to_dc_func_)
      return false;
    return render_pdf_to_dc_func_(pdf_buffer, buffer_size, page_number,
                                  dc, dpi_x, dpi_y, bounds_origin_x,
                                  bounds_origin_y, bounds_width, bounds_height,
                                  fit_to_bounds, stretch_to_bounds,
                                  keep_aspect_ratio, center_in_bounds,
                                  autorotate);
  }

 private:
  // Exported by PDF plugin.
  typedef bool (*RenderPDFPageToDCProc)(
      const void* pdf_buffer, int buffer_size, int page_number, HDC dc,
      int dpi_x, int dpi_y, int bounds_origin_x, int bounds_origin_y,
      int bounds_width, int bounds_height, bool fit_to_bounds,
      bool stretch_to_bounds, bool keep_aspect_ratio, bool center_in_bounds,
      bool autorotate);
  RenderPDFPageToDCProc render_pdf_to_dc_func_;

  DISALLOW_COPY_AND_ASSIGN(PdfFunctionsWin);
};

typedef PdfFunctionsWin PdfFunctions;
#else  // OS_WIN
typedef PdfFunctionsBase PdfFunctions;
#endif  // OS_WIN

base::LazyInstance<PdfFunctions> g_pdf_lib = LAZY_INSTANCE_INITIALIZER;

}  // namespace

PrintingHandler::PrintingHandler() {}

PrintingHandler::~PrintingHandler() {}

// static
void PrintingHandler::PreSandboxStartup() {
  g_pdf_lib.Get().Init();
}

bool PrintingHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintingHandler, message)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles,
                        OnRenderPDFPagesToMetafile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles_GetPage,
                        OnRenderPDFPagesToMetafileGetPage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles_Stop,
                        OnRenderPDFPagesToMetafileStop)
#endif  // OS_WIN
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if defined(OS_WIN)
void PrintingHandler::OnRenderPDFPagesToMetafile(
    IPC::PlatformFileForTransit pdf_transit,
    const printing::PdfRenderSettings& settings) {
  pdf_rendering_settings_ = settings;
  base::File pdf_file = IPC::PlatformFileForTransitToFile(pdf_transit);
  int page_count = LoadPDF(pdf_file.Pass());
  Send(
      new ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_PageCount(page_count));
}

void PrintingHandler::OnRenderPDFPagesToMetafileGetPage(
    int page_number,
    IPC::PlatformFileForTransit output_file) {
  base::File emf_file = IPC::PlatformFileForTransitToFile(output_file);
  double scale_factor = 1.0;
  bool success =
      RenderPdfPageToMetafile(page_number, emf_file.Pass(), &scale_factor);
  Send(new ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_PageDone(
      success, scale_factor));
}

void PrintingHandler::OnRenderPDFPagesToMetafileStop() {
  ReleaseProcessIfNeeded();
}

int PrintingHandler::LoadPDF(base::File pdf_file) {
  if (!g_pdf_lib.Get().IsValid())
    return 0;

  int64 length = pdf_file.GetLength();
  if (length < 0)
    return 0;

  pdf_data_.resize(length);
  if (length != pdf_file.Read(0, pdf_data_.data(), pdf_data_.size()))
    return 0;

  int total_page_count = 0;
  if (!g_pdf_lib.Get().GetPDFDocInfo(
          &pdf_data_.front(), pdf_data_.size(), &total_page_count, NULL)) {
    return 0;
  }
  return total_page_count;
}

bool PrintingHandler::RenderPdfPageToMetafile(int page_number,
                                              base::File output_file,
                                              double* scale_factor) {
  printing::Emf metafile;
  metafile.Init();

  // We need to scale down DC to fit an entire page into DC available area.
  // Current metafile is based on screen DC and have current screen size.
  // Writing outside of those boundaries will result in the cut-off output.
  // On metafiles (this is the case here), scaling down will still record
  // original coordinates and we'll be able to print in full resolution.
  // Before playback we'll need to counter the scaling up that will happen
  // in the service (print_system_win.cc).
  *scale_factor =
      gfx::CalculatePageScale(metafile.context(),
                              pdf_rendering_settings_.area().right(),
                              pdf_rendering_settings_.area().bottom());
  gfx::ScaleDC(metafile.context(), *scale_factor);

  // The underlying metafile is of type Emf and ignores the arguments passed
  // to StartPage.
  metafile.StartPage(gfx::Size(), gfx::Rect(), 1);
  if (!g_pdf_lib.Get().RenderPDFPageToDC(
          &pdf_data_.front(),
          pdf_data_.size(),
          page_number,
          metafile.context(),
          pdf_rendering_settings_.dpi(),
          pdf_rendering_settings_.dpi(),
          pdf_rendering_settings_.area().x(),
          pdf_rendering_settings_.area().y(),
          pdf_rendering_settings_.area().width(),
          pdf_rendering_settings_.area().height(),
          true,
          false,
          true,
          true,
          pdf_rendering_settings_.autorotate())) {
    return false;
  }
  metafile.FinishPage();
  metafile.FinishDocument();
  return metafile.SaveTo(&output_file);
}

#endif  // OS_WIN

